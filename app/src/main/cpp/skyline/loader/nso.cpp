// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <lz4.h>
#include <nce.h>
#include <kernel/types/KProcess.h>
#include "nso.h"

namespace skyline::loader {
    NsoLoader::NsoLoader(const std::shared_ptr<vfs::Backing> &backing) : backing(backing) {
        u32 magic{backing->Read<u32>()};

        if (magic != util::MakeMagic<u32>("NSO0"))
            throw exception("Invalid NSO magic! 0x{0:X}", magic);
    }

    std::vector<u8> NsoLoader::GetSegment(const std::shared_ptr<vfs::Backing> &backing, const NsoSegmentHeader &segment, u32 compressedSize) {
        std::vector<u8> outputBuffer(segment.decompressedSize);

        if (compressedSize) {
            std::vector<u8> compressedBuffer(compressedSize);
            backing->Read(compressedBuffer, segment.fileOffset);

            LZ4_decompress_safe(reinterpret_cast<char *>(compressedBuffer.data()), reinterpret_cast<char *>(outputBuffer.data()), compressedSize, segment.decompressedSize);
        } else {
            backing->Read(outputBuffer, segment.fileOffset);
        }

        return outputBuffer;
    }

    Loader::ExecutableLoadInfo NsoLoader::LoadNso(const std::shared_ptr<vfs::Backing> &backing, const std::shared_ptr<kernel::type::KProcess> process, const DeviceState &state, size_t offset) {
        auto header{backing->Read<NsoHeader>()};

        if (header.magic != util::MakeMagic<u32>("NSO0"))
            throw exception("Invalid NSO magic! 0x{0:X}", header.magic);

        Executable nsoExecutable{};

        nsoExecutable.text.contents = GetSegment(backing, header.text, header.flags.textCompressed ? header.textCompressedSize : 0);
        nsoExecutable.text.contents.resize(util::AlignUp(nsoExecutable.text.contents.size(), PAGE_SIZE));
        nsoExecutable.text.offset = header.text.memoryOffset;

        nsoExecutable.ro.contents = GetSegment(backing, header.ro, header.flags.roCompressed ? header.roCompressedSize : 0);
        nsoExecutable.ro.contents.resize(util::AlignUp(nsoExecutable.ro.contents.size(), PAGE_SIZE));
        nsoExecutable.ro.offset = header.ro.memoryOffset;

        nsoExecutable.data.contents = GetSegment(backing, header.data, header.flags.dataCompressed ? header.dataCompressedSize : 0);
        nsoExecutable.data.contents.resize(util::AlignUp(nsoExecutable.data.contents.size(), PAGE_SIZE));
        nsoExecutable.data.offset = header.data.memoryOffset;

        nsoExecutable.bssSize = util::AlignUp(header.bssSize, PAGE_SIZE);

        return LoadExecutable(process, state, nsoExecutable, offset);
    }

    void NsoLoader::LoadProcessData(const std::shared_ptr<kernel::type::KProcess> process, const DeviceState &state) {
        state.process->memory.InitializeVmm(memory::AddressSpaceType::AddressSpace39Bit);
        auto loadInfo{LoadNso(backing, process, state)};
        state.process->memory.InitializeRegions(loadInfo.base, loadInfo.size);
    }
}
