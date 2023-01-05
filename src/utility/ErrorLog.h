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

#pragma once

#include <SimpleQueue.h>

/*############ Error Types ################*/
//Errors are saved in RAM and can be requested through the mesh

enum class LoggingError : u8 {
    GENERAL_ERROR = 0, //Defined in "FmTypes.h" (ErrorType)
    HCI_ERROR = 1, //Defined in "FruityHalError.h" (BleHciError)
    CUSTOM = 2, //Defined below (CustomErrorTypes)
    GATT_STATUS = 3, //Defined in "FruityHalError.h" (BleGattEror)
    REBOOT = 4, //Defined below (RebootReason)
    VENDOR = 5, //A placeholder for vendor/3rd party error codes, feel free to use this category for your own implementation of custom error types
};

//There are a number of different error types (by convention)
// FATAL_ are errors that must never occur, mostly implementation faults or critical connectivity issues
// WARN_ is for errors that might occur from time to time and should be investigated if they happen too often
// INFO_ is for noting important events that do not occur often
// COUNT_ is for errors or information that might happen often but should not pass a certain threshold
enum class CustomErrorTypes : u8 {
    FATAL_BLE_GATTC_EVT_TIMEOUT_FORCED_US = 1,
    INFO_TRYING_CONNECTION_SUSTAIN = 2,
    WARN_CONNECTION_SUSTAIN_FAILED_TO_ESTABLISH = 3,
    COUNT_CONNECTION_SUCCESS = 4,
    COUNT_HANDSHAKE_DONE = 5,
    //REBOOT=6, deprectated, use error type REBOOT
    WARN_HANDSHAKE_TIMEOUT = 7,
    WARN_CM_FAIL_NO_SPOT = 8,
    FATAL_QUEUE_NUM_MISMATCH = 9,
    WARN_GATT_WRITE_ERROR = 10,
    WARN_TX_WRONG_DATA = 11,
    DEPRECATED_WARN_RX_WRONG_DATA = 12,     // Use COUNT_WARN_RX_WRONG_DATA instead
    WARN_CLUSTER_UPDATE_FLOW_MISMATCH = 13,
    WARN_VITAL_PRIO_QUEUE_FULL = 14,
    COUNT_NO_PENDING_CONNECTION = 15,
    FATAL_HANDLE_PACKET_SENT_ERROR = 16,
    COUNT_DROPPED_PACKETS = 17,
    COUNT_SENT_PACKETS_RELIABLE = 18,
    COUNT_SENT_PACKETS_UNRELIABLE = 19,
    INFO_ERRORS_REQUESTED = 20,
    INFO_CONNECTION_SUSTAIN_SUCCESS = 21,
    COUNT_JOIN_ME_RECEIVED = 22,
    WARN_CONNECT_AS_MASTER_NOT_POSSIBLE = 23,
    FATAL_PENDING_NOT_CLEARED = 24,
    FATAL_PROTECTED_PAGE_ERASE = 25,
    INFO_IGNORING_CONNECTION_SUSTAIN = 26,
    INFO_IGNORING_CONNECTION_SUSTAIN_LEAF = 27,
    COUNT_GATT_CONNECT_FAILED = 28,
    FATAL_PACKET_PROCESSING_FAILED = 29,
    FATAL_PACKET_TOO_BIG = 30,
    COUNT_HANDSHAKE_ACK1_DUPLICATE = 31,
    COUNT_HANDSHAKE_ACK2_DUPLICATE = 32,
    COUNT_ENROLLMENT_NOT_SAVED = 33,
    COUNT_FLASH_OPERATION_ERROR = 34,
    FATAL_WRONG_FLASH_STORAGE_COMMAND = 35,
    FATAL_ABORTED_FLASH_TRANSACTION = 36,
    FATAL_PACKETQUEUE_PACKET_TOO_BIG = 37,
    FATAL_NO_RECORDSTORAGE_SPACE_LEFT = 38,
    FATAL_RECORD_CRC_WRONG = 39,
    COUNT_3RD_PARTY_TIMEOUT = 40,
    FATAL_CONNECTION_ALLOCATOR_OUT_OF_MEMORY = 41,
    FATAL_CONNECTION_REMOVED_WHILE_TIME_SYNC = 42,
    FATAL_COULD_NOT_RETRIEVE_CAPABILITIES = 43,
    FATAL_INCORRECT_HOPS_TO_SINK = 44,
    WARN_SPLIT_PACKET_MISSING = 45,
    WARN_SPLIT_PACKET_NOT_IN_MTU = 46,
    FATAL_DFU_FLASH_OPERATION_FAILED = 47,
    WARN_RECORD_STORAGE_ERASE_CYCLES_HIGH = 48,
    FATAL_RECORD_STORAGE_ERASE_CYCLES_HIGH = 49,
    FATAL_RECORD_STORAGE_COULD_NOT_FIND_SWAP_PAGE = 50,
    FATAL_MTU_UPGRADE_FAILED = 51,
    WARN_ENROLLMENT_ERASE_FAILED = 52,
    FATAL_RECORD_STORAGE_UNLOCK_FAILED = 53,
    WARN_ENROLLMENT_LOCK_DOWN_FAILED = 54,
    WARN_CONNECTION_SUSTAIN_FAILED = 55,
    COUNT_EMERGENCY_CONNECTION_CANT_DISCONNECT_ANYBODY = 56,
    INFO_EMERGENCY_DISCONNECT_SUCCESSFUL = 57,
    WARN_COULD_NOT_CREATE_EMERGENCY_DISCONNECT_VALIDATION_CONNECTION = 58,
    WARN_UNEXPECTED_REMOVAL_OF_EMERGENCY_DISCONNECT_VALIDATION_CONNECTION = 59,
    WARN_EMERGENCY_DISCONNECT_PARTNER_COULDNT_DISCONNECT_ANYBODY = 60,
    WARN_REQUEST_PROPOSALS_UNEXPECTED_LENGTH = 61,
    WARN_REQUEST_PROPOSALS_TOO_LONG = 62,
    FATAL_SENSOR_PINS_NOT_DEFINED_IN_BOARD_ID = 63,
    COUNT_ACCESS_TO_REMOVED_CONNECTION = 64,
    WARN_ADVERTISING_CONTROLLER_DEACTIVATE_FAILED = 65,
    WARN_GAP_SEC_INFO_REPLY_FAILED = 66,
    WARN_GAP_SEC_DISCONNECT_FAILED = 67,
    FATAL_FAILED_TO_REGISTER_APPLICATION_INTERRUPT_HANDLER = 68,
    FATAL_FAILED_TO_REGISTER_MAIN_CONTEXT_HANDLER = 69,
    FATAL_SIG_PROVISIONING_FAILED = 70,
    FATAL_SIG_ELEMENT_CREATION_FAILED = 71,
    FATAL_SIG_STORAGE_ERROR = 72,
    COUNT_RECEIVED_INVALID_FRUITY_MESH_PACKET = 73,
    FATAL_NEW_CHUNK_TOO_SMALL_FOR_MESSAGE = 74,
    FATAL_NO_CHUNK_FOR_NEW_CONNECTION = 75,
    COUNT_GENERATED_SPLIT_PACKETS = 76,
    COUNT_RECEIVED_SPLIT_OVER_MESH_ACCESS = 77,
    FATAL_ILLEGAL_PROCCESS_BUFFER_LENGTH = 78,
    FATAL_QUEUE_ORIGINS_FULL = 79,
    COUNT_TOTAL_RECEIVED_MESSAGES = 80, // total number of received messages by node (also includes the relay packets)
    COUNT_RECEIVED_MESSAGES = 81, //number of received messages meant to be for that particular node
    COUNT_UART_RX_ERROR = 82,
    INFO_UNUSED_STACK_BYTES = 83,
    FATAL_CONNECTION_REMOVED_WHILE_ENROLLED_NODES_SYNC = 84,
    INFO_UPTIME_RELATIVE = 85,
    INFO_UPTIME_ABSOLUTE = 86,
    COUNT_WARN_RX_WRONG_DATA = 87,
    WATCHDOG_REBOOT = 88,
    INFO_LICENSE_CHECK = 89,
    FATAL_LICENSE_MIGRATION_FAILED = 90,
    // When adding new error type please also add in frutyapi in BeaconErrorMessage.java
};

struct ErrorLogEntry {
    LoggingError errorType;
    u32 errorCode;
    u32 extraInfo;
    u32 timestamp;
};

static_assert(sizeof(ErrorLogEntry) <= 16, "ErrorLogEntry must be at most 16 bytes long");

class ErrorLog
{
public:
    static constexpr std::size_t STORAGE_SIZE = 100;

private:
    SimpleQueue<ErrorLogEntry, STORAGE_SIZE> storage;

public:
    ErrorLog();

    NO_DISCARD std::size_t Size() const
    {
        return storage.GetAmountOfElements();
    }

    void Reset();

    void PushError(const ErrorLogEntry &entry);

    void PushCount(const ErrorLogEntry &entry);

    NO_DISCARD bool PopEntry(ErrorLogEntry &entry);

    template<typename Predicate>
    NO_DISCARD ErrorLogEntry *FindByPredicate(Predicate predicate)
    {
        return storage.FindByPredicate(predicate);
    }
};
