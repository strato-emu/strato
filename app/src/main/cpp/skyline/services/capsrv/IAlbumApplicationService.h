// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::capsrv {
    /**
     * @url https://switchbrew.org/wiki/Capture_services#caps:u
     */
    class IAlbumApplicationService : public BaseService {
      private:
        /**
         * @url https://switchbrew.org/wiki/Capture_services#ContentType
         */
        enum class ContentType : u8 {
            Screenshot = 0,
            Movie = 1,
            ExtraMovie = 3,
        };

      public:
        IAlbumApplicationService(const DeviceState &state, ServiceManager &manager);

        /**
         * @url https://switchbrew.org/wiki/Capture_services#SetShimLibraryVersion
         */
        Result SetShimLibraryVersion(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/Capture_services#GetAlbumFileList0AafeAruidDeprecated
         */
        Result GetAlbumFileList0AafeAruidDeprecated(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/Capture_services#GetAlbumFileList3AaeAruid
         */
        Result GetAlbumFileList3AaeAruid(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x20, IAlbumApplicationService, SetShimLibraryVersion),
            SFUNC(0x66, IAlbumApplicationService, GetAlbumFileList0AafeAruidDeprecated),
            SFUNC(0x8E, IAlbumApplicationService, GetAlbumFileList3AaeAruid)
        )
    };
}
