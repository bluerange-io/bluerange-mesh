////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// ** BlueRange Mesh – Community Edition (CE)
// ** Copyright (c) 2015-2021 MWAY DIGITAL GmbH, Germany
// ** Copyright (c) 2021-2026 BlueRange GmbH, Germany
// **
// ** This file is part of BlueRange Mesh Community Edition (formerly known as
// ** FruityMesh).
// **
// ** BlueRange Mesh Community Edition is free software: you can redistribute it
// ** and/or modify it under the terms of the GNU General Public License as
// ** published by the Free Software Foundation, either version 3 of the
// ** License, or (at your option) any later version.
// **
// ** BlueRange Mesh Community Edition is distributed in the hope that it will
// ** be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
// ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// ** See the GNU General Public License for more details.
// **
// ** You should have received a copy of the GNU General Public License along
// ** with this program. If not, see https://www.gnu.org/licenses/.
// **
// ** IMPORTANT:
// ** Any modification, extension, or derivative work of this file MUST also be
// ** licensed under the GNU General Public License v3 or later and the complete
// ** corresponding source code MUST be made available.
// **
// ** Commercial Use:
// ** If you wish to use this software without the obligations of the GPLv3
// ** (including source code disclosure), a commercial license for
// ** BlueRange Mesh OEM Edition is required.
// **
// ** License violations automatically terminate your rights under this license
// ** and may result in legal action under applicable law.
// ** For further information please use the contact form at:
// ** https://bluerange.io/en/contact
// ****************************************************************************/
////////////////////////////////////////////////////////////////////////////////

#pragma once
#ifndef __EMSCRIPTEN__
#include <vector>
#include <CherrySimTypes.h>
#include <CircularBuffer.h>

typedef struct TerminalLine {
    u16 lineLength = 0;
    char data[TERMINAL_READ_BUFFER_LENGTH] = {};
} TerminalLine;

//This class holds all the information necessary to deal with one connected client
class SocketClient
{
public:
    SocketClient();
    ~SocketClient();

    //Client Socket file descriptor
    int fd = 0;

    //BufferEvent from libevent
    struct bufferevent* buf_ev = {};

    /// Pointer to the node entry whose terminal is connected with this client or nullptr if it is not (yet)
    /// connected with any terminal.
    ///
    /// The terminal is switched using "sim term [nodeId]"
    NodeEntry *nodeEntry = nullptr;

    //The line buffer buffers a single line until it was read
    TerminalLine inputLine = {};

    std::queue<u8> inputBuffer = {};

    //Set to true if we know that a full line is available to not check every time
    int fullLinesAvailable = 0;
};

/*
 * The SocketTerm allows multiple clients to connect to the simulation over raw TCP sockets
 * For testing, a simple telnet client can be used
 *
 *
 * Restrictions:
 * - Only a single client can connect to the terminal of a node, so the number of clients is limited
*    to the number of nodes. This restriction is arbitrary but might simplify some stuff in the future.
 */
class SocketTerm
{
private:

    static struct event_base* eventBase;

    static struct event ev_accept;

    static std::vector<SocketClient*> clients;

public:
    //This should create a server socket to that a number of clients can connect to this process
    //It will listen on the given port to accept connections
    //This will crash with an IllegalStateException if the port is not available
    static void CreateServerSocket(uint16_t port);

    //Called by libevent as soon as a new client is connected
    static void ClientConnectedHandler(int fd, short ev, void* arg);

    //Called by libevent as soon as data was received from a client
    static void ClientOnReadHandler(struct bufferevent* bev, void* arg);

    //Called by libevent as soon as there was a socket error
    static void ClientOnErrorHandler(struct bufferevent* bev, short what, void* arg);

    //Must be called periodically to process input and output data
    static void ProcessSockets();

public:
    SocketTerm();
    ~SocketTerm();

    //This is called by each node, not sure what it is supposed to do yet
    static void SocketTermInitNode(NodeEntry* node);

    // Called by each node, should check if there is terminal data for this node
    static u32 CheckAndGetLine(NodeEntry *nodeEntry, char *buffer, u16 bufferLength);

    // Is called by the implementation of a node and should transfer some data to this terminals socket
    static void PutString(NodeEntry *nodeEntry, const char *buffer, u16 bufferLength);

    /// Find the socket client which is connected to the specified node entry.
    static SocketClient *FindUniqueClientByNodeEntry(const NodeEntry *nodeEntry);

    /// Checks if a certain node entry is connected to a socket client.
    static bool IsTermActive(const NodeEntry *nodeEntry);

private:
    //Allows the SocketTerm to process a line before giving it to the CherrySim
    //Returns true if the line was processed and should then not be passed to the sim
    static bool ProcessInput(SocketClient* client, char* buffer, u16 bufferLength);

    //Helper method for sending some data to a client
    //message must be \0 terminated
    static bool SendToClient(SocketClient* client, const char* message, ...);

    //Resets the input buffers of a client
    static void ResetClientInput(SocketClient* client);

    //Disconnects a client and frees its resources
    static void DisconnectClient(SocketClient* client);
};
#endif
