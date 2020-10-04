// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <nce.h>
#include <os.h>
#include <kernel/memory.h>
#include "nso.h"
#include "nca.h"

namespace skyline::loader {
    NcaLoader::NcaLoader(const std::shared_ptr<vfs::Backing> &backing, const std::shared_ptr<crypto::KeyStore> &keyStore) : nca(backing, keyStore) {
        if (nca.exeFs == nullptr)
            throw exception("Only NCAs with an ExeFS can be loaded directly");
    }

    void NcaLoader::LoadExeFs(const std::shared_ptr<vfs::FileSystem> &exeFs, const std::shared_ptr<kernel::type::KProcess> process, const DeviceState &state) {
        if (exeFs == nullptr)
            throw exception("Cannot load a null ExeFS");

        auto nsoFile{exeFs->OpenFile("rtld")};
        if (nsoFile == nullptr)
            throw exception("Cannot load an ExeFS that doesn't contain rtld");

        auto loadInfo{NsoLoader::LoadNso(nsoFile, process, state)};
        u64 offset{loadInfo.size};
        u8* base{loadInfo.base};

        state.logger->Info("Loaded nso 'rtld' at 0x{:X}", fmt::ptr(base));

        for (const auto &nso : {"main", "subsdk0", "subsdk1", "subsdk2", "subsdk3", "subsdk4", "subsdk5", "subsdk6", "subsdk7", "sdk"}) {
            nsoFile = exeFs->OpenFile(nso);

            if (nsoFile == nullptr)
                continue;

            loadInfo = NsoLoader::LoadNso(nsoFile, process, state, offset);
            state.logger->Info("Loaded nso '{}' at 0x{:X}", nso, fmt::ptr(base + offset));
            offset += loadInfo.size;
        }

        state.os->memory.InitializeRegions(base, offset, memory::AddressSpaceType::AddressSpace39Bit);
    }

    void NcaLoader::LoadProcessData(const std::shared_ptr<kernel::type::KProcess> process, const DeviceState &state) {
        LoadExeFs(nca.exeFs, process, state);
    }
}
