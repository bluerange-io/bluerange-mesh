////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2021 M-Way Solutions GmbH
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
#include <FruitySimServer.h>

#define WIN32_LEAN_AND_MEAN

#include <memory>
#include <cstdint>
#include <iostream>
#if defined(SIM_SERVER_PRESENT)
#include <evhttp.h>
#endif // SIM_SERVER_PRESENT
#include <thread>
#include <json.hpp>
#include <stdio.h>

#include <CherrySim.h>
#include <CherrySimUtils.h>
#include <Node.h>
#include <MeshConnection.h>
#include "MersenneTwister.h"

#ifdef __unix
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif // __unix

#ifdef _MSC_VER
#define fileno _fileno
#endif

/**
This provides a basic webserver that serves the fruitymap and information about the mesh.
The only thing necessary to view the simulation is a browser, that's it :-)

FIXME: Objects are currently not destroyed properly, so the application will crash when closing
*/

using json = nlohmann::json;

#if defined(SIM_SERVER_PRESENT)
#ifdef _WIN32
WSADATA wsaData;
static bool WSAStartupWasCalled = false;
#endif //_WIN32
event_base* eventBase = nullptr;
std::unique_ptr<evhttp, decltype(&evhttp_free)>* server = nullptr;


//HACK! WSAStartup has a memory leak when called several times, even then WSACleanup is called the same
//amount of times as documented here: https://docs.microsoft.com/en-us/windows/desktop/api/winsock/nf-winsock-wsastartup
//This Class and the variable WSAStartupWasCalled makes sure that WSAStartup and WSACleanup is only called once.
#ifdef _WIN32
class WSACleanuper {
public:
    ~WSACleanuper() {
        if (WSAStartupWasCalled) {
            WSACleanup();
        }
    }
};
static WSACleanuper wsaCleanuper;
#endif // _WIN32

#endif // SIM_SERVER_PRESENT

FruitySimServer::FruitySimServer()
{
    StartServer();
}

int FruitySimServer::StartServer()
{
    MersenneTwisterDisabler disabler;
#if defined(SIM_SERVER_PRESENT)
#ifdef _WIN32
    //Initialize Winsock
    if (!WSAStartupWasCalled) {
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            printf("WSAStartup failed: %d\n", result);
            return 1;
        }
        WSAStartupWasCalled = true;
    }
#endif // _WIN32
    
    eventBase = event_init();

    if (!eventBase)
    {
        std::cerr << "Failed to init libevent." << std::endl;
        return -1;
    }

    char const SrvAddress[] = "0.0.0.0";
    std::uint16_t SrvPort = 5555;
    server = new std::unique_ptr<evhttp, decltype(&evhttp_free)>(evhttp_start(SrvAddress, SrvPort), &evhttp_free);
    if (!server)
    {
        std::cerr << "Failed to init http server." << std::endl;
        return -1;
    }

    void(*OnReq)(evhttp_request *req, void *) = [](evhttp_request *req, void *)
    {
        FILE* file = nullptr;
    
        auto *OutBuf = evhttp_request_get_output_buffer(req);
        if (!OutBuf)
            return;

        if (strstr(req->uri, "/devices") != nullptr)
        {
            std::string devices = GenerateDevicesJson();
    
            evbuffer_add_printf(OutBuf, "%s", devices.c_str());
            evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "application/json");
        }
        else if (strstr(req->uri, "/site") != nullptr)
        {
            std::string site = GenerateSiteJson();
            evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "application/json");
    
            evbuffer_add_printf(OutBuf, "%s", site.c_str());
        }
        else {
            //Serve static files, no directories supported, only some mimetypes work, hacky approach
            std::string fileName = CherrySimUtils::GetNormalizedPath() + "/";

            if (strstr(req->uri, ".png") != nullptr)
            {
                fileName += std::string("web/img/") + std::string(req->uri + 9);
            }
            else if (strcmp(req->uri, "/") == 0) {
                fileName += std::string("web/index.html");
            }
            else if (strcmp(req->uri, "/simulator/floorplan") == 0) {
                fileName += std::string("web/img/floorplan.png");
            }
            else if (strcmp(req->uri, "/simulator/wallplan") == 0) {
                fileName += std::string("web/img/floorplan.png");
            }
            else {
                fileName += std::string("web/") + std::string(req->uri + 1);
            }
            if ((file = fopen(fileName.c_str(), "r")) != nullptr) {
                struct stat buf;
                fstat(fileno(file), &buf);
                off_t size = buf.st_size;
    
                if (strstr(fileName.c_str(), ".html") != nullptr) {
                    evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "text/html");
                }
                else if (strstr(fileName.c_str(), ".js") != nullptr) {
                    evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "application/javascript");
                }
                else if (strstr(fileName.c_str(), ".png") != nullptr) {
                    evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "image/png");
                }
    
                //Add the file (will close the file once done)
                evbuffer_add_file(OutBuf, fileno(file), 0, size);
            }
        }
    
        evhttp_send_reply(req, HTTP_OK, "OK", OutBuf);
    };
    
    evhttp_set_gencb(server->get(), OnReq, nullptr);
#endif // SIM_SERVER_PRESENT
    return 0;
}

FruitySimServer::~FruitySimServer()
{
#if defined(SIM_SERVER_PRESENT)
    if (server != nullptr) delete server;
    server = nullptr;
    event_base_free(eventBase);
#endif // SIM_SERVER_PRESENT
}

//In the inspector of the debugger, you can call "cherrySimInstance->server->ProcessServerRequests()" to be able
//to view the current state of the simulation while it is halted at a breakpoint
void FruitySimServer::ProcessServerRequests()
{
    MersenneTwisterDisabler disabler;
#if defined(SIM_SERVER_PRESENT)
    event_base_loop(eventBase, EVLOOP_NONBLOCK);
#endif // SIM_SERVER_PRESENT
}

#if defined(SIM_SERVER_PRESENT)
std::string FruitySimServer::GenerateSiteJson()
{
    MersenneTwisterDisabler disabler;
    json site;

    site["heightInMeter"] = cherrySimInstance->simConfig.mapHeightInMeters;
    site["lengthInMeter"] = cherrySimInstance->simConfig.mapWidthInMeters;
    site["name"] = "SimulatorSite";
    site["pixelPerMeter"] = 5;

    return site.dump(4);
}
#endif // SIM_SERVER_PRESENT

#if defined(SIM_SERVER_PRESENT)
std::string FruitySimServer::GenerateDevicesJson()
{
    MersenneTwisterDisabler disabler;
    json devices;
    devices["status"] = "success";
    for (unsigned int i = 0; i < cherrySimInstance->GetTotalNodes(); i++) {
        NodeIndexSetter nodeIndexSetter(i);
        NodeEntry* node = &cherrySimInstance->nodes[i];
        json device;

        //Get the only handshaked inConnection
        //TODO: The inConnection is only used to draw the direction arrow in the fruitymap, but currently
        //the json only supports communicating 1 inConnection, this should be changed at some point so that
        //Each connection can report its direction and masterBit
        auto inConnections = node->gs.cm.GetMeshConnections(ConnectionDirection::DIRECTION_IN);
        MeshConnection* inConnection = nullptr;
        for (int k = 0; k < inConnections.count; k++) {
            if (inConnections.handles[k] && inConnections.handles[k].IsHandshakeDone()) {
                inConnection = inConnections.handles[k].GetConnection();
            }
        }

        //UUID is generated based on the node index
        char uuid[50];
        sprintf(uuid, "00000000-1111-2222-3333-00000000%04u", node->index);

        device["uuid"] = uuid;
        device["deviceId"] = node->gs.config.GetSerialNumber();
        device["platform"] = "BLENODE";
        device["ledOn"] = node->led1On || node->led2On || node->led3On;
        device["inConnectionHasMasterBit"] = false;
        device["inConnectionPartnerHasMasterBit"] = false;

        //Find out who has the master bit of the inConnection
        if(inConnection != nullptr) device["inConnectionHasMasterBit"] = inConnection->connectionMasterBit == 1;

        bool partnerHasMB = false;

        if (inConnection != nullptr) {
            SoftdeviceConnection* foundSoftdeviceConnection = cherrySimInstance->FindConnectionByHandle(node, inConnection->connectionHandle);
            //We must check if the simulator connection still exists as it might have been cleaned up already
            if (foundSoftdeviceConnection != nullptr) {
                NodeEntry* partnerNode = foundSoftdeviceConnection->partner;
                MeshConnections conn = partnerNode->gs.cm.GetMeshConnections(ConnectionDirection::DIRECTION_OUT);
                for (int k = 0; k < conn.count; k++) {
                    if (conn.handles[k] && conn.handles[k].GetConnectionHandle() == inConnection->connectionHandle) {
                        partnerHasMB = conn.handles[k].GetConnection()->connectionMasterBit;
                    }
                }
            }
        }

        device["inConnectionPartnerHasMasterBit"] = partnerHasMB;

        device["connectionLossCounter"] = node->gs.node.connectionLossCounter;
        device["inConnectionPartner"] = inConnection == nullptr ? 0 : inConnection->partnerId;


        //FIXME: This mixes fruitymesh and simulator connections, but should only use simulator data
        if (inConnection != nullptr) {
            SoftdeviceConnection* sdInConn = cherrySimInstance->FindConnectionByHandle(node, inConnection->connectionHandle);
            if (sdInConn != nullptr) device["inConnectionRssi"] = (int)cherrySimInstance->GetReceptionRssiNoNoise(node, sdInConn->partner);
        }
        else {
            device["inConnectionRssi"] = 0;
        }


        char advData[200];
        if (node->state.advertisingActive) {
            Logger::ConvertBufferToHexString(node->state.advertisingData, node->state.advertisingDataLength, advData, sizeof(advData));
        }
        else {
            sprintf(advData, "Not advertising");
        }

        device["details"] = {
            {"platform", "BLENODE"},
            {"clusterId", node->gs.node.clusterId},
            {"clusterSize", node->gs.node.GetClusterSize()},
            {"nodeId", node->gs.node.configuration.nodeId},
            {"serialNumber", node->gs.config.GetSerialNumber()},
            {"connections", json::array()},
            {"nonConnections", json::array()},
            {"lastSentAdvertisingMessage", advData},
            {"freeIn", node->gs.cm.freeMeshInConnections},
            {"freeOut", node->gs.cm.freeMeshOutConnections}
        };
        for (int j = 0; j < node->state.configuredTotalConnectionCount; j++) {
            if (node->state.connections[j].connectionActive) {
                json connection;
                connection["handle"] = node->state.connections[j].connectionHandle;
                connection["rssi"] = 7;
                connection["target"] = node->state.connections[j].partner->gs.node.configuration.nodeId;

                device["details"]["connections"].push_back(connection);
            }
        }
        device["properties"] = {
            {"onMap", "true"},
            {"x", node->x},
            {"y", node->y}
        };
        devices["result"].push_back(device);
    }

    return devices.dump(4);
}
#endif // SIM_SERVER_PRESENT
