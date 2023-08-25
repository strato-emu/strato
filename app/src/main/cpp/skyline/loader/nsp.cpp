// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include <vfs/ticket.h>
#include "nca.h"
#include "nsp.h"
#include "vfs/patch_manager.h"

namespace skyline::loader {
    static void ExtractTickets(const std::shared_ptr<vfs::PartitionFileSystem>& dir, const std::shared_ptr<crypto::KeyStore> &keyStore) {
        std::vector<vfs::Ticket> tickets;

        auto dirContent{dir->OpenDirectory("", {false, true})};
        for (const auto &entry : dirContent->Read()) {
            if (entry.name.substr(entry.name.find_last_of('.') + 1) == "tik")
                tickets.emplace_back(dir->OpenFile(entry.name));
        }

        for (auto ticket : tickets) {
            auto titleKey{span(ticket.titleKeyBlock).subspan(0, 16).as<crypto::KeyStore::Key128>()};
            keyStore->PopulateTitleKey(ticket.rightsId, titleKey);
        }
    }

    NspLoader::NspLoader(const std::shared_ptr<vfs::Backing> &backing, const std::shared_ptr<crypto::KeyStore> &keyStore) : nsp(std::make_shared<vfs::PartitionFileSystem>(backing)) {
        ExtractTickets(nsp, keyStore);

        auto root{nsp->OpenDirectory("", {false, true})};
        for (const auto &entry : root->Read()) {
            if (entry.name.substr(entry.name.find_last_of('.') + 1) != "nca")
                continue;

            try {
                auto nca{vfs::NCA(nsp->OpenFile(entry.name), keyStore)};

                if (nca.contentType == vfs::NCAContentType::Program && nca.romFs != nullptr && nca.exeFs != nullptr)
                    programNca = std::move(nca);
                else if (nca.contentType == vfs::NCAContentType::Control && nca.romFs != nullptr)
                    controlNca = std::move(nca);
                else if (nca.contentType == vfs::NCAContentType::Meta)
                    metaNca = std::move(nca);
                else if (nca.contentType == vfs::NCAContentType::PublicData)
                    publicNca = std::move(nca);
            } catch (const loader_exception &e) {
                throw loader_exception(e.error);
            } catch (const std::exception &e) {
                continue;
            }
        }

        if (programNca)
            romFs = programNca->romFs;

        if (controlNca) {
            controlRomFs = std::make_shared<vfs::RomFileSystem>(controlNca->romFs);
            nacp.emplace(controlRomFs->OpenFile("control.nacp"));
        }

        if (metaNca)
            cnmt = vfs::CNMT(metaNca->cnmt);
    }

    void *NspLoader::LoadProcessData(const std::shared_ptr<kernel::type::KProcess> &process, const DeviceState &state) {
        if (state.updateLoader) {
            auto patchManager{std::make_shared<vfs::PatchManager>()};
            programNca->exeFs = patchManager->PatchExeFS(state, programNca->exeFs);
        }

        process->npdm = vfs::NPDM(programNca->exeFs->OpenFile("main.npdm"));
        return NcaLoader::LoadExeFs(this, programNca->exeFs, process, state);
    }

    std::vector<u8> NspLoader::GetIcon(language::ApplicationLanguage language) {
        if (controlRomFs == nullptr)
            return std::vector<u8>();

        std::shared_ptr<vfs::Backing> icon{};

        auto iconName{fmt::format("icon_{}.dat", language::ToString(language))};
        if (!(icon = controlRomFs->OpenFileUnchecked(iconName, {true, false, false}))) {
            iconName = fmt::format("icon_{}.dat", language::ToString(nacp->GetFirstSupportedTitleLanguage()));
            icon = controlRomFs->OpenFileUnchecked(iconName, {true, false, false});
        }

        if (icon == nullptr)
            return std::vector<u8>();

        std::vector<u8> buffer(icon->size);
        icon->Read(buffer);
        return buffer;
    }
}
