// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <vfs/nca.h>
#include <vfs/rom_filesystem.h>
#include <vfs/partition_filesystem.h>
#include <crypto/key_store.h>
#include "loader.h"

namespace skyline::loader {
    /**
     * @brief The NspLoader class consolidates all the data in an NSP providing a simple way to load an application and access its metadata
     * @url https://switchbrew.org/wiki/NCA_Format#PFS0
     */
    class NspLoader : public Loader {
      private:
        std::shared_ptr<vfs::PartitionFileSystem> nsp; //!< A shared pointer to the NSP's PFS0
        std::shared_ptr<vfs::RomFileSystem> controlRomFs; //!< A shared pointer to the control NCA's RomFS
        std::optional<vfs::NCA> programNca; //!< The main program NCA within the NSP
        std::optional<vfs::NCA> controlNca; //!< The main control NCA within the NSP

      public:
        NspLoader(const std::shared_ptr<vfs::Backing> &backing, const std::shared_ptr<crypto::KeyStore> &keyStore);

        std::vector<u8> GetIcon();

        void* LoadProcessData(const std::shared_ptr<kernel::type::KProcess> process, const DeviceState &state);
    };
}
