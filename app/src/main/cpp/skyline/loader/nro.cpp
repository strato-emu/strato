// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <nce.h>
#include <os.h>
#include <kernel/memory.h>
#include <vfs/nacp.h>
#include <vfs/region_backing.h>
#include "nro.h"

namespace skyline::loader {
    NroLoader::NroLoader(const std::shared_ptr<vfs::Backing> &backing) : backing(backing) {
        backing->Read(&header);

        if (header.magic != util::MakeMagic<u32>("NRO0"))
            throw exception("Invalid NRO magic! 0x{0:X}", header.magic);

        // The homebrew asset section is appended to the end of an NRO file
        if (backing->size > header.size) {
            backing->Read(&assetHeader, header.size);

            if (assetHeader.magic != util::MakeMagic<u32>("ASET"))
                throw exception("Invalid ASET magic! 0x{0:X}", assetHeader.magic);

            NroAssetSection &nacpHeader = assetHeader.nacp;
            nacp = std::make_shared<vfs::NACP>(std::make_shared<vfs::RegionBacking>(backing, header.size + nacpHeader.offset, nacpHeader.size));

            NroAssetSection &romFsHeader = assetHeader.romFs;
            romFs = std::make_shared<vfs::RegionBacking>(backing, header.size + romFsHeader.offset, romFsHeader.size);
        }
    }

    std::vector<u8> NroLoader::GetIcon() {
        NroAssetSection &segmentHeader = assetHeader.icon;
        std::vector<u8> buffer(segmentHeader.size);

        backing->Read(buffer.data(), header.size + segmentHeader.offset, segmentHeader.size);
        return buffer;
    }

    std::vector<u8> NroLoader::GetSegment(const NroSegmentHeader &segment) {
        std::vector<u8> buffer(segment.size);

        backing->Read(buffer.data(), segment.offset, segment.size);
        return buffer;
    }

    void NroLoader::LoadProcessData(const std::shared_ptr<kernel::type::KProcess> process, const DeviceState &state) {
        Executable nroExecutable{};

        nroExecutable.text.contents = GetSegment(header.text);
        nroExecutable.text.offset = 0;

        nroExecutable.ro.contents = GetSegment(header.ro);
        nroExecutable.ro.offset = header.text.size;

        nroExecutable.data.contents = GetSegment(header.data);
        nroExecutable.data.offset = header.text.size + header.ro.size;

        nroExecutable.bssSize = header.bssSize;

        auto loadInfo = LoadExecutable(process, state, nroExecutable);
        state.os->memory.InitializeRegions(loadInfo.base, loadInfo.size, memory::AddressSpaceType::AddressSpace39Bit);
    }
}
