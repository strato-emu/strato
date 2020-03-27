// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <vector>
#include <kernel/memory.h>
#include "nro.h"

namespace skyline::loader {
    NroLoader::NroLoader(const int romFd) : Loader(romFd) {
        ReadOffset((u32 *) &header, 0x0, sizeof(NroHeader));

        constexpr auto nroMagic = 0x304F524E; // "NRO0" in reverse, this is written at the start of every NRO file

        if (header.magic != nroMagic)
            throw exception("Invalid NRO magic! 0x{0:X}", header.magic);
    }

    void NroLoader::LoadProcessData(const std::shared_ptr<kernel::type::KProcess> process, const DeviceState &state) {
        std::vector<u8> text(header.text.size);
        std::vector<u8> rodata(header.ro.size);
        std::vector<u8> data(header.data.size);

        ReadOffset(text.data(), header.text.offset, header.text.size);
        ReadOffset(rodata.data(), header.ro.offset, header.ro.size);
        ReadOffset(data.data(), header.data.offset, header.data.size);

        std::vector<u32> patch = state.nce->PatchCode(text, constant::BaseAddress, header.text.size + header.ro.size + header.data.size + header.bssSize);

        u64 textSize = text.size();
        u64 rodataSize = rodata.size();
        u64 dataSize = data.size();
        u64 patchSize = patch.size() * sizeof(u32);
        u64 padding = utils::AlignUp(textSize + rodataSize + dataSize + header.bssSize + patchSize, PAGE_SIZE) - (textSize + rodataSize + dataSize + header.bssSize + patchSize);

        process->NewHandle<kernel::type::KPrivateMemory>(constant::BaseAddress, textSize, memory::Permission{true, true, true}, memory::states::CodeStatic); // R-X
        state.logger->Debug("Successfully mapped section .text @ 0x{0:X}, Size = 0x{1:X}", constant::BaseAddress, textSize);

        process->NewHandle<kernel::type::KPrivateMemory>(constant::BaseAddress + textSize, rodataSize, memory::Permission{true, false, false}, memory::states::CodeReadOnly); // R--
        state.logger->Debug("Successfully mapped section .ro @ 0x{0:X}, Size = 0x{1:X}", constant::BaseAddress + textSize, rodataSize);

        process->NewHandle<kernel::type::KPrivateMemory>(constant::BaseAddress + textSize + rodataSize, dataSize, memory::Permission{true, true, false}, memory::states::CodeStatic); // RW-
        state.logger->Debug("Successfully mapped section .data @ 0x{0:X}, Size = 0x{1:X}", constant::BaseAddress + textSize + rodataSize, dataSize);

        process->NewHandle<kernel::type::KPrivateMemory>(constant::BaseAddress + textSize + rodataSize + dataSize, header.bssSize, memory::Permission{true, true, true}, memory::states::CodeMutable); // RWX
        state.logger->Debug("Successfully mapped section .bss @ 0x{0:X}, Size = 0x{1:X}", constant::BaseAddress + textSize + rodataSize + dataSize, header.bssSize);

        process->NewHandle<kernel::type::KPrivateMemory>(constant::BaseAddress + textSize + rodataSize + dataSize + header.bssSize, patchSize + padding, memory::Permission{true, true, true}, memory::states::CodeStatic); // RWX
        state.logger->Debug("Successfully mapped section .patch @ 0x{0:X}, Size = 0x{1:X}", constant::BaseAddress + textSize + rodataSize + dataSize + header.bssSize, patchSize);

        process->WriteMemory(text.data(), constant::BaseAddress, textSize);
        process->WriteMemory(rodata.data(), constant::BaseAddress + textSize, rodataSize);
        process->WriteMemory(data.data(), constant::BaseAddress + textSize + rodataSize, dataSize);
        process->WriteMemory(patch.data(), constant::BaseAddress + textSize + rodataSize + dataSize + header.bssSize, patchSize);

        state.os->memory.InitializeRegions(constant::BaseAddress, textSize + rodataSize + dataSize + header.bssSize + patchSize + padding, memory::AddressSpaceType::AddressSpace39Bit);
    }
}
