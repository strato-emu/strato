// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <nce.h>
#include <kernel/types/KProcess.h>
#include <vfs/nacp.h>
#include <vfs/region_backing.h>
#include "nro.h"

namespace skyline::loader {
    NroLoader::NroLoader(const std::shared_ptr<vfs::Backing> &backing) : backing(backing) {
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

    std::vector<u8> NroLoader::GetIcon() {
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

    void* NroLoader::LoadProcessData(const std::shared_ptr<kernel::type::KProcess> process, const DeviceState &state) {
        Executable nroExecutable{};

        nroExecutable.text.contents = GetSegment(header.text);
        nroExecutable.text.offset = 0;

        nroExecutable.ro.contents = GetSegment(header.ro);
        nroExecutable.ro.offset = header.text.size;

        nroExecutable.data.contents = GetSegment(header.data);
        nroExecutable.data.offset = header.text.size + header.ro.size;

        nroExecutable.bssSize = header.bssSize;

        state.process->memory.InitializeVmm(memory::AddressSpaceType::AddressSpace39Bit);
        auto loadInfo{LoadExecutable(process, state, nroExecutable)};
        state.process->memory.InitializeRegions(loadInfo.base, loadInfo.size);

        return loadInfo.entry;
    }
}
