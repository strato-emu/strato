// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <nce.h>
#include <kernel/types/KProcess.h>
#include <vfs/nacp.h>
#include <vfs/region_backing.h>
#include "nro.h"

namespace skyline::loader {
    NroLoader::NroLoader(std::shared_ptr<vfs::Backing> pBacking) : backing(std::move(pBacking)) {
        header = backing->Read<NroHeader>();

        if (header.magic != util::MakeMagic<u32>("NRO0"))
            throw exception("Invalid NRO magic! 0x{0:X}", header.magic);

        // The homebrew asset section is appended to the end of an NRO file
        if (backing->size > header.size) {
            assetHeader = backing->Read<NroAssetHeader>(header.size);

            if (assetHeader.magic != util::MakeMagic<u32>("ASET"))
                throw exception("Invalid ASET magic! 0x{0:X}", assetHeader.magic);

            NroAssetSection &nacpHeader{assetHeader.nacp};
            nacp.emplace(std::make_shared<vfs::RegionBacking>(backing, header.size + nacpHeader.offset, nacpHeader.size));

            NroAssetSection &romFsHeader{assetHeader.romFs};
            romFs = std::make_shared<vfs::RegionBacking>(backing, header.size + romFsHeader.offset, romFsHeader.size);
        }
    }

    std::vector<u8> NroLoader::GetIcon(language::ApplicationLanguage language) {
        NroAssetSection &segmentHeader{assetHeader.icon};
        std::vector<u8> buffer(segmentHeader.size);

        backing->Read(buffer, header.size + segmentHeader.offset);
        return buffer;
    }

    std::vector<u8> NroLoader::GetSegment(const NroSegmentHeader &segment) {
        std::vector<u8> buffer(segment.size);

        backing->Read(buffer, segment.offset);
        return buffer;
    }

    void *NroLoader::LoadProcessData(const std::shared_ptr<kernel::type::KProcess> &process, const DeviceState &state) {
        Executable executable{};

        executable.text.contents = GetSegment(header.text);
        executable.text.offset = 0;

        executable.ro.contents = GetSegment(header.ro);
        executable.ro.offset = header.text.size;

        executable.data.contents = GetSegment(header.data);
        executable.data.offset = header.text.size + header.ro.size;

        executable.bssSize = header.bssSize;

        if (header.dynsym.offset > header.ro.offset && header.dynsym.offset + header.dynsym.size < header.ro.offset + header.ro.size && header.dynstr.offset > header.ro.offset && header.dynstr.offset + header.dynstr.size < header.ro.offset + header.ro.size) {
            executable.dynsym = {header.dynsym.offset, header.dynsym.size};
            executable.dynstr = {header.dynstr.offset, header.dynstr.size};
        }

        state.process->memory.InitializeVmm(memory::AddressSpaceType::AddressSpace39Bit);
        auto applicationName{nacp ? nacp->GetApplicationName(nacp->GetFirstSupportedTitleLanguage()) : ""};
        auto loadInfo{LoadExecutable(process, state, executable, 0, applicationName.empty() ? "main.nro" : applicationName + ".nro")};
        state.process->memory.InitializeRegions(span<u8>{loadInfo.base, loadInfo.size});

        return loadInfo.entry;
    }
}
