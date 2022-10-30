// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <unordered_set>
#include <services/serviceman.h>

namespace skyline::service::ro {
    namespace result {
        constexpr Result AlreadyLoaded{22, 3};
        constexpr Result InvalidNro{22, 4};
        constexpr Result InvalidNrr{22, 6};
        constexpr Result InvalidAddress{22, 1025};
        constexpr Result InvalidSize{22, 1026};
    }

    /**
     * @brief IRoInterface or ldr:ro is used by applications to dynamically load nros
     * @url https://switchbrew.org/wiki/RO_services#LoadModule
     */
    class IRoInterface : public BaseService {
      private:
        enum class NrrKind : u8 {
            User = 0,
            JitPlugin = 1
        };

        std::unordered_set<std::array<u8, 0x20>, util::ObjectHash<std::array<u8, 0x20>>> loadedNros{};

        Result RegisterModuleInfoImpl(u64 nrrAddress, u64 nrrSize, NrrKind nrrKind);

      public:
        IRoInterface(const DeviceState &state, ServiceManager &manager);

        /**
         * @url https://switchbrew.org/wiki/RO_services#LoadModule
         */
        Result LoadModule(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/RO_services#UnloadModule
         */
        Result UnloadModule(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/RO_services#RegisterModuleInfo
         */
        Result RegisterModuleInfo(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/RO_services#UnregisterModuleInfo
         */
        Result UnregisterModuleInfo(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/RO_services#Initialize
         */
        Result RegisterProcessHandle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/RO_services#RegisterProcessModuleInfo
         */
        Result RegisterProcessModuleInfo(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

      SERVICE_DECL(
          SFUNC(0x0, IRoInterface, LoadModule),
          SFUNC(0x1, IRoInterface, UnloadModule),
          SFUNC(0x2, IRoInterface, RegisterModuleInfo),
          SFUNC(0x3, IRoInterface, UnregisterModuleInfo),
          SFUNC(0x4, IRoInterface, RegisterProcessHandle),
          SFUNC(0xA, IRoInterface, RegisterProcessModuleInfo)
      )
    };
}
