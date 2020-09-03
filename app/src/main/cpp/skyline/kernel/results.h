// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>

namespace skyline::kernel::result {
    constexpr Result OutOfSessions(1, 7);
    constexpr Result InvalidArgument(1, 14);
    constexpr Result NotImplemented(1, 33);
    constexpr Result StopProcessingException(1, 54);
    constexpr Result NoSynchronizationObject(1, 57);
    constexpr Result TerminationRequested(1, 59);
    constexpr Result NoEvent(1, 70);
    constexpr Result InvalidSize(1, 101);
    constexpr Result InvalidAddress(1, 102);
    constexpr Result OutOfResource(1, 103);
    constexpr Result OutOfMemory(1, 104);
    constexpr Result OutOfHandles(1, 105);
    constexpr Result InvalidCurrentMemory(1, 106);
    constexpr Result InvalidNewMemoryPermission(1, 108);
    constexpr Result InvalidMemoryRegion(1, 110);
    constexpr Result InvalidPriority(1, 112);
    constexpr Result InvalidCoreId(1, 113);
    constexpr Result InvalidHandle(1, 114);
    constexpr Result InvalidPointer(1, 115);
    constexpr Result InvalidCombination(1, 116);
    constexpr Result TimedOut(1, 117);
    constexpr Result Cancelled(1, 118);
    constexpr Result OutOfRange(1, 119);
    constexpr Result InvalidEnumValue(1, 120);
    constexpr Result NotFound(1, 121);
    constexpr Result Busy(1, 122);
    constexpr Result SessionClosed(1, 123);
    constexpr Result NotHandled(1, 124);
    constexpr Result InvalidState(1, 125);
    constexpr Result ReservedUsed(1, 126);
    constexpr Result NotSupported(1, 127);
    constexpr Result Debug(1, 128);
    constexpr Result NoThread(1, 129);
    constexpr Result UnknownThread(1, 130);
    constexpr Result PortClosed(1, 131);
    constexpr Result LimitReached(1, 132);
    constexpr Result InvalidMemoryPool(1, 133);
    constexpr Result ReceiveListBroken(1, 258);
    constexpr Result OutOfAddressSpace(1, 259);
    constexpr Result MessageTooLarge(1, 260);
    constexpr Result InvalidProcessId(1, 517);
    constexpr Result InvalidThreadId(1, 518);
    constexpr Result InvalidId(1, 519);
    constexpr Result ProcessTerminated(1, 520);
}