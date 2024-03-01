// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright © 2023 Strato Team and Contributors (https://github.com/strato-emu/)

#include <os.h>
#include <vfs/nca.h>
#include "patch_manager.h"
#include "region_backing.h"

namespace skyline::vfs {
    PatchManager::PatchManager() {}

    std::shared_ptr<FileSystem> PatchManager::PatchExeFS(const DeviceState &state, std::shared_ptr<FileSystem> exefs) {
        auto updateProgramNCA{state.updateLoader->programNca};
        return updateProgramNCA->exeFs;
    }

    std::shared_ptr<vfs::Backing> PatchManager::PatchRomFS(const DeviceState &state, std::optional<vfs::NCA> nca, u64 ivfcOffset) {
        auto newNca{std::make_shared<vfs::NCA>(nca, state.os->keyStore, state.loader->programNca->romFs, ivfcOffset)};
        return newNca->romFs;
    }
}
