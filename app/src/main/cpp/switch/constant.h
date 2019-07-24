#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

namespace lightSwitch {
    typedef std::runtime_error exception;

    namespace constant {
        constexpr uint64_t base_addr = 0x80000000;
        constexpr uint64_t stack_addr = 0x3000000;
        constexpr size_t stack_size = 0x1000000;
        constexpr uint64_t tls_addr = 0x2000000;
        constexpr size_t tls_size = 0x1000;
        constexpr uint32_t nro_magic = 0x304F524E; // NRO0 in reverse
        constexpr uint_t svc_unimpl = 0x177202; // "Unimplemented behaviour"
        constexpr uint32_t base_handle_index = 0xd001;
    };
}