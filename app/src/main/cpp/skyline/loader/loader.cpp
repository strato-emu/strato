// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <dlfcn.h>
#include <cxxabi.h>
#include <nce.h>
#include <os.h>
#include <kernel/types/KProcess.h>
#include <kernel/memory.h>
#include <hle/symbol_hook_table.h>
#include "loader.h"

namespace skyline::loader {
    Loader::ExecutableLoadInfo Loader::LoadExecutable(const std::shared_ptr<kernel::type::KProcess> &process, const DeviceState &state, Executable &executable, size_t offset, const std::string &name, bool dynamicallyLinked) {
        u8 *base{reinterpret_cast<u8 *>(process->memory.code.data() + offset)};

        size_t textSize{executable.text.contents.size()};
        size_t roSize{executable.ro.contents.size()};
        size_t dataSize{executable.data.contents.size() + executable.bssSize};

        if (!util::IsPageAligned(textSize) || !util::IsPageAligned(roSize) || !util::IsPageAligned(dataSize))
            throw exception("Sections are not aligned with page size: 0x{:X}, 0x{:X}, 0x{:X}", textSize, roSize, dataSize);

        if (executable.text.offset != 0)
            throw exception("Executable's .text offset is not 0: 0x{:X}", executable.text.offset);

        if (!util::IsPageAligned(executable.text.offset) || !util::IsPageAligned(executable.ro.offset) || !util::IsPageAligned(executable.data.offset))
            throw exception("Section offsets are not aligned with page size: 0x{:X}, 0x{:X}, 0x{:X}", executable.text.offset, executable.ro.offset, executable.data.offset);

        auto patch{state.nce->GetPatchData(executable.text.contents)};

        span dynsym{reinterpret_cast<Elf64_Sym *>(executable.ro.contents.data() + executable.dynsym.offset), executable.dynsym.size / sizeof(Elf64_Sym)};
        span dynstr{reinterpret_cast<char *>(executable.ro.contents.data() + executable.dynstr.offset), executable.dynstr.size};
        std::vector<nce::NCE::HookedSymbolEntry> executableSymbols;
        size_t hookSize{};
        if (dynamicallyLinked) {
            if constexpr (!hle::HookedSymbols.empty()) {
                for (auto &symbol : dynsym) {
                    if (symbol.st_name == 0 || symbol.st_value == 0)
                        continue;

                    if (ELF64_ST_TYPE(symbol.st_info) != STT_FUNC || ELF64_ST_BIND(symbol.st_info) != STB_GLOBAL || symbol.st_shndx == SHN_UNDEF)
                        continue;

                    std::string_view symbolName{dynstr.data() + symbol.st_name};

                    auto item{std::find_if(hle::HookedSymbols.begin(), hle::HookedSymbols.end(), [&symbolName](const auto &item) {
                        return item.name == symbolName;
                    })};
                    if (item != hle::HookedSymbols.end()) {
                        executableSymbols.emplace_back(std::string{symbolName}, item->hook, &symbol.st_value);
                        continue;
                    }

                    #ifdef PRINT_HOOK_ALL
                    if (symbolName == "memcpy" || symbolName == "memcmp" || symbolName == "memset" || symbolName == "strcmp" ||  symbolName == "strlen")
                        // If symbol is from libc (such as memcpy, strcmp, strlen, etc), we don't need to hook it
                        continue;

                    executableSymbols.emplace_back(std::string{symbolName}, hle::EntryExitHook{
                        .entry = [](const DeviceState &, const hle::HookedSymbol &symbol) {
                            Logger::Error("Entering \"{}\" ({})", symbol.prettyName, symbol.name);
                        },
                        .exit = [](const DeviceState &, const hle::HookedSymbol &symbol) {
                            Logger::Error("Exiting \"{}\"", symbol.prettyName);
                        },
                    }, &symbol.st_value);
                    #endif
                }
            }

            #ifdef PRINT_HOOK_ALL
            for (auto &symbol : dynsym) {
                if (symbolName == "memcpy" || symbolName == "memcmp" || symbolName == "memset" || symbolName == "strcmp" ||  symbolName == "strlen")
                        // If symbol is from libc (such as memcpy, strcmp, strlen, etc), we don't need to hook it
                        continue;

                    executableSymbols.emplace_back(std::string{symbolName}, hle::EntryExitHook{
                        .entry = [](const DeviceState &, const hle::HookedSymbol &symbol) {
                            Logger::Error("Entering \"{}\" ({})", symbol.prettyName, symbol.name);
                        },
                        .exit = [](const DeviceState &, const hle::HookedSymbol &symbol) {
                            Logger::Error("Exiting \"{}\"", symbol.prettyName);
                        },
                    }, &symbol.st_value);
            }
            #endif

            hookSize = util::AlignUp(state.nce->GetHookSectionSize(executableSymbols), PAGE_SIZE);
        }

        auto patchType{process->memory.addressSpaceType == memory::AddressSpaceType::AddressSpace36Bit ? memory::states::Heap : memory::states::Reserved};
        process->NewHandle<kernel::type::KPrivateMemory>(span<u8>{base, patch.size + hookSize}, memory::Permission{false, false, false}, patchType); // ---
        Logger::Debug("Successfully mapped section .patch @ 0x{:X}, Size = 0x{:X}", base, patch.size);
        if (hookSize > 0)
            Logger::Debug("Successfully mapped section .hook @ 0x{:X}, Size = 0x{:X}", base + patch.size, hookSize);

        u8 *executableBase{base + patch.size + hookSize};
        process->NewHandle<kernel::type::KPrivateMemory>(span<u8>{executableBase + executable.text.offset, textSize}, memory::Permission{true, false, true}, memory::states::CodeStatic); // R-X
        Logger::Debug("Successfully mapped section .text @ 0x{:X}, Size = 0x{:X}", executableBase, textSize);

        process->NewHandle<kernel::type::KPrivateMemory>(span<u8>{executableBase + executable.ro.offset, roSize}, memory::Permission{true, false, false}, memory::states::CodeStatic); // R--
        Logger::Debug("Successfully mapped section .rodata @ 0x{:X}, Size = 0x{:X}", executableBase + executable.ro.offset, roSize);

        process->NewHandle<kernel::type::KPrivateMemory>(span<u8>{executableBase + executable.data.offset, dataSize}, memory::Permission{true, true, false}, memory::states::CodeMutable); // RW-
        Logger::Debug("Successfully mapped section .data + .bss @ 0x{:X}, Size = 0x{:X}", executableBase + executable.data.offset, dataSize);

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
                .symbols = {dynsym.begin(), dynsym.end()},
                .symbolStrings = {dynstr.begin(), dynstr.end()},
            };
            executables.insert(std::upper_bound(executables.begin(), executables.end(), base, [](void *ptr, const ExecutableSymbolicInfo &it) { return ptr < it.patchStart; }), std::move(symbolicInfo));
        }

        state.nce->PatchCode(executable.text.contents, reinterpret_cast<u32 *>(base), patch.size, patch.offsets, hookSize);
        if (hookSize)
            state.nce->WriteHookSection(executableSymbols, span<u8>{base + patch.size, hookSize}.cast<u32>());

        std::memcpy(executableBase, executable.text.contents.data(), executable.text.contents.size());
        std::memcpy(executableBase + executable.ro.offset, executable.ro.contents.data(), roSize);
        std::memcpy(executableBase + executable.data.offset, executable.data.contents.data(), dataSize - executable.bssSize);

        Logger::EmulationContext.Flush();
        return {base, size, executableBase + executable.text.offset};
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
