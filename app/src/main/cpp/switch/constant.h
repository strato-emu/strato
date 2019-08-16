#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

namespace lightSwitch {
    typedef std::runtime_error exception;

    namespace constant {
        constexpr uint64_t base_addr = 0x80000000;
        constexpr uint64_t stack_addr = 0x3000000;
        constexpr size_t stack_size = 280; //0x1000000
        constexpr uint64_t tls_addr = 0x2000000;
        constexpr size_t tls_size = 0x1000;
        constexpr uint32_t nro_magic = 0x304F524E; // NRO0 in reverse
        constexpr uint_t svc_unimpl = 0x177202; // "Unimplemented behaviour"
        constexpr uint32_t base_handle_index = 0xD001;
        constexpr uint16_t svc_last = 0x7F;
        constexpr uint8_t num_regs = 31;
        constexpr uint32_t tpidrro_el0 = 0x5E83; // ID of tpidrro_el0 in MRS
    };

    namespace instr {
        // https://developer.arm.com/docs/ddi0596/latest/base-instructions-alphabetic-order/brk-breakpoint-instruction
        // For some reason if value is set to uint16_t it isn't read correctly ?
        struct brk {
            brk(uint16_t val) {
                start = 0x0; // First 5 bits of an BRK instruction are 0
                value = val;
                end = 0x6A1; // Last 11 bits of an BRK instruction stored as uint16_t
            }

            bool verify() {
                return (start == 0x0 && end == 0x6A1);
            }

            uint8_t start:5;
            uint32_t value:16;
            uint16_t end:11;
        };

        // https://developer.arm.com/docs/ddi0596/latest/base-instructions-alphabetic-order/svc-supervisor-call
        struct svc {
            bool verify() {
                return (start == 0x1 && end == 0x6A0);
            }

            uint8_t start:5;
            uint32_t value:16;
            uint16_t end:11;
        };

        // https://developer.arm.com/docs/ddi0596/latest/base-instructions-alphabetic-order/mrs-move-system-register
        struct mrs {
            bool verify() {
                return (end == 0xD53);
            }

            uint8_t Xt:5;
            uint32_t Sreg:15;
            uint16_t end:12;
        };
    };

    enum xreg {
        x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30
    };
    enum wreg {
        w0, w1, w2, w3, w4, w5, w6, w7, w8, w9, w10, w11, w12, w13, w14, w15, w16, w17, w18, w19, w20, w21, w22, w23, w24, w25, w26, w27, w28, w29, w30
    };
}