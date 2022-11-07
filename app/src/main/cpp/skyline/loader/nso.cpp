// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <lz4.h>
#include <nce.h>
#include <kernel/types/KProcess.h>
#include "nso.h"

namespace skyline::loader {
    NsoLoader::NsoLoader(std::shared_ptr<vfs::Backing> pBacking) : backing(std::move(pBacking)) {
        u32 magic{backing->Read<u32>()};

        if (magic != util::MakeMagic<u32>("NSO0"))
            throw exception("Invalid NSO magic! 0x{0:X}", magic);
    }

    std::vector<u8> NsoLoader::GetSegment(const std::shared_ptr<vfs::Backing> &backing, const NsoSegmentHeader &segment, u32 compressedSize) {
        std::vector<u8> outputBuffer(segment.decompressedSize);

        if (compressedSize) {
            std::vector<u8> compressedBuffer(compressedSize);
            backing->Read(compressedBuffer, segment.fileOffset);

            LZ4_decompress_safe(reinterpret_cast<char *>(compressedBuffer.data()), reinterpret_cast<char *>(outputBuffer.data()), static_cast<int>(compressedSize), static_cast<int>(segment.decompressedSize));
        } else {
            backing->Read(outputBuffer, segment.fileOffset);
        }

        return outputBuffer;
    }

    Loader::ExecutableLoadInfo NsoLoader::LoadNso(Loader *loader, const std::shared_ptr<vfs::Backing> &backing, const std::shared_ptr<kernel::type::KProcess> &process, const DeviceState &state, size_t offset, const std::string &name, bool dynamicallyLinked) {
        auto header{backing->Read<NsoHeader>()};

        if (header.magic != util::MakeMagic<u32>("NSO0"))
            throw exception("Invalid NSO magic! 0x{0:X}", header.magic);

        Executable executable{};

        executable.text.contents = GetSegment(backing, header.text, header.flags.textCompressed ? header.textCompressedSize : 0);
        executable.text.contents.resize(util::AlignUp(executable.text.contents.size(), constant::PageSize));
        executable.text.offset = header.text.memoryOffset;

        executable.ro.contents = GetSegment(backing, header.ro, header.flags.roCompressed ? header.roCompressedSize : 0);
        executable.ro.contents.resize(util::AlignUp(executable.ro.contents.size(), constant::PageSize));
        executable.ro.offset = header.ro.memoryOffset;

        executable.data.contents = GetSegment(backing, header.data, header.flags.dataCompressed ? header.dataCompressedSize : 0);
        executable.data.offset = header.data.memoryOffset;

        // Data and BSS are aligned together
        executable.bssSize = util::AlignUp(executable.data.contents.size() + header.bssSize, constant::PageSize) - executable.data.contents.size();

        if (header.dynsym.offset + header.dynsym.size <= header.ro.decompressedSize && header.dynstr.offset + header.dynstr.size <= header.ro.decompressedSize) {
            executable.dynsym = {header.dynsym.offset, header.dynsym.size};
            executable.dynstr = {header.dynstr.offset, header.dynstr.size};
        }

        return loader->LoadExecutable(process, state, executable, offset, name, dynamicallyLinked);
    }

    void *NsoLoader::LoadProcessData(const std::shared_ptr<kernel::type::KProcess> &process, const DeviceState &state) {
        state.process->memory.InitializeVmm(memory::AddressSpaceType::AddressSpace39Bit);
        auto loadInfo{LoadNso(this, backing, process, state)};
        state.process->memory.InitializeRegions(span<u8>{loadInfo.base, loadInfo.size});
        return loadInfo.entry;
    }
}
