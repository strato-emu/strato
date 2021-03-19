// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "base_proxy.h"

namespace skyline::service::am {
    /**
     * @brief IApplicationProxy returns handles to various services
     * @url https://switchbrew.org/wiki/Applet_Manager_services#IApplicationProxy
     */
    class IApplicationProxy : public BaseProxy {
      public:
        IApplicationProxy(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief Returns #IApplicationFunctions
         * @url https://switchbrew.org/wiki/Applet_Manager_services#IApplicationFunctions
         */
        Result GetApplicationFunctions(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC_BASE(0x0, IApplicationProxy, BaseProxy, GetCommonStateGetter),
            SFUNC_BASE(0x1, IApplicationProxy, BaseProxy, GetSelfController),
            SFUNC_BASE(0x2, IApplicationProxy, BaseProxy, GetWindowController),
            SFUNC_BASE(0x3, IApplicationProxy, BaseProxy, GetAudioController),
            SFUNC_BASE(0x4, IApplicationProxy, BaseProxy, GetDisplayController),
            SFUNC_BASE(0xB, IApplicationProxy, BaseProxy, GetLibraryAppletCreator),
            SFUNC(0x14, IApplicationProxy, GetApplicationFunctions),
            SFUNC_BASE(0x3E8, IApplicationProxy, BaseProxy, GetDebugFunctions)
        )
    };
}
