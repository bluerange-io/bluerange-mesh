////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2022 M-Way Solutions GmbH
// ** Contact: https://www.blureange.io/licensing
// **
// ** This file is part of the Bluerange/FruityMesh implementation
// **
// ** $BR_BEGIN_LICENSE:GPL-EXCEPT$
// ** Commercial License Usage
// ** Licensees holding valid commercial Bluerange licenses may use this file in
// ** accordance with the commercial license agreement provided with the
// ** Software or, alternatively, in accordance with the terms contained in
// ** a written agreement between them and M-Way Solutions GmbH.
// ** For licensing terms and conditions see https://www.bluerange.io/terms-conditions. For further
// ** information use the contact form at https://www.bluerange.io/contact.
// **
// ** GNU General Public License Usage
// ** Alternatively, this file may be used under the terms of the GNU
// ** General Public License version 3 as published by the Free Software
// ** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
// ** included in the packaging of this file. Please review the following
// ** information to ensure the GNU General Public License requirements will
// ** be met: https://www.gnu.org/licenses/gpl-3.0.html.
// **
// ** $BR_END_LICENSE$
// **
// ****************************************************************************/
////////////////////////////////////////////////////////////////////////////////
#include "SocketTerm.h"
#include "Exceptions.h"
#include "CherrySim.h"

#include <event2/event.h>
#include <event2/event_struct.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#ifdef __unix
#include <arpa/inet.h>
#endif

constexpr size_t TRACE_BUFFER_SIZE = 500;
static constexpr int INPUT_CHUNK_SIZE = 1 * 1024;
static constexpr int MAX_INPUT_BUFFER_SIZE = 100 * 1024;
static constexpr int MAX_NUM_BUFFERED_INPUT_LINES = 30;

struct event_base* SocketTerm::eventBase = nullptr;
struct event SocketTerm::ev_accept = {};
std::vector<SocketClient*> SocketTerm::clients = {};

SocketTerm::SocketTerm()
{
}

SocketTerm::~SocketTerm()
{
}

void SocketTerm::CreateServerSocket(uint16_t port)
{
    int listen_fd;
    struct sockaddr_in listen_addr;
    char reuseaddr_on;

    /* Initialize libevent. */
    eventBase = event_base_new();

    /* Create our listening socket. */
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        printf("SocketTerm: Could not open Socket for listening" EOL);
        SIMEXCEPTION(IllegalStateException);
    }
    CheckedMemset(&listen_addr, 0, sizeof(listen_addr));
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_addr.s_addr = INADDR_ANY;
    listen_addr.sin_port = htons(port);
    if (bind(listen_fd, (struct sockaddr*) & listen_addr, sizeof(listen_addr)) < 0) {
        printf("SocketTerm: Could not bind to Socket" EOL);
        SIMEXCEPTION(IllegalStateException);
    }
    if (listen(listen_fd, 5) < 0) {

        printf("SocketTerm: Could not listen on Socket" EOL);
        SIMEXCEPTION(IllegalStateException);
    }
    reuseaddr_on = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_on, sizeof(reuseaddr_on));

    /* Set the socket to non-blocking, this is essential in event
     * based programming with libevent. */
    if (evutil_make_socket_nonblocking(listen_fd) < 0) {
        printf("SocketTerm: Failed to set server socket to non-blocking" EOL);
        SIMEXCEPTION(IllegalStateException);
    }

    /* We now have a listening socket, we create a read event to
     * be notified when a client connects. */
    event_assign(&ev_accept, eventBase, listen_fd, EV_READ | EV_PERSIST, SocketTerm::ClientConnectedHandler, nullptr);
    event_add(&ev_accept, nullptr);

    printf("SocketTerm: Listening on port %u" EOL, (u32)port);
}

void SocketTerm::ClientConnectedHandler(int fd, short ev, void *arg)
{
    //As soon as a client disconnects, we disable stdio to improve performance
    //We need to activate all terminals in CherrySim so that we can grab what we need
    cherrySimInstance->simConfig.terminalId = 0;

    ErrorType err = ErrorType::SUCCESS;

    printf("SocketTerm: New client %d connected, event %u" EOL, fd, (u32)ev);

    int client_fd;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    SocketClient* client;

    client_fd = accept(fd, (struct sockaddr*) & client_addr, &client_len);
    if (client_fd < 0) {
        printf("SocketTerm: Could not accept Client" EOL);
        return;
    }

    /* Set the client socket to non-blocking mode. */
    if (evutil_make_socket_nonblocking(client_fd) < 0) {
        printf("SocketTerm: Failed to set client socket non-blocking" EOL);
    }

    /* We've accepted a new client, create a client object. */
    client = new SocketClient();
    if (client == nullptr) {
        printf("SocketTerm: Could not allocate memory for client" EOL);
        SIMEXCEPTION(IllegalStateException);
        return;
    }
    client->fd = client_fd;

    client->buf_ev = bufferevent_socket_new(eventBase, client_fd, 0);
    bufferevent_setcb(client->buf_ev, SocketTerm::ClientOnReadHandler, NULL, SocketTerm::ClientOnErrorHandler, client);

    /* We have to enable it before our callbacks will be
     * called. */
    bufferevent_enable(client->buf_ev, EV_READ);

    /* Add the new client to the tailq. */
    clients.push_back(client);

    printf("SocketTerm: Accepted connection from %s, use 'sim term [terminalId]' first\n", inet_ntoa(client_addr.sin_addr));

    // Send a welcome message
    SendToClient(client, R"({"type":"sim_socket_connected","code":%u})" SEP, err);

    if (err != ErrorType::SUCCESS) {
        DisconnectClient(client);
    }
}

//Called by libevent as soon as data was received from a client
void SocketTerm::ClientOnReadHandler(struct bufferevent* bev, void* arg)
{
    // Set the flag inidicating that data from the mesh gateway has been received.
    cherrySimInstance->receivedDataFromMeshGw = true;

    auto * this_client = static_cast<SocketClient *>(arg);
    uint8_t data[8192];
    while (this_client->inputBuffer.size() + INPUT_CHUNK_SIZE < MAX_INPUT_BUFFER_SIZE)
    {
        u32 count = bufferevent_read(bev, data, sizeof(data));

        for (u32 i = 0; i < count; i++)
        {
            //We also keep a linear buffer of our current line so that we can evaluate it
            //In the context of the client
            //If this buffer is full without any line ending, we end it
            if (this_client->inputLine.lineLength >= TERMINAL_READ_BUFFER_LENGTH) {
                data[i] = '\n';
            }
            if (this_client->inputLine.lineLength < TERMINAL_READ_BUFFER_LENGTH) {
                this_client->inputLine.data[this_client->inputLine.lineLength++] = data[i];
            }

            //Store the data in our inputBuffer
            this_client->inputBuffer.push(data[i]);

            //Check for line endings
            if (data[i] == '\n') {
                ProcessInput(this_client, this_client->inputLine.data, this_client->inputLine.lineLength);

                this_client->inputLine.lineLength = 0;
                this_client->fullLinesAvailable++;
            }
        }
        if (count == 0) break;

        //TODO: Make sure this works when the buffer is filled with too much data and no line break
    }
}

//Called by libevent as soon as there was a socket error
void SocketTerm::ClientOnErrorHandler(struct bufferevent* bev, short what, void* arg)
{
    auto * client = static_cast<SocketClient *>(arg);

    if (what & BEV_EVENT_EOF) {
        /* Client disconnected, remove the read event and the
         * free the client structure. */
        printf("SocketTerm: Client disconnected.\n");
    }
    else {
        printf("SocketTerm: Client socket error %d, disconnecting.\n", (int)what);
    }

    DisconnectClient(client);
}

void SocketTerm::ProcessSockets()
{
    if (eventBase == nullptr) return;

    event_base_loop(eventBase, EVLOOP_NONBLOCK);
}

void SocketTerm::SocketTermInitNode(NodeEntry *node)
{
    // Nothing to do for now
}

u32 SocketTerm::CheckAndGetLine(NodeEntry *nodeEntry, char *buffer, u16 bufferLength)
{
    SocketClient *client = FindUniqueClientByNodeEntry(nodeEntry);

    if (client != nullptr && client->fullLinesAvailable > 0) {
        u32 inputCount = client->inputBuffer.size();
        for (u32 i = 0; i < inputCount && i < bufferLength; i++) {
            buffer[i] = client->inputBuffer.front();
            client->inputBuffer.pop();
            if (buffer[i] == '\n') {
                u16 length = i;
                //Support CRLF as well
                if (i > 0 && buffer[i - 1] == '\r') {
                    buffer[i - 1] = '\0';
                    length--;
                }
                buffer[i] = '\0';

                return length;
            }
        }
        //If we came to this point we have not found a line ending
        client->fullLinesAvailable--;
    }
    return 0;
}

void SocketTerm::PutString(NodeEntry *nodeEntry, const char *buffer, u16 bufferLength)
{
    SocketClient *client = FindUniqueClientByNodeEntry(nodeEntry);

    if (client != nullptr) {
        bufferevent_write(client->buf_ev, buffer, bufferLength);
    }
}


// ############ SocketClient ####################

SocketClient::SocketClient()
{

}

SocketClient::~SocketClient()
{

}


// ############ Helpers ####################

SocketClient *SocketTerm::FindUniqueClientByNodeEntry(const NodeEntry *nodeEntry)
{
    SocketClient *client = nullptr;
    for (const auto &currentClient : clients)
    {
        if (currentClient->nodeEntry == nodeEntry)
        {
            if (client != nullptr)
            {
                SIMEXCEPTION(IllegalStateException);
            }
            client = currentClient;
        }
    }
    return client;
}

bool SocketTerm::SendToClient(SocketClient* client, const char* message, ...)
{
    char buffer2[TRACE_BUFFER_SIZE] = {};

    //Variable argument list must be passed to vnsprintf
    va_list aptr;
    va_start(aptr, message);
    vsnprintf(buffer2, TRACE_BUFFER_SIZE, message, aptr);
    va_end(aptr);

    return bufferevent_write(client->buf_ev, buffer2, strlen(buffer2)) == 0;
}

bool SocketTerm::ProcessInput(SocketClient* client, char* buffer, u16 bufferLength)
{
    if (strncmp(buffer, "sim term ", 9) == 0)
    {
        ErrorType err = ErrorType::SUCCESS;

        char *     end        = buffer + bufferLength;
        const auto terminalId = static_cast<TerminalId>(strtoul(buffer + 9, &end, 10));

        if (terminalId == 0)
        {
            err = ErrorType::INVALID_PARAM;
        }
        else
        {
            auto *nodeEntry = cherrySimInstance->FindUniqueNodeByTerminalId(terminalId);

            if (nodeEntry == nullptr)
            {
                err = ErrorType::NOT_FOUND;
            }

            // Next, make sure the terminal is not already taken by another client

            // Switch to the new terminal and clear all input
            if (nodeEntry != nullptr)
            {
                if (FindUniqueClientByNodeEntry(nodeEntry))
                {
                    err = ErrorType::FORBIDDEN;
                }
                else
                {
                    client->nodeEntry = nodeEntry;
                }
            }
        }

        // In each case, we clear the buffers
        ResetClientInput(client);

        // Send a response to the client
        if (client->nodeEntry)
        {
            SendToClient(
                client,
                R"({"type":"sim_term_changed","code":%u,"status":"attached_to_node",)"
                R"("simulatorNodeIndex":%u,"nodeId":%u,"networkId":%u})" SEP,
                err, static_cast<unsigned>(client->nodeEntry->index),
                static_cast<unsigned>(client->nodeEntry->GetNodeId()),
                static_cast<unsigned>(client->nodeEntry->GetNetworkId()));
        }
        else
        {
            SendToClient(client, R"({"type":"sim_term_changed","code":%u,"status":"not_attached"})" SEP, err);
        }

        return true;
    }
    else if (client->nodeEntry == nullptr)
    {
        SendToClient(client, R"(SocketTerm: Please use 'sim term [nodeId]' to select a terminal first.)" SEP);
    }

    return false;
}

void SocketTerm::ResetClientInput(SocketClient* client)
{
    client->inputBuffer = std::queue<u8>();
    client->fullLinesAvailable = 0;
    client->inputLine.lineLength = 0;
}

void SocketTerm::DisconnectClient(SocketClient* client)
{
    //Erase from the queue of clients
    clients.erase(std::remove(clients.begin(), clients.end(), client), clients.end());

    bufferevent_free(client->buf_ev);
    evutil_closesocket(client->fd);
    delete client;
}

bool SocketTerm::IsTermActive(const NodeEntry *nodeEntry)
{
    SocketClient *client = FindUniqueClientByNodeEntry(nodeEntry);
    return client != nullptr;
}