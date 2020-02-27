////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2020 M-Way Solutions GmbH
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
CREATEEXCEPTIONINHERITING(TooManyArgumentsException                                 , IllegalArgumentException);
CREATEEXCEPTIONINHERITING(IndexOutOfBoundsException                                 , IllegalArgumentException);
CREATEEXCEPTIONINHERITING(CommandTooLongException                                   , IllegalArgumentException);
CREATEEXCEPTIONINHERITING(NotANumberStringException                                 , IllegalArgumentException);
CREATEEXCEPTIONINHERITING(NumberStringNotInRangeException                           , IllegalArgumentException);
CREATEEXCEPTIONINHERITING(MoreThanOneTerminalCommandHandlerReactedOnCommandException, IllegalArgumentException);

CREATEEXCEPTION(IllegalStateException);
CREATEEXCEPTIONINHERITING(ZeroOnNonPodTypeException               , IllegalStateException);
CREATEEXCEPTIONINHERITING(UartNotSetException                     , IllegalStateException);
CREATEEXCEPTIONINHERITING(ReceivedWrongTimeSyncPaketException     , IllegalStateException);
CREATEEXCEPTIONINHERITING(CommandbufferAlreadyInUseException      , IllegalStateException);
CREATEEXCEPTIONINHERITING(ModuleAllocatorMemoryAlreadySetException, IllegalStateException);
CREATEEXCEPTIONINHERITING(ErrorCodeUnknownException               , IllegalStateException);
CREATEEXCEPTIONINHERITING(RecordStorageIsLockedDownException      , IllegalStateException);
CREATEEXCEPTIONINHERITING(StackOverflowException                  , IllegalStateException);

CREATEEXCEPTION(BufferException);
CREATEEXCEPTIONINHERITING(TriedToReadEmptyBufferException         , BufferException);
CREATEEXCEPTIONINHERITING(BufferTooSmallException                 , BufferException);
CREATEEXCEPTIONINHERITING(TooManyTerminalCommandListenersException, BufferException);
CREATEEXCEPTIONINHERITING(TooManyTerminalJsonListenersException   , BufferException);
CREATEEXCEPTIONINHERITING(TooManyModulesException                 , BufferException);
CREATEEXCEPTIONINHERITING(RequiredFlashTooBigException            , BufferException);
CREATEEXCEPTIONINHERITING(DataToCacheTooBigException              , BufferException);
CREATEEXCEPTIONINHERITING(PacketStatBufferSizeNotEnough, BufferException);

CREATEEXCEPTION(PaketException);
CREATEEXCEPTIONINHERITING(PaketTooSmallException           , PaketException);
CREATEEXCEPTIONINHERITING(PaketTooBigException             , PaketException);
CREATEEXCEPTIONINHERITING(IllegalSenderException           , PaketException);
CREATEEXCEPTIONINHERITING(GotUnsupportedActionTypeException, PaketException);
CREATEEXCEPTIONINHERITING(SplitMissingException            , PaketException);
CREATEEXCEPTIONINHERITING(PacketTooBigException            , PaketException);
CREATEEXCEPTIONINHERITING(SplitNotInMTUException           , PaketException);

CREATEEXCEPTION(NodeDidNotRestartException);
CREATEEXCEPTION(BLEStackError);
CREATEEXCEPTION(HardfaultException);
CREATEEXCEPTION(NonCompatibleDataTypeException);
CREATEEXCEPTION(OutOfMemoryException);
CREATEEXCEPTION(MemoryCorruptionException);
CREATEEXCEPTION(NotFromThisAllocatorException);
CREATEEXCEPTION(TimeoutException);
CREATEEXCEPTION(WatchdogTriggeredException);
CREATEEXCEPTION(SafeBootTriggeredException);
CREATEEXCEPTION(MessageTypeInvalidException);
CREATEEXCEPTION(IllegalAdvertismentStateException);
CREATEEXCEPTION(MalformedPaketException);
CREATEEXCEPTION(NotImplementedException);
CREATEEXCEPTION(CorruptOrOutdatedSavefile);
CREATEEXCEPTION(ZeroTimeoutNotSupportedException);
//LCOV_EXCL_STOP debug code

#undef CREATEEXCEPTION //Exceptions must be created above!
#undef CREATEEXCEPTIONINHERITING

namespace Exceptions {

	void disableExceptionByIndex(std::type_index index);
	void enableExceptionByIndex(std::type_index index);
	bool isExceptionEnabledByIndex(std::type_index index);

	template<typename T>
	bool isExceptionEnabled() {
		return isExceptionEnabledByIndex(std::type_index(typeid(T)));
	}

	bool getDebugBreakOnException();
	class DisableDebugBreakOnException {
	public:
		DisableDebugBreakOnException();
		~DisableDebugBreakOnException();
	};

	template<typename T>
	class ExceptionDisabler {
	public:
		ExceptionDisabler() {
			disableExceptionByIndex(std::type_index(typeid(T)));
		}

		~ExceptionDisabler() {
			enableExceptionByIndex(std::type_index(typeid(T)));
		}
	};
}

#define SIMEXCEPTIONFORCE(T) \
	{\
		printf("Exception occured: " #T " " __FILE__ " %d\n", __LINE__); \
		if(Exceptions::getDebugBreakOnException()) {\
			__debugbreak(); \
		}\
		throw T(); \
	}

#define SIMEXCEPTION(T) \
	{\
		if(Exceptions::isExceptionEnabled<T>()) {\
			SIMEXCEPTIONFORCE(T); \
		}\
		else \
		{ \
			printf("Exception occured but ignored: " #T " " __FILE__ " %d\n", __LINE__); \
		} \
	}


