// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "region_backing.h"
#include "partition_filesystem.h"
#include "nca.h"
#include "rom_filesystem.h"
#include "directory.h"

namespace skyline::vfs {
    NCA::NCA(const std::shared_ptr<vfs::Backing> &backing) : backing(backing) {
        backing->Read(&header);

        if (header.magic != util::MakeMagic<u32>("NCA3"))
            throw exception("Attempting to load an encrypted or invalid NCA");

        contentType = header.contentType;

        for (size_t i = 0; i < header.sectionHeaders.size(); i++) {
            auto &sectionHeader = header.sectionHeaders.at(i);
            auto &sectionEntry = header.fsEntries.at(i);

            if (sectionHeader.fsType == NcaSectionFsType::PFS0 && sectionHeader.hashType == NcaSectionHashType::HierarchicalSha256)
                ReadPfs0(sectionHeader, sectionEntry);
            else if (sectionHeader.fsType == NcaSectionFsType::RomFs && sectionHeader.hashType == NcaSectionHashType::HierarchicalIntegrity)
                ReadRomFs(sectionHeader, sectionEntry);
        }
    }

    void NCA::ReadPfs0(const NcaSectionHeader &header, const NcaFsEntry &entry) {
        size_t offset = static_cast<size_t>(entry.startOffset) * constant::MediaUnitSize + header.sha256HashInfo.pfs0Offset;
        size_t size = constant::MediaUnitSize * static_cast<size_t>(entry.endOffset - entry.startOffset);

        auto pfs = std::make_shared<PartitionFileSystem>(std::make_shared<RegionBacking>(backing, offset, size));

        if (contentType == NcaContentType::Program) {
            // An ExeFS must always contain an NPDM and a main NSO, whereas the logo section will always contain a logo and a startup movie
            if (pfs->FileExists("main") && pfs->FileExists("main.npdm"))
                exeFs = std::move(pfs);
            else if (pfs->FileExists("NintendoLogo.png") && pfs->FileExists("StartupMovie.gif"))
                logo = std::move(pfs);
        } else if (contentType == NcaContentType::Meta) {
            cnmt = std::move(pfs);
        }
    }

    void NCA::ReadRomFs(const NcaSectionHeader &header, const NcaFsEntry &entry) {
        size_t offset = static_cast<size_t>(entry.startOffset) * constant::MediaUnitSize + header.integrityHashInfo.levels.back().offset;
        size_t size = header.integrityHashInfo.levels.back().size;

        romFs = std::make_shared<RegionBacking>(backing, offset, size);
    }
}