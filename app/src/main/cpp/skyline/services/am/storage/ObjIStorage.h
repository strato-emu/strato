// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>
#include "IStorage.h"

namespace skyline::service::am {
    /**
     * @brief ObjIStorage is an IStorage backed by a trivially copyable object
     */
    template<typename T> requires std::is_trivially_copyable_v<T>
    class ObjIStorage : public IStorage {
      private:
        T obj;

      public:
        ObjIStorage(const DeviceState &state, ServiceManager &manager, T &&obj) :  IStorage(state, manager, true), obj(obj) {}

        ~ObjIStorage() override = default;

        span<u8> GetSpan() override {
            return {reinterpret_cast<u8 *>(&obj), sizeof(T)};
        };
    };
}
