// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <linux/elf.h>
#include <vfs/nacp.h>
#include <vfs/cnmt.h>
#include <vfs/nca.h>
#include <common/signal.h>
#include "executable.h"

namespace skyline::loader {
    /**
     * @brief A concept that checks if a type is either a 32-bit or a 64-bit ELF symbol
     * @tparam T The type to check
     */
    template<typename T>
    concept ElfSymbol = std::same_as<T, Elf32_Sym> || std::same_as<T, Elf64_Sym>;

    /**
     * @brief The types of ROM files
     * @note This needs to be synchronized with emu.skyline.loader.BaseLoader.RomFormat
     */
    enum class RomFormat {
        NRO, //!< The NRO format: https://switchbrew.org/wiki/NRO
        NSO, //!< The NSO format: https://switchbrew.org/wiki/NSO
        NCA, //!< The NCA format: https://switchbrew.org/wiki/NCA
        XCI, //!< The XCI format: https://switchbrew.org/wiki/XCI
        NSP, //!< The NSP format from "nspwn" exploit: https://switchbrew.org/wiki/Switch_System_Flaws
    };

    /**
     * @brief All possible results when parsing ROM files
     * @note This needs to be synchronized with emu.skyline.loader.LoaderResult
     */
    enum class LoaderResult : int8_t {
        Success,
        ParsingError,
        MissingHeaderKey,
        MissingTitleKey,
        MissingTitleKek,
        MissingKeyArea,

        ErrorSparseNCA,
        ErrorCompressedNCA,
    };

    /**
     * @brief An exception used specifically for errors related to loaders, it's used to communicate errors to the Kotlin-side of the loader
     */
    class loader_exception : public exception {
      public:
        const LoaderResult error;

        loader_exception(LoaderResult error, const std::string &message = "No message") : exception("Loader exception {}: {}", error, message), error(error) {}
    };

    /**
     * @brief The Loader class provides an abstract interface for ROM loaders
     */
    class Loader {
      private:
        /**
         * @brief All data used to determine the corresponding symbol for an address from an executable
         */
        struct ExecutableSymbolicInfo {
            void *patchStart; //!< A pointer to the start of this executable's patch section
            void *hookStart; //!< A pointer to the start of this executable's hook section
            void *programStart; //!< A pointer to the start of the executable
            void *programEnd; //!< A pointer to the end of the executable
            std::string name; //!< The name of the executable
            std::string patchName; //!< The name of the patch section
            std::string hookName; //!< The name of the hook section
            span<u8> symbols; //!< A span over the .dynsym section, this may be casted to the appropriate Elf_Sym type on demand
            span<char> symbolStrings; //!< A span over the .dynstr section
        };

        std::vector<ExecutableSymbolicInfo> executables;

      public:
        /**
         * @brief Information about the placement of an executable in memory
         */
        struct ExecutableLoadInfo {
            u8 *base; //!< The base of the loaded executable
            size_t size; //!< The total size of the loaded executable
            void *entry; //!< The entry point of the loaded executable
        };

        /**
         * @brief Patches an executable and loads it into memory while setting up symbolic information
         * @param offset The offset from the base address that the executable should be placed at
         * @param name An optional name for the executable, used for symbol resolution
         * @return An ExecutableLoadInfo struct containing the load base and size
         */
        ExecutableLoadInfo LoadExecutable(const std::shared_ptr<kernel::type::KProcess> &process, const DeviceState &state, Executable &executable, size_t offset = 0, const std::string &name = {}, bool dynamicallyLinked = false);

        std::optional<vfs::NACP> nacp;
        std::optional<vfs::CNMT> cnmt;
        std::optional<vfs::NCA> programNca; //!< The main program NCA within the NSP
        std::optional<vfs::NCA> controlNca; //!< The main control NCA within the NSP
        std::optional<vfs::NCA> publicNca;
        std::shared_ptr<vfs::Backing> romFs;

        virtual ~Loader() = default;

        virtual std::vector<u8> GetIcon(language::ApplicationLanguage language) {
            return std::vector<u8>();
        }

        /**
         * @return Entry point to the start of the main executable in the ROM
         */
        virtual void *LoadProcessData(const std::shared_ptr<kernel::type::KProcess> &process, const DeviceState &state) = 0;

        /**
         * @note The lifetime of the data contained within is tied to the lifetime of the Loader class it was obtained from (as this points to symbols from the executables loaded into memory directly)
         */
        struct SymbolInfo {
            char *name; //!< The name of the symbol that was found
            std::string_view executableName; //!< The executable that contained the symbol
        };

        /**
         * @return All symbolic information about the symbol for the specified address
         * @note If a symbol isn't found then SymbolInfo::name will be nullptr
         */
        template<ElfSymbol ElfSym>
        SymbolInfo ResolveSymbol(void *ptr);

        /**
         * @return All symbolic information about the 64-bit symbol for the specified address
         * @note If a symbol isn't found then SymbolInfo::name will be nullptr
         */
        SymbolInfo ResolveSymbol64(void *ptr) {
            return ResolveSymbol<Elf64_Sym>(ptr);
        }

        /**
         * @param frame The initial stack frame or the calling function's stack frame by default
         * @return A string with the stack trace based on the supplied context
         */
        std::string GetStackTrace(signal::StackFrame *frame = nullptr);

        /**
         * @return A string with the stack trace based on the stack frames in the supplied vector
         */
        std::string GetStackTrace(const std::vector<void *> &frames);
    };
}
