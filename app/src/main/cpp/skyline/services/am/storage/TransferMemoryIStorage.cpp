// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "TransferMemoryIStorage.h"

#include <utility>

namespace skyline::service::am {
    TransferMemoryIStorage::TransferMemoryIStorage(const DeviceState &state, ServiceManager &manager, std::shared_ptr<kernel::type::KTransferMemory> transferMemory, bool writable) : transferMemory(std::move(transferMemory)), IStorage(state, manager, writable) {}

    span<u8> TransferMemoryIStorage::GetSpan() {
        return transferMemory->host;
    }

    TransferMemoryIStorage::~TransferMemoryIStorage() = default;
}
