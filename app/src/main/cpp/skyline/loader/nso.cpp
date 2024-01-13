// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <lz4.h>
#include <nce.h>
#include <kernel/types/KProcess.h>
#include <boost/regex/v5/regex.hpp>
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

        PrintRoContentsInfo(executable.ro.contents);

        return loader->LoadExecutable(process, state, executable, offset, name, dynamicallyLinked);
    }

    void *NsoLoader::LoadProcessData(const std::shared_ptr<kernel::type::KProcess> &process, const DeviceState &state) {
        state.process->memory.InitializeVmm(memory::AddressSpaceType::AddressSpace39Bit);
        auto loadInfo{LoadNso(this, backing, process, state)};
        state.process->memory.InitializeRegions(span<u8>{loadInfo.base, loadInfo.size});
        return loadInfo.entry;
    }

    void NsoLoader::PrintRoContentsInfo(const std::vector<u8> &contents) {
        const boost::regex moduleRegex(R"([a-z]:[\\/][ -~]{5,}\.nss)", boost::regex::icase);
        const boost::regex fsSdkRegex("sdk_version: ([0-9.]*)");
        const boost::regex sdkMwRegex("SDK MW[ -~]*");

        std::string contentsRaw(contents.begin(), contents.end());
        std::string modulePath;
        if (memcmp(&contents[0], "\x00\x00\x00\x00", 4) == 0) {
            i32 length;
            std::memcpy(&length, &contents[4], sizeof(i32));

            if (length > 0)
                modulePath = reinterpret_cast<const char *>(&contents[4 + sizeof(i32)]);
        }

        if (modulePath.empty()) {
            boost::smatch moduleMatch;
            if (boost::regex_search(contentsRaw, moduleMatch, moduleRegex))
                modulePath = moduleMatch.str();
        }

        LOGI("Module Path: {}", modulePath);

        boost::smatch fsSdkMatch;
        if (boost::regex_search(contentsRaw, fsSdkMatch, fsSdkRegex))
            LOGI("SDK Version: {}", fsSdkMatch[1].str());

        boost::sregex_iterator sdkMwMatchesBegin(contentsRaw.begin(), contentsRaw.end(), sdkMwRegex);
        boost::sregex_iterator sdkMwMatchesEnd;

        if (sdkMwMatchesBegin != sdkMwMatchesEnd) {
            std::string libContent;

            while (sdkMwMatchesBegin != sdkMwMatchesEnd) {
                libContent += sdkMwMatchesBegin->str() + "\n";
                sdkMwMatchesBegin++;
            }

            LOGI("SDK Libraries: {}", libContent);
        }
    }
}
