// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include "nca.h"
#include "nsp.h"

namespace skyline::loader {
    NspLoader::NspLoader(const std::shared_ptr<vfs::Backing> &backing, const std::shared_ptr<crypto::KeyStore> &keyStore) : nsp(std::make_shared<vfs::PartitionFileSystem>(backing)) {
        auto root{nsp->OpenDirectory("", {false, true})};

        for (const auto &entry : root->Read()) {
            if (entry.name.substr(entry.name.find_last_of(".") + 1) != "nca")
                continue;

            try {
                auto nca{vfs::NCA(nsp->OpenFile(entry.name), keyStore)};

                if (nca.contentType == vfs::NcaContentType::Program && nca.romFs != nullptr && nca.exeFs != nullptr)
                    programNca = std::move(nca);
                else if (nca.contentType == vfs::NcaContentType::Control && nca.romFs != nullptr)
                    controlNca = std::move(nca);
            } catch (const loader_exception &e) {
                throw loader_exception(e.error);
            } catch (const std::exception &e) {
                continue;
            }
        }

        if (!programNca || !controlNca)
            throw exception("Incomplete NSP file");

        romFs = programNca->romFs;
        controlRomFs = std::make_shared<vfs::RomFileSystem>(controlNca->romFs);
        nacp.emplace(controlRomFs->OpenFile("control.nacp"));
    }

    void* NspLoader::LoadProcessData(const std::shared_ptr<kernel::type::KProcess> process, const DeviceState &state) {
        process->npdm = vfs::NPDM(programNca->exeFs->OpenFile("main.npdm"));
        return NcaLoader::LoadExeFs(programNca->exeFs, process, state);
    }

    std::vector<u8> NspLoader::GetIcon() {
        if (romFs == nullptr)
            return std::vector<u8>();

        auto root{controlRomFs->OpenDirectory("", {false, true})};
        std::shared_ptr<vfs::Backing> icon;

        // Use the first icon file available
        for (const auto &entry : root->Read()) {
            if (entry.name.rfind("icon") == 0) {
                icon = controlRomFs->OpenFile(entry.name);
                break;
            }
        }

        if (icon == nullptr)
            return std::vector<u8>();

        std::vector<u8> buffer(icon->size);
        icon->Read(buffer);
        return buffer;
    }
}
