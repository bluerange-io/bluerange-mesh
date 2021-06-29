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
#pragma once


#include <set>
#include <string>
#include <typeinfo>
#include <exception>
#include <typeindex>
#include "debugbreak.h"

struct FruityMeshException : public std::exception {};

//LCOV_EXCL_START Debug Code
#define CREATEEXCEPTION(T) struct T : FruityMeshException{};
#define CREATEEXCEPTIONINHERITING(T, Parent) struct T : Parent{};

CREATEEXCEPTION(IllegalArgumentException);
CREATEEXCEPTIONINHERITING(CommandNotFoundException                                  , IllegalArgumentException);
CREATEEXCEPTIONINHERITING(CRCMissingException                                       , IllegalArgumentException);
CREATEEXCEPTIONINHERITING(CRCInvalidException                                       , IllegalArgumentException);
CREATEEXCEPTIONINHERITING(WrongCommandParameterException                            , IllegalArgumentException);
CREATEEXCEPTIONINHERITING(TooFewParameterException                                  , IllegalArgumentException);
CREATEEXCEPTIONINHERITING(MessageTooLongException                                   , IllegalArgumentException);
CREATEEXCEPTIONINHERITING(MessageTooSmallException                                  , IllegalArgumentException);
CREATEEXCEPTIONINHERITING(TooManyArgumentsException                                 , IllegalArgumentException);
CREATEEXCEPTIONINHERITING(IndexOutOfBoundsException                                 , IllegalArgumentException);
CREATEEXCEPTIONINHERITING(CommandTooLongException                                   , IllegalArgumentException);
CREATEEXCEPTIONINHERITING(NotANumberStringException                                 , IllegalArgumentException);
CREATEEXCEPTIONINHERITING(NumberStringNotInRangeException                           , IllegalArgumentException);
CREATEEXCEPTIONINHERITING(MoreThanOneTerminalCommandHandlerReactedOnCommandException, IllegalArgumentException);
CREATEEXCEPTIONINHERITING(UnknownJsonEntryException                                 , IllegalArgumentException);
CREATEEXCEPTIONINHERITING(IllegalParameterException                                 , IllegalArgumentException);
CREATEEXCEPTIONINHERITING(NotAValidMessageTypeException                             , IllegalArgumentException);
CREATEEXCEPTIONINHERITING(IllegalFruityMeshPacketException                          , IllegalArgumentException);
CREATEEXCEPTIONINHERITING(IntegerUnderflowException                                 , IllegalArgumentException);

CREATEEXCEPTION(IllegalStateException);
CREATEEXCEPTIONINHERITING(ZeroOnNonPodTypeException                , IllegalStateException);
CREATEEXCEPTIONINHERITING(UartNotSetException                      , IllegalStateException);
CREATEEXCEPTIONINHERITING(ReceivedWrongTimeSyncPacketException     , IllegalStateException);
CREATEEXCEPTIONINHERITING(ModuleAllocatorMemoryAlreadySetException , IllegalStateException);
CREATEEXCEPTIONINHERITING(ErrorCodeUnknownException                , IllegalStateException);
CREATEEXCEPTIONINHERITING(RecordStorageIsLockedDownException       , IllegalStateException);
CREATEEXCEPTIONINHERITING(StackOverflowException                   , IllegalStateException);
CREATEEXCEPTIONINHERITING(AccessToRemovedConnectionException       , IllegalStateException);
CREATEEXCEPTIONINHERITING(InternalTerminalCommandErrorException    , IllegalStateException);
CREATEEXCEPTIONINHERITING(FileException                            , IllegalStateException);
CREATEEXCEPTIONINHERITING(SigProvisioningFailedException           , IllegalStateException);
CREATEEXCEPTIONINHERITING(SigCreateElementFailedException          , IllegalStateException);
CREATEEXCEPTIONINHERITING(IncorrectHopsToSinkException             , IllegalStateException);

CREATEEXCEPTION(BufferException);
CREATEEXCEPTIONINHERITING(TriedToReadEmptyBufferException         , BufferException);
CREATEEXCEPTIONINHERITING(BufferTooSmallException                 , BufferException);
CREATEEXCEPTIONINHERITING(TooManyTerminalCommandListenersException, BufferException);
CREATEEXCEPTIONINHERITING(TooManyTerminalJsonListenersException   , BufferException);
CREATEEXCEPTIONINHERITING(TooManyModulesException                 , BufferException);
CREATEEXCEPTIONINHERITING(RequiredFlashTooBigException            , BufferException);
CREATEEXCEPTIONINHERITING(DataToCacheTooBigException              , BufferException);
CREATEEXCEPTIONINHERITING(PacketStatBufferSizeNotEnough, BufferException);

CREATEEXCEPTION(PacketException);
CREATEEXCEPTIONINHERITING(PacketTooSmallException           , PacketException);
CREATEEXCEPTIONINHERITING(PacketTooBigException             , PacketException);
CREATEEXCEPTIONINHERITING(IllegalSenderException            , PacketException);
CREATEEXCEPTIONINHERITING(GotUnsupportedActionTypeException , PacketException);
CREATEEXCEPTIONINHERITING(SplitMissingException             , PacketException);
CREATEEXCEPTIONINHERITING(SplitNotInMTUException            , PacketException);

CREATEEXCEPTION(NodeDidNotRestartException);
CREATEEXCEPTION(BLEStackError);
CREATEEXCEPTION(HardfaultException);
CREATEEXCEPTION(NonCompatibleDataTypeException);
CREATEEXCEPTION(OutOfMemoryException);
CREATEEXCEPTION(AllocatorOutOfMemoryException);
CREATEEXCEPTION(MemoryCorruptionException);
CREATEEXCEPTION(NotFromThisAllocatorException);
CREATEEXCEPTION(TimeoutException);
CREATEEXCEPTION(WatchdogTriggeredException);
CREATEEXCEPTION(SafeBootTriggeredException);
CREATEEXCEPTION(MessageTypeInvalidException);
CREATEEXCEPTION(IllegalAdvertismentStateException);
CREATEEXCEPTION(MalformedPacketException);
CREATEEXCEPTION(NotImplementedException);
CREATEEXCEPTION(CorruptOrOutdatedSavefile);
CREATEEXCEPTION(ZeroTimeoutNotSupportedException);
CREATEEXCEPTION(ErrorLoggedException);
CREATEEXCEPTION(InterruptDeadlockException);
CREATEEXCEPTION(DeviceNotAvailableException);
//LCOV_EXCL_STOP debug code

#undef CREATEEXCEPTION //Exceptions must be created above!
#undef CREATEEXCEPTIONINHERITING

namespace Exceptions {

    void DisableExceptionByIndex(std::type_index index);
    void EnableExceptionByIndex(std::type_index index);
    bool IsExceptionEnabledByIndex(std::type_index index);

    template<typename T>
    bool IsExceptionEnabled() {
        return IsExceptionEnabledByIndex(std::type_index(typeid(T)));
    }

    bool GetDebugBreakOnException();
    class DisableDebugBreakOnException {
    public:
        DisableDebugBreakOnException();
        ~DisableDebugBreakOnException() noexcept(false);
    };

    template<typename T>
    class ExceptionDisabler {
    public:
        ExceptionDisabler() {
            DisableExceptionByIndex(std::type_index(typeid(T)));
        }

        ~ExceptionDisabler() {
            EnableExceptionByIndex(std::type_index(typeid(T)));
        }
    };
}

#define SIMEXCEPTIONFORCE(T) \
    {\
        printf("Exception occured: " #T " " __FILE__ " %d\n", __LINE__); \
        if(Exceptions::GetDebugBreakOnException()) {\
            debug_break(); \
        }\
        throw T(); \
    }

#define SIMEXCEPTION(T) \
    {\
        if(Exceptions::IsExceptionEnabled<T>()) {\
            SIMEXCEPTIONFORCE(T); \
        }\
        else \
        { \
            printf("Exception occured but ignored: " #T " " __FILE__ " %d\n", __LINE__); \
        } \
    }


