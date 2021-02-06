// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <vfs/nca.h>
#include "loader.h"

namespace skyline::loader {
    /**
     * @brief The NcaLoader class allows loading an NCA's ExeFS through the Loader interface
     * @url https://switchbrew.org/wiki/NSO
     */
    class NcaLoader : public Loader {
      private:
        vfs::NCA nca; //!< The backing NCA of the loader

      public:
        NcaLoader(std::shared_ptr<vfs::Backing> backing, std::shared_ptr<crypto::KeyStore> keyStore);

        /**
         * @brief Loads an ExeFS into memory and processes it accordingly for execution
         * @param exefs A filesystem object containing the ExeFS filesystem to load into memory
         */
        static void *LoadExeFs(Loader *loader, const std::shared_ptr<vfs::FileSystem> &exefs, const std::shared_ptr<kernel::type::KProcess> &process, const DeviceState &state);

        void *LoadProcessData(const std::shared_ptr<kernel::type::KProcess> &process, const DeviceState &state) override;
    };
}
