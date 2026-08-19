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


#include <set>
#include <string>
#include <typeinfo>
#include <exception>
#include <typeindex>
#include "debugbreak.h"

//We accumulate exception for one simulation step when it is disabled and cleared at the start of next simulation step
extern void LogThrownCherrySimException(std::type_index index);

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
CREATEEXCEPTIONINHERITING(NotLoggedInException                                      , IllegalArgumentException);
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
CREATEEXCEPTIONINHERITING(DivisionByZeroException                                   , IllegalArgumentException);

CREATEEXCEPTION(IllegalStateException);
CREATEEXCEPTIONINHERITING(ZeroOnNonPodTypeException                    , IllegalStateException);
CREATEEXCEPTIONINHERITING(UartNotSetException                          , IllegalStateException);
CREATEEXCEPTIONINHERITING(ReceivedWrongTimeSyncPacketException         , IllegalStateException);
CREATEEXCEPTIONINHERITING(ModuleAllocatorMemoryAlreadySetException     , IllegalStateException);
CREATEEXCEPTIONINHERITING(ErrorCodeUnknownException                    , IllegalStateException);
CREATEEXCEPTIONINHERITING(RecordStorageIsLockedDownException           , IllegalStateException);
CREATEEXCEPTIONINHERITING(StackOverflowException                       , IllegalStateException);
CREATEEXCEPTIONINHERITING(AccessToRemovedConnectionException           , IllegalStateException);
CREATEEXCEPTIONINHERITING(InternalTerminalCommandErrorException        , IllegalStateException);
CREATEEXCEPTIONINHERITING(FileException                                , IllegalStateException);
CREATEEXCEPTIONINHERITING(SigProvisioningFailedException               , IllegalStateException);
CREATEEXCEPTIONINHERITING(SigCreateElementFailedException              , IllegalStateException);
CREATEEXCEPTIONINHERITING(IncorrectHopsToSinkException                 , IllegalStateException);
CREATEEXCEPTIONINHERITING(JsonParseException                           , IllegalStateException);
CREATEEXCEPTIONINHERITING(InvalidTerminalIdException                   , IllegalStateException);
CREATEEXCEPTIONINHERITING(NodeIdNotFoundException                      , IllegalStateException);
CREATEEXCEPTIONINHERITING(TerminalIdNotFoundException                  , IllegalStateException);
CREATEEXCEPTIONINHERITING(MulipleNodesHaveSameNodeIdException          , IllegalStateException);
CREATEEXCEPTIONINHERITING(MulipleNodesHaveSameNodeAndNetworkIdException, IllegalStateException);
CREATEEXCEPTIONINHERITING(NoSinkConfiguredForMeshGatewayConfigurationException, IllegalStateException);
CREATEEXCEPTIONINHERITING(ImageNotLicenseCompatibleException           , IllegalStateException);
CREATEEXCEPTIONINHERITING(LicenseMigrationFailedException              , IllegalStateException);
CREATEEXCEPTIONINHERITING(DfuImageCrcException                         , IllegalStateException);
CREATEEXCEPTIONINHERITING(TransformationFailedException                , IllegalStateException);
CREATEEXCEPTIONINHERITING(PreambleNotAtStartException                  , IllegalStateException);
CREATEEXCEPTIONINHERITING(DoubleDataOffsetException                    , IllegalStateException);
CREATEEXCEPTIONINHERITING(PinAlreadyInitializedException               , IllegalStateException);

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
CREATEEXCEPTION(MessageShouldNotOccurException)
CREATEEXCEPTION(WatchdogTriggeredException);
CREATEEXCEPTION(SafeBootTriggeredException);
CREATEEXCEPTION(MessageTypeInvalidException);
CREATEEXCEPTION(IllegalAdvertismentStateException);
CREATEEXCEPTION(MalformedPacketException);
CREATEEXCEPTION(NotImplementedException);
CREATEEXCEPTION(NotUsedException); //Not a typical use-case but can happen
CREATEEXCEPTION(CorruptOrOutdatedSavefile);
CREATEEXCEPTION(ZeroTimeoutNotSupportedException);
CREATEEXCEPTION(ErrorLoggedException);
CREATEEXCEPTION(InterruptDeadlockException);
CREATEEXCEPTION(DeviceNotAvailableException);
CREATEEXCEPTION(LicenseNotCreatedException);
CREATEEXCEPTION(LicenseNotValidException);
CREATEEXCEPTION(BaudRateMismatchException);
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
        printf("Exception occurred: " #T " " __FILE__ " %d\n", __LINE__); \
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
            printf("Exception occurred but ignored: " #T " " __FILE__ " %d\n", __LINE__); \
            LogThrownCherrySimException(typeid(T)); \
        } \
    }


