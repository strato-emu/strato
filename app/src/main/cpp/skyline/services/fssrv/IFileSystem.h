// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>

namespace skyline::service::fssrv {
    /**
     * @brief These are the possible types of the filesystem
     */
    enum class FsType {
        Nand, //!< The internal NAND storage
        SdCard, //!< The external SDCard storage
        GameCard, //!< The Game-Card of the inserted game (https://switchbrew.org/wiki/Gamecard)
    };

    /**
     * @brief IFileSystem is used to interact with a filesystem (https://switchbrew.org/wiki/Filesystem_services#IFileSystem)
     */
    class IFileSystem : public BaseService {
      public:
        FsType type; //!< The type of filesystem this class represents

        IFileSystem(FsType type, const DeviceState &state, ServiceManager &manager);
    };
}
