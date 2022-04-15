// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>
#include <kernel/types/KTransferMemory.h>
#include "IStorage.h"

namespace skyline::service::am {
    /**
     * @brief An IStorage backed by a transfer memory supplied by the guest
     */
    class TransferMemoryIStorage : public IStorage {
      private:
        std::shared_ptr<kernel::type::KTransferMemory> transferMemory;

      public:

        TransferMemoryIStorage(const DeviceState &state, ServiceManager &manager, std::shared_ptr<kernel::type::KTransferMemory> transferMemory, bool writable);

        ~TransferMemoryIStorage() override;

        span<u8> GetSpan() override;
    };
}
