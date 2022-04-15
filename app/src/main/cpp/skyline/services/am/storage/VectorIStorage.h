// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>
#include "IStorage.h"

namespace skyline::service::am {
    /**
     * @brief VectorIStorage is an IStorage backed by a vector
     */
    class VectorIStorage : public IStorage {
      private:
        std::vector<u8> content;

      public:
        VectorIStorage(const DeviceState &state, ServiceManager &manager, size_t size);

        VectorIStorage(const DeviceState &state, ServiceManager &manager, std::vector<u8> data);

        ~VectorIStorage() override;

        span<u8> GetSpan() override;
    };
}
