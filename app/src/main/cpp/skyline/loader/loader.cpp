// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <nce.h>
#include <os.h>
#include <kernel/types/KProcess.h>
#include <kernel/memory.h>
#include "loader.h"

namespace skyline::loader {
    Loader::ExecutableLoadInfo Loader::LoadExecutable(const std::shared_ptr<kernel::type::KProcess> process, const DeviceState &state, Executable &executable, size_t offset) {
        u8 *base{reinterpret_cast<u8 *>(process->memory.base.address + offset)};

        u64 textSize{executable.text.contents.size()};
        u64 roSize{executable.ro.contents.size()};
        u64 dataSize{executable.data.contents.size() + executable.bssSize};

        if (!util::PageAligned(textSize) || !util::PageAligned(roSize) || !util::PageAligned(dataSize))
            throw exception("LoadProcessData: Sections are not aligned with page size: 0x{:X}, 0x{:X}, 0x{:X}", textSize, roSize, dataSize);

        if (!util::PageAligned(executable.text.offset) || !util::PageAligned(executable.ro.offset) || !util::PageAligned(executable.data.offset))
            throw exception("LoadProcessData: Section offsets are not aligned with page size: 0x{:X}, 0x{:X}, 0x{:X}", executable.text.offset, executable.ro.offset, executable.data.offset);

        auto patch{state.nce->GetPatchData(executable.text.contents)};

        process->NewHandle<kernel::type::KPrivateMemory>(base, patch.size, memory::Permission{false, false, false}, memory::states::Reserved); // ---
        state.logger->Debug("Successfully mapped section .patch @ 0x{:X}, Size = 0x{:X}", base, patch.size);

        process->NewHandle<kernel::type::KPrivateMemory>(base + patch.size + executable.text.offset, textSize, memory::Permission{true, false, true}, memory::states::CodeStatic); // R-X
        state.logger->Debug("Successfully mapped section .text @ 0x{:X}, Size = 0x{:X}", base + patch.size + executable.text.offset, textSize);

        process->NewHandle<kernel::type::KPrivateMemory>(base + patch.size + executable.ro.offset, roSize, memory::Permission{true, false, false}, memory::states::CodeReadOnly); // R--
        state.logger->Debug("Successfully mapped section .rodata @ 0x{:X}, Size = 0x{:X}", base + patch.size + executable.ro.offset, roSize);

        process->NewHandle<kernel::type::KPrivateMemory>(base + patch.size + executable.data.offset, dataSize, memory::Permission{true, true, false}, memory::states::CodeMutable); // RW-
        state.logger->Debug("Successfully mapped section .data + .bss @ 0x{:X}, Size = 0x{:X}", base + patch.size + executable.data.offset, dataSize);

        state.nce->PatchCode(executable.text.contents, reinterpret_cast<u32*>(base), patch.size, patch.offsets);
        std::memcpy(base + patch.size + executable.text.offset, executable.text.contents.data(), textSize);
        std::memcpy(base + patch.size + executable.ro.offset, executable.ro.contents.data(), roSize);
        std::memcpy(base + patch.size + executable.data.offset, executable.data.contents.data(), dataSize - executable.bssSize);

        return {base, patch.size + textSize + roSize + dataSize, base + patch.size};
    }
}
