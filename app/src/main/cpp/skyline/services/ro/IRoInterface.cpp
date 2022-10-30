// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <fmt/ranges.h>
#include <mbedtls/sha256.h>
#include <loader/nro.h>
#include <nce.h>
#include "IRoInterface.h"

namespace skyline::service::ro {
    IRoInterface::IRoInterface(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result IRoInterface::LoadModule(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        u64 pid{request.Pop<u64>()};
        u64 nroAddress{request.Pop<u64>()};
        u64 nroSize{request.Pop<u64>()};
        u64 bssAddress{request.Pop<u64>()};
        u64 bssSize{request.Pop<u64>()};

        if (!util::IsPageAligned(nroAddress) || !util::IsPageAligned(bssAddress))
            return result::InvalidAddress;

        if (!util::IsPageAligned(nroSize) || !nroSize || !util::IsPageAligned(bssSize))
            return result::InvalidSize;

        auto data{span{reinterpret_cast<u8 *>(nroAddress), nroSize}};
        auto &header{data.as<loader::NroHeader>()};

        std::array<u8, 0x20> hash{};
        mbedtls_sha256_ret(data.data(), data.size(), hash.data(), 0);

        if (!loadedNros.emplace(hash).second)
            return result::AlreadyLoaded;

        // We don't handle NRRs here since they're purely used for signature verification which we will never do
        if (bssSize != header.bssSize)
            return result::InvalidNro;

        loader::Executable executable{};

        executable.text.offset = 0;
        executable.text.contents.resize(header.text.size);
        span(executable.text.contents).copy_from(data.subspan(header.text.offset, header.text.size));

        executable.ro.offset = header.text.size;
        executable.ro.contents.resize(header.ro.size);
        span(executable.ro.contents).copy_from(data.subspan(header.ro.offset, header.ro.size));

        executable.data.offset = header.text.size + header.ro.size;
        executable.data.contents.resize(header.data.size);
        span(executable.data.contents).copy_from(data.subspan(header.data.offset, header.data.size));

        executable.bssSize = header.bssSize;

        if (header.dynsym.offset > header.ro.offset && header.dynsym.offset + header.dynsym.size < header.ro.offset + header.ro.size && header.dynstr.offset > header.ro.offset && header.dynstr.offset + header.dynstr.size < header.ro.offset + header.ro.size) {
            executable.dynsym = {header.dynsym.offset, header.dynsym.size};
            executable.dynstr = {header.dynstr.offset, header.dynstr.size};
        }

        u64 textSize{executable.text.contents.size()};
        u64 roSize{executable.ro.contents.size()};
        u64 dataSize{executable.data.contents.size() + executable.bssSize};

        auto patch{state.nce->GetPatchData(executable.text.contents)};
        auto size{patch.size + textSize + roSize + dataSize};

        u8 *ptr{};
        do {
            ptr = util::AlignDown(util::RandomNumber(state.process->memory.base.data(), std::prev(state.process->memory.base.end()).base()), constant::PageSize) - size;

            if (state.process->memory.heap.contains(ptr) || state.process->memory.alias.contains(ptr))
                continue;

            auto desc{state.process->memory.Get(ptr)};
            if (!desc || desc->state != memory::states::Unmapped || (static_cast<size_t>(ptr - desc->ptr) + size) < desc->size)
                continue;
        } while (!ptr);

        auto loadInfo{state.loader->LoadExecutable(state.process, state, executable, static_cast<size_t>(ptr - state.process->memory.base.data()), util::HexDump(hash) + ".nro")};

        response.Push(loadInfo.entry);
        return {};
    }

    Result IRoInterface::UnloadModule(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        Logger::Error("Module unloading is unimplemented!");
        return {};

    }

    Result IRoInterface::RegisterModuleInfo(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};

    }

    Result IRoInterface::UnregisterModuleInfo(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};

    }

    Result IRoInterface::RegisterProcessHandle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};

    }

    Result IRoInterface::RegisterProcessModuleInfo(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};

    }
}
