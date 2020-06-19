// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <nce.h>
#include <os.h>
#include <kernel/memory.h>
#include <vfs/nacp.h>
#include <vfs/region_backing.h>
#include "nro.h"

namespace skyline::loader {
    NroLoader::NroLoader(const std::shared_ptr<vfs::Backing> &backing) : Loader(backing) {
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
        }
    }

    std::vector<u8> NroLoader::GetIcon() {
        NroAssetSection &segmentHeader = assetHeader.icon;
        std::vector<u8> buffer(segmentHeader.size);

        backing->Read(buffer.data(), header.size + segmentHeader.offset, segmentHeader.size);
        return buffer;
    }

    std::vector<u8> NroLoader::GetSegment(NroSegmentType segment) {
        NroSegmentHeader &segmentHeader = header.segments[static_cast<int>(segment)];
        std::vector<u8> buffer(segmentHeader.size);

        backing->Read(buffer.data(), segmentHeader.offset, segmentHeader.size);
        return buffer;
    }

    void NroLoader::LoadProcessData(const std::shared_ptr<kernel::type::KProcess> process, const DeviceState &state) {
        std::vector<u8> text = GetSegment(loader::NroLoader::NroSegmentType::Text);
        std::vector<u8> rodata = GetSegment(loader::NroLoader::NroSegmentType::RO);
        std::vector<u8> data = GetSegment(loader::NroLoader::NroSegmentType::Data);

        u64 textSize = text.size();
        u64 rodataSize = rodata.size();
        u64 dataSize = data.size();
        u64 bssSize = header.bssSize;

        std::vector<u32> patch = state.nce->PatchCode(text, constant::BaseAddress, textSize + rodataSize + dataSize + bssSize);

        if (!util::IsAligned(textSize, PAGE_SIZE) || !util::IsAligned(rodataSize, PAGE_SIZE) || !util::IsAligned(dataSize, PAGE_SIZE))
            throw exception("LoadProcessData: Sections are not aligned with page size: 0x{:X}, 0x{:X}, 0x{:X}", textSize, rodataSize, dataSize);

        u64 patchSize = patch.size() * sizeof(u32);
        u64 padding = util::AlignUp(textSize + rodataSize + dataSize + bssSize + patchSize, PAGE_SIZE) - (textSize + rodataSize + dataSize + bssSize + patchSize);

        process->NewHandle<kernel::type::KPrivateMemory>(constant::BaseAddress, textSize, memory::Permission{true, true, true}, memory::states::CodeStatic); // R-X
        state.logger->Debug("Successfully mapped section .text @ 0x{0:X}, Size = 0x{1:X}", constant::BaseAddress, textSize);

        process->NewHandle<kernel::type::KPrivateMemory>(constant::BaseAddress + textSize, rodataSize, memory::Permission{true, false, false}, memory::states::CodeReadOnly); // R--
        state.logger->Debug("Successfully mapped section .ro @ 0x{0:X}, Size = 0x{1:X}", constant::BaseAddress + textSize, rodataSize);

        process->NewHandle<kernel::type::KPrivateMemory>(constant::BaseAddress + textSize + rodataSize, dataSize, memory::Permission{true, true, false}, memory::states::CodeStatic); // RW-
        state.logger->Debug("Successfully mapped section .data @ 0x{0:X}, Size = 0x{1:X}", constant::BaseAddress + textSize + rodataSize, dataSize);

        process->NewHandle<kernel::type::KPrivateMemory>(constant::BaseAddress + textSize + rodataSize + dataSize, bssSize, memory::Permission{true, true, true}, memory::states::CodeMutable); // RWX
        state.logger->Debug("Successfully mapped section .bss @ 0x{0:X}, Size = 0x{1:X}", constant::BaseAddress + textSize + rodataSize + dataSize, bssSize);

        process->NewHandle<kernel::type::KPrivateMemory>(constant::BaseAddress + textSize + rodataSize + dataSize + bssSize, patchSize + padding, memory::Permission{true, true, true}, memory::states::CodeStatic); // RWX
        state.logger->Debug("Successfully mapped section .patch @ 0x{0:X}, Size = 0x{1:X}", constant::BaseAddress + textSize + rodataSize + dataSize + bssSize, patchSize);

        process->WriteMemory(text.data(), constant::BaseAddress, textSize);
        process->WriteMemory(rodata.data(), constant::BaseAddress + textSize, rodataSize);
        process->WriteMemory(data.data(), constant::BaseAddress + textSize + rodataSize, dataSize);
        process->WriteMemory(patch.data(), constant::BaseAddress + textSize + rodataSize + dataSize + bssSize, patchSize);

        state.os->memory.InitializeRegions(constant::BaseAddress, textSize + rodataSize + dataSize + bssSize + patchSize + padding, memory::AddressSpaceType::AddressSpace39Bit);
    }
}