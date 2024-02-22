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
    Loader::ExecutableLoadInfo Loader::LoadExecutable(const std::shared_ptr<kernel::type::KProcess> &process, const DeviceState &state, Executable &executable, size_t offset, const std::string &name, bool dynamicallyLinked) {
        u8 *base{reinterpret_cast<u8 *>(process->memory.code.host.data() + offset)};
        // The base address in the guest address space, used to map memory by the memory manager
        u8 *guestBase{reinterpret_cast<u8 *>(process->memory.code.guest.data() + offset)};

        const bool is64bit{process->npdm.meta.flags.is64Bit};
        // NCE patching is only required for 64-bit executables
        const bool needsNcePatching{is64bit};
        // Only enable symbol hooking for 64-bit executables
        const bool enableSymbolHooking{is64bit};

        size_t textSize{executable.text.contents.size()};
        size_t roSize{executable.ro.contents.size()};
        size_t dataSize{executable.data.contents.size() + executable.bssSize};

        if (!util::IsPageAligned(textSize) || !util::IsPageAligned(roSize) || !util::IsPageAligned(dataSize))
            throw exception("Sections are not aligned with page size: 0x{:X}, 0x{:X}, 0x{:X}", textSize, roSize, dataSize);

        if (executable.text.offset != 0)
            throw exception("Executable's .text offset is not 0: 0x{:X}", executable.text.offset);

        if (!util::IsPageAligned(executable.text.offset) || !util::IsPageAligned(executable.ro.offset) || !util::IsPageAligned(executable.data.offset))
            throw exception("Section offsets are not aligned with page size: 0x{:X}, 0x{:X}, 0x{:X}", executable.text.offset, executable.ro.offset, executable.data.offset);

        // Use an empty PatchData if we don't need to patch
        auto patch{needsNcePatching ? state.nce->GetPatchData(executable.text.contents) : nce::NCE::PatchData{}};

        span dynsym{reinterpret_cast<u8 *>(executable.ro.contents.data() + executable.dynsym.offset), executable.dynsym.size};
        span dynstr{reinterpret_cast<char *>(executable.ro.contents.data() + executable.dynstr.offset), executable.dynstr.size};

        // Get patching info for symbols that we want to hook if symbol hooking is enabled
        std::vector<hle::HookedSymbolEntry> executableSymbols;
        size_t hookSize{0};
        if (enableSymbolHooking && dynamicallyLinked) {
            executableSymbols = hle::GetExecutableSymbols(dynsym.cast<Elf64_Sym>(), dynstr);
            hookSize = util::AlignUp(state.nce->GetHookSectionSize(executableSymbols), PAGE_SIZE);
        }

        // Reserve patch + hook size only if we need to patch
        if (patch.size > 0) {
            if (process->memory.addressSpaceType == memory::AddressSpaceType::AddressSpace36Bit) {
                process->memory.MapHeapMemory(span<u8>{guestBase, patch.size + hookSize}); // ---
                process->memory.SetRegionPermission(span<u8>{guestBase, patch.size + hookSize}, memory::Permission{false, false, false});
            } else {
                process->memory.Reserve(span<u8>{guestBase, patch.size + hookSize}); // ---
            }
            LOGD("Successfully mapped section .patch @ {}, Size = 0x{:X}", fmt::ptr(guestBase), patch.size);
            if (hookSize > 0)
                LOGD("Successfully mapped section .hook @ {}, Size = 0x{:X}", fmt::ptr(guestBase + patch.size), hookSize);
        }

        u8 *executableBase{base + patch.size + hookSize};
        // The base executable address in the guest address space
        u8 *executableGuestBase{guestBase + patch.size + hookSize};

        process->memory.MapCodeMemory(span<u8>{executableGuestBase + executable.text.offset, textSize}, memory::Permission{true, false, true}); // R-X
        LOGD("Successfully mapped section .text @ {}, Size = 0x{:X}", fmt::ptr(executableGuestBase), textSize);

        process->memory.MapCodeMemory(span<u8>{executableGuestBase + executable.ro.offset, roSize}, memory::Permission{true, false, false}); // R--
        LOGD("Successfully mapped section .rodata @ {}, Size = 0x{:X}", fmt::ptr(executableGuestBase + executable.ro.offset), roSize);

        process->memory.MapMutableCodeMemory(span<u8>{executableGuestBase + executable.data.offset, dataSize}); // RW-
        LOGD("Successfully mapped section .data + .bss @ {}, Size = 0x{:X}", fmt::ptr(executableGuestBase + executable.data.offset), dataSize);

        size_t size{patch.size + hookSize + textSize + roSize + dataSize};
        {
            // Note: We need to copy out the symbols here as it'll be overwritten by any hooks
            ExecutableSymbolicInfo symbolicInfo{
                .patchStart = base,
                .hookStart = base + patch.size,
                .programStart = executableBase,
                .programEnd = base + size,
                .name = name,
                .patchName = name + ".patch",
                .hookName = name + ".hook",
                .symbols = dynsym,
                .symbolStrings = dynstr,
            };
            executables.insert(std::upper_bound(executables.begin(), executables.end(), base, [](void *ptr, const ExecutableSymbolicInfo &it) { return ptr < it.patchStart; }), std::move(symbolicInfo));
        }

        // Patch the executable (NCE and symbol hooks)
        if (patch.size > 0) {
            state.nce->PatchCode(executable.text.contents, reinterpret_cast<u32 *>(base), patch.size, patch.offsets, hookSize);
            if (hookSize)
                state.nce->WriteHookSection(executableSymbols, span<u8>{base + patch.size, hookSize}.cast<u32>());
        }

        // Copy the executable sections to code memory
        std::memcpy(executableBase, executable.text.contents.data(), executable.text.contents.size());
        std::memcpy(executableBase + executable.ro.offset, executable.ro.contents.data(), roSize);
        std::memcpy(executableBase + executable.data.offset, executable.data.contents.data(), dataSize - executable.bssSize);

        return {guestBase, size, executableGuestBase + executable.text.offset};
    }

    template<ElfSymbol ElfSym>
    Loader::SymbolInfo Loader::ResolveSymbol(void *ptr) {
        auto executable{std::lower_bound(executables.begin(), executables.end(), ptr, [](const ExecutableSymbolicInfo &it, void *ptr) { return it.programEnd < ptr; })};
        auto symbols{executable->symbols.template cast<ElfSym>()};

        if (executable != executables.end() && ptr >= executable->patchStart && ptr <= executable->programEnd) {
            if (ptr >= executable->programStart) {
                auto offset{reinterpret_cast<u8 *>(ptr) - reinterpret_cast<u8 *>(executable->programStart)};
                auto symbol{std::find_if(symbols.begin(), symbols.end(), [&offset](const ElfSym &sym) { return sym.st_value <= offset && sym.st_value + sym.st_size > offset; })};
                if (symbol != symbols.end() && symbol->st_name && symbol->st_name < executable->symbolStrings.size()) {
                    return {executable->symbolStrings.data() + symbol->st_name, executable->name};
                } else {
                    return {.executableName = executable->name};
                }
            } else if (ptr >= executable->hookStart) {
                return {.executableName = executable->hookName};
            } else {
                return {.executableName = executable->patchName};
            }
        }
        return {};
    }

    inline std::string GetFunctionStackTrace(Loader *loader, void *pointer) {
        Dl_info info;
        auto symbol{loader->ResolveSymbol64(pointer)};
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
