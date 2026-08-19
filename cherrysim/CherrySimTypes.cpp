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

#include "CherrySimTypes.h"
#include "CherrySim.h"
#include <cstdio>

void to_json(nlohmann::json& j, const SimConfiguration & config)
{
    j = nlohmann::json{
        { "nodeConfigName"                           , config.nodeConfigName                            },
        { "seed"                                     , config.seed                                      },
        { "mapWidthInMeters"                         , config.mapWidthInMeters                          },
        { "mapHeightInMeters"                        , config.mapHeightInMeters                         },
        { "mapElevationInMeters"                     , config.mapElevationInMeters                      },
        { "floorplanImage"                           , config.floorplanImage                            },
        { "simTickDurationMs"                        , config.simTickDurationMs                         },
        { "terminalId"                               , config.terminalId                                },
        { "simOtherDelay"                            , config.simOtherDelay                             },
        { "playDelay"                                , config.playDelay                                 },
        { "interruptProbability"                     , config.interruptProbability                      },
        { "connectionTimeoutProbabilityPerSec"       , config.connectionTimeoutProbabilityPerSec        },
        { "sdBleGapAdvDataSetFailProbability"        , config.sdBleGapAdvDataSetFailProbability         },
        { "sdBusyProbability"                        , config.sdBusyProbability                         },
        { "sdBusyProbabilityUnlikely"                , config.sdBusyProbabilityUnlikely                 },
        { "simulateAsyncFlash"                       , config.simulateAsyncFlash                        },
        { "asyncFlashCommitTimeProbability"          , config.asyncFlashCommitTimeProbability           },
        { "importFromJson"                           , config.importFromJson                            },
        { "realTime"                                 , config.realTime                                  },
        { "receptionProbabilityVeryClose"            , config.receptionProbabilityVeryClose             },
        { "receptionProbabilityClose"                , config.receptionProbabilityClose                 },
        { "receptionProbabilityFar"                  , config.receptionProbabilityFar                   },
        { "receptionProbabilityVeryFar"              , config.receptionProbabilityVeryFar               },
        { "siteJsonPath"                             , config.siteJsonPath                              },
        { "devicesJsonPath"                          , config.devicesJsonPath                           },
        { "replayPath"                               , config.replayPath                                },
        { "logReplayCommands"                        , config.logReplayCommands                         },
        { "useLogAccumulator"                        , config.useLogAccumulator                         },
        { "defaultNetworkId"                         , config.defaultNetworkId                          },
        { "ignoreDeviceJsonEnrollments"              , config.ignoreDeviceJsonEnrollments               },
        { "preDefinedPositions"                      , config.preDefinedPositions                       },
        { "rssiNoise"                                , config.rssiNoise                                 },
        { "simulateWatchdog"                         , config.simulateWatchdog                          },
        { "simulateJittering"                        , config.simulateJittering                         },
        { "verbose"                                  , config.verbose                                   },
        { "fastLaneToSimTimeMs"                      , config.fastLaneToSimTimeMs                       },
        { "enableClusteringValidityCheck"            , config.enableClusteringValidityCheck             },
        { "enableSimStatistics"                      , config.enableSimStatistics                       },
        { "storeFlashToFile"                         , config.storeFlashToFile                          },
        { "floorBiasInMeters"                        , config.floorBiasInMeters                         },
        { "ceilingHeightInMeters"                    , config.ceilingHeightInMeters                     },
        { "ceilingAttenuationDb"                     , config.ceilingAttenuationDb                      },
        { "perfectReceptionProbabilityForAdvertising", config.perfectReceptionProbabilityForAdvertising },
        { "perfectReceptionProbabilityForConnection" , config.perfectReceptionProbabilityForConnection  },
        { "verboseCommands"                          , config.verboseCommands                           },
        { "simulateAdvertisingIndexStep"             , config.simulateAdvertisingIndexStep              },
        { "disableNonCriticalExceptions"             , config.disableNonCriticalExceptions              },
        { "webServerPort"                            , config.webServerPort                             },
        { "socketServerPort"                         , config.socketServerPort                          },
    };
}

void from_json(const nlohmann::json & j, SimConfiguration & config)
{
    for (nlohmann::json::const_iterator it = j.begin(); it != j.end(); ++it)
    {

             if(it.key() == "nodeConfigName"                            ) j.at("nodeConfigName").get_to(config.nodeConfigName);
        else if(it.key() == "seed"                                      ) config.seed                                      = *it;
        else if(it.key() == "mapWidthInMeters"                          ) config.mapWidthInMeters                          = *it;
        else if(it.key() == "mapHeightInMeters"                         ) config.mapHeightInMeters                         = *it;
        else if(it.key() == "mapElevationInMeters"                      ) config.mapElevationInMeters                      = *it;
        else if(it.key() == "floorplanImage"                            ) config.floorplanImage                            = *it;
        else if(it.key() == "simTickDurationMs"                         ) config.simTickDurationMs                         = *it;
        else if(it.key() == "terminalId"                                ) config.terminalId                                = *it;
        else if(it.key() == "simOtherDelay"                             ) config.simOtherDelay                             = *it;
        else if(it.key() == "playDelay"                                 ) config.playDelay                                 = *it;
        else if(it.key() == "interruptProbability"                      ) config.interruptProbability                      = *it;
        else if(it.key() == "connectionTimeoutProbabilityPerSec"        ) config.connectionTimeoutProbabilityPerSec        = *it;
        else if(it.key() == "sdBleGapAdvDataSetFailProbability"         ) config.sdBleGapAdvDataSetFailProbability         = *it;
        else if(it.key() == "sdBusyProbability"                         ) config.sdBusyProbability                         = *it;
        else if(it.key() == "sdBusyProbabilityUnlikely"                 ) config.sdBusyProbabilityUnlikely                 = *it;
        else if(it.key() == "simulateAsyncFlash"                        ) config.simulateAsyncFlash                        = *it;
        else if(it.key() == "asyncFlashCommitTimeProbability"           ) config.asyncFlashCommitTimeProbability           = *it;
        else if(it.key() == "importFromJson"                            ) config.importFromJson                            = *it;
        else if(it.key() == "realTime"                                  ) config.realTime                                  = *it;
        else if(it.key() == "receptionProbabilityVeryClose"             ) config.receptionProbabilityVeryClose             = *it;
        else if(it.key() == "receptionProbabilityClose"                 ) config.receptionProbabilityClose                 = *it;
        else if(it.key() == "receptionProbabilityFar"                   ) config.receptionProbabilityFar                   = *it;
        else if(it.key() == "receptionProbabilityVeryFar"               ) config.receptionProbabilityVeryFar               = *it;
        else if(it.key() == "siteJsonPath"                              ) config.siteJsonPath                              = *it;
        else if(it.key() == "devicesJsonPath"                           ) config.devicesJsonPath                           = *it;
        else if(it.key() == "replayPath"                                ) config.replayPath                                = *it;
        else if(it.key() == "logReplayCommands"                         ) config.logReplayCommands                         = *it;
        else if(it.key() == "useLogAccumulator"                         ) config.useLogAccumulator                         = *it;
        else if(it.key() == "defaultNetworkId"                          ) config.defaultNetworkId                          = *it;
        else if(it.key() == "ignoreDeviceJsonEnrollments"               ) config.ignoreDeviceJsonEnrollments               = *it;
        else if(it.key() == "preDefinedPositions"                       ) j.at("preDefinedPositions").get_to(config.preDefinedPositions);
        else if(it.key() == "rssiNoise"                                 ) config.rssiNoise                                 = *it;
        else if(it.key() == "simulateWatchdog"                          ) config.simulateWatchdog                          = *it;
        else if(it.key() == "simulateJittering"                         ) config.simulateJittering                         = *it;
        else if(it.key() == "verbose"                                   ) config.verbose                                   = *it;
        else if(it.key() == "fastLaneToSimTimeMs"                       ) config.fastLaneToSimTimeMs                       = *it;
        else if(it.key() == "enableClusteringValidityCheck"             ) config.enableClusteringValidityCheck             = *it;
        else if(it.key() == "enableSimStatistics"                       ) config.enableSimStatistics                       = *it;
        else if(it.key() == "storeFlashToFile"                          ) config.storeFlashToFile                          = *it;
        else if(it.key() == "floorBiasInMeters"                         ) config.floorBiasInMeters                         = *it;
        else if(it.key() == "ceilingHeightInMeters"                     ) config.ceilingHeightInMeters                     = *it;
        else if(it.key() == "ceilingAttenuationDb"                      ) config.ceilingAttenuationDb                      = *it;
        else if(it.key() == "perfectReceptionProbabilityForAdvertising" ) config.perfectReceptionProbabilityForAdvertising = *it;
        else if(it.key() == "perfectReceptionProbabilityForConnection"  ) config.perfectReceptionProbabilityForConnection  = *it;
        else if(it.key() == "verboseCommands"                           ) config.verboseCommands                           = *it;
        else if(it.key() == "simulateAdvertisingIndexStep"              ) config.simulateAdvertisingIndexStep              = *it;
        else if(it.key() == "disableNonCriticalExceptions"              ) config.disableNonCriticalExceptions              = *it;
        else if(it.key() == "webServerPort"                             ) config.webServerPort                             = *it;
        else if(it.key() == "socketServerPort"                          ) config.socketServerPort                          = *it;
        else printf("WARNING: Unknown json entry %s in CherrySimConfig", it.key().c_str());
    }
}

void NodeEntry::Initialize(u32 nodeIndex)
{
    this->index                        = nodeIndex;
    this->gs.node.configuration.nodeId = static_cast<NodeId>(nodeIndex + 1);

    // Initialize FICR memory
    CheckedMemset(&ficr, 0xFF, sizeof(ficr));

    // Initialize UICR memory
    CheckedMemset(&uicr, 0xFF, sizeof(uicr));

    // Initialize flash memory
    CheckedMemset(&flash, 0xFF, sizeof(flash));
    // TODO: We could load a softdevice and app image into flash, would that help for something?

    // Generate device address based on the id works for up to 65535 addresses
    address.addr_type = FruityHal::BleGapAddrType::RANDOM_STATIC;
    address.addr.fill(0x00);
    CheckedMemcpy(address.addr.data() + sizeof(u16), &this->gs.node.configuration.nodeId, sizeof(u16));
}

void SimConfiguration::SetToPerfectConditions()
{
    this->interruptProbability = UINT32_MAX;
    this->connectionTimeoutProbabilityPerSec = 0;
    this->sdBleGapAdvDataSetFailProbability = 0;
    this->sdBusyProbability = 0;
    this->asyncFlashCommitTimeProbability = UINT32_MAX;
    this->receptionProbabilityVeryClose = UINT32_MAX;
    this->receptionProbabilityClose = UINT32_MAX;
    this->receptionProbabilityFar = UINT32_MAX;
    this->receptionProbabilityVeryFar = UINT32_MAX;
    this->perfectReceptionProbabilityForAdvertising = true;
    this->perfectReceptionProbabilityForConnection = true;
}

float NodeEntry::GetXinMeters() const
{
    return this->x * cherrySimInstance->simConfig.mapWidthInMeters;
}

float NodeEntry::GetYinMeters() const
{
    return this->y * cherrySimInstance->simConfig.mapHeightInMeters;
}

float NodeEntry::GetZinMeters() const
{
    return this->z * cherrySimInstance->simConfig.mapElevationInMeters;
}