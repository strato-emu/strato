// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <dlfcn.h>
#include <cxxabi.h>
#include <nce.h>
#include <os.h>
#include <kernel/types/KProcess.h>
#include <kernel/memory.h>
#include "loader.h"

namespace skyline::loader {
    Loader::ExecutableLoadInfo Loader::LoadExecutable(const std::shared_ptr<kernel::type::KProcess> &process, const DeviceState &state, Executable &executable, size_t offset, const std::string &name) {
        u8 *base{reinterpret_cast<u8 *>(process->memory.base.data() + offset)};

        u64 textSize{executable.text.contents.size()};
        u64 roSize{executable.ro.contents.size()};
        u64 dataSize{executable.data.contents.size() + executable.bssSize};

        if (!util::IsPageAligned(textSize) || !util::IsPageAligned(roSize) || !util::IsPageAligned(dataSize))
            throw exception("LoadProcessData: Sections are not aligned with page size: 0x{:X}, 0x{:X}, 0x{:X}", textSize, roSize, dataSize);

        if (!util::IsPageAligned(executable.text.offset) || !util::IsPageAligned(executable.ro.offset) || !util::IsPageAligned(executable.data.offset))
            throw exception("LoadProcessData: Section offsets are not aligned with page size: 0x{:X}, 0x{:X}, 0x{:X}", executable.text.offset, executable.ro.offset, executable.data.offset);

        auto patch{state.nce->GetPatchData(executable.text.contents)};
        auto size{patch.size + textSize + roSize + dataSize};

        process->NewHandle<kernel::type::KPrivateMemory>(span<u8>{base, patch.size}, memory::Permission{false, false, false}, memory::states::Reserved); // ---
        Logger::Debug("Successfully mapped section .patch @ 0x{:X}, Size = 0x{:X}", base, patch.size);

        process->NewHandle<kernel::type::KPrivateMemory>(span<u8>{base + patch.size + executable.text.offset, textSize}, memory::Permission{true, false, true}, memory::states::CodeStatic); // R-X
        Logger::Debug("Successfully mapped section .text @ 0x{:X}, Size = 0x{:X}", base + patch.size + executable.text.offset, textSize);

        process->NewHandle<kernel::type::KPrivateMemory>(span<u8>{base + patch.size + executable.ro.offset, roSize}, memory::Permission{true, false, false}, memory::states::CodeStatic); // R--
        Logger::Debug("Successfully mapped section .rodata @ 0x{:X}, Size = 0x{:X}", base + patch.size + executable.ro.offset, roSize);

        process->NewHandle<kernel::type::KPrivateMemory>(span<u8>{base + patch.size + executable.data.offset, dataSize}, memory::Permission{true, true, false}, memory::states::CodeMutable); // RW-
        Logger::Debug("Successfully mapped section .data + .bss @ 0x{:X}, Size = 0x{:X}", base + patch.size + executable.data.offset, dataSize);

        state.nce->PatchCode(executable.text.contents, reinterpret_cast<u32 *>(base), patch.size, patch.offsets);
        std::memcpy(base + patch.size + executable.text.offset, executable.text.contents.data(), textSize);
        std::memcpy(base + patch.size + executable.ro.offset, executable.ro.contents.data(), roSize);
        std::memcpy(base + patch.size + executable.data.offset, executable.data.contents.data(), dataSize - executable.bssSize);

        auto rodataOffset{base + patch.size + executable.ro.offset};
        ExecutableSymbolicInfo symbolicInfo{
            .patchStart = base,
            .programStart = base + patch.size,
            .programEnd = base + size,
            .name = name,
            .patchName = name + ".patch",
            .symbols = span(reinterpret_cast<Elf64_Sym *>(rodataOffset + executable.dynsym.offset), executable.dynsym.size / sizeof(Elf64_Sym)),
            .symbolStrings = span(reinterpret_cast<char *>(rodataOffset + executable.dynstr.offset), executable.dynstr.size),
        };
        executables.insert(std::upper_bound(executables.begin(), executables.end(), base, [](void *ptr, const ExecutableSymbolicInfo &it) { return ptr < it.patchStart; }), symbolicInfo);

        return {base, size, base + patch.size};
    }

    Loader::SymbolInfo Loader::ResolveSymbol(void *ptr) {
        auto executable{std::lower_bound(executables.begin(), executables.end(), ptr, [](const ExecutableSymbolicInfo &it, void *ptr) { return it.programEnd < ptr; })};
        if (executable != executables.end() && ptr >= executable->patchStart && ptr <= executable->programEnd) {
            if (ptr >= executable->programStart) {
                auto offset{reinterpret_cast<u8 *>(ptr) - reinterpret_cast<u8 *>(executable->programStart)};
                auto symbol{std::find_if(executable->symbols.begin(), executable->symbols.end(), [&offset](const Elf64_Sym &sym) { return sym.st_value <= offset && sym.st_value + sym.st_size > offset; })};
                if (symbol != executable->symbols.end() && symbol->st_name && symbol->st_name < executable->symbolStrings.size()) {
                    return {executable->symbolStrings.data() + symbol->st_name, executable->name};
                } else {
                    return {.executableName = executable->name};
                }
            } else {
                return {.executableName = executable->patchName};
            }
        }
        return {};
    }

    inline std::string GetFunctionStackTrace(Loader *loader, void *pointer) {
        Dl_info info;
        auto symbol{loader->ResolveSymbol(pointer)};
        if (symbol.name) {
            int status{};
            size_t length{};
            std::unique_ptr<char, decltype(&std::free)> demangled{abi::__cxa_demangle(symbol.name, nullptr, &length, &status), std::free};

            return fmt::format("\n* 0x{:X} ({} from {})", reinterpret_cast<uintptr_t>(pointer), (status == 0) ? std::string_view(demangled.get()) : symbol.name, symbol.executableName);
        } else if (!symbol.executableName.empty()) {
            return fmt::format("\n* 0x{:X} (from {})", reinterpret_cast<uintptr_t>(pointer), symbol.executableName);
        } else if (dladdr(pointer, &info)) {
            int status{};
            size_t length{};
            std::unique_ptr<char, decltype(&std::free)> demangled{abi::__cxa_demangle(info.dli_sname, nullptr, &length, &status), std::free};

            auto extractFilename{[](const char *path) {
                const char *filename{path};
                for (const char *p{path}; *p; p++)
                    if (*p == '/')
                        filename = p + 1;
                return filename;
            }}; //!< Extracts the filename from a path as we only want the filename of a shared object

            if (info.dli_sname && info.dli_fname)
                return fmt::format("\n* 0x{:X} ({} from {})", reinterpret_cast<uintptr_t>(pointer), (status == 0) ? std::string_view(demangled.get()) : info.dli_sname, extractFilename(info.dli_fname));
            else if (info.dli_sname)
                return fmt::format("\n* 0x{:X} ({})", reinterpret_cast<uintptr_t>(pointer), (status == 0) ? std::string_view(demangled.get()) : info.dli_sname);
            else if (info.dli_fname)
                return fmt::format("\n* 0x{:X} (from {})", reinterpret_cast<uintptr_t>(pointer), extractFilename(info.dli_fname));
            else
                return fmt::format("\n* 0x{:X}", reinterpret_cast<uintptr_t>(pointer));
        } else {
            return fmt::format("\n* 0x{:X}", reinterpret_cast<uintptr_t>(pointer));
        }
    }

    std::string Loader::GetStackTrace(signal::StackFrame *frame) {
        std::string trace;
        if (!frame)
            asm("MOV %0, FP" : "=r"(frame));
        while (frame) {
            trace += GetFunctionStackTrace(this, frame->lr);
            frame = frame->next;
        }
        return trace;
    }

    std::string Loader::GetStackTrace(const std::vector<void *> &frames) {
        std::string trace;
        for (const auto &frame : frames)
            trace += GetFunctionStackTrace(this, frame);
        return trace;
    }
}
