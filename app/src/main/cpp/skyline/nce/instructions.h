// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <common.h>

namespace skyline {
    namespace regs {
        enum X { X0, X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17, X18, X19, X20, X21, X22, X23, X24, X25, X26, X27, X28, X29, X30 };
        enum W { W0, W1, W2, W3, W4, W5, W6, W7, W8, W9, W10, W11, W12, W13, W14, W15, W16, W17, W18, W19, W20, W21, W22, W23, W24, W25, W26, W27, W28, W29, W30 };
        enum S { Sp, Pc };
    }

    namespace instr {
        /**
         * @brief A bit-field struct that encapsulates a BRK instruction. See https://developer.arm.com/docs/ddi0596/latest/base-instructions-alphabetic-order/brk-breakpoint-instruction.
         */
        struct Brk {
            /**
             * @brief Creates a BRK instruction with a specific immediate value, used for generating BRK opcodes
             * @param value The immediate value of the instruction
             */
            inline constexpr Brk(u16 value) {
                sig0 = 0x0;
                this->value = value;
                sig1 = 0x6A1;
            }

            /**
             * @brief Returns if the opcode is valid or not
             * @return If the opcode represents a valid BRK instruction
             */
            inline constexpr bool Verify() {
                return (sig0 == 0x0 && sig1 == 0x6A1);
            }

            union {
                struct {
                    u8 sig0   : 5;  //!< 5-bit signature (0x0)
                    u32 value : 16; //!< 16-bit immediate value
                    u16 sig1  : 11; //!< 11-bit signature (0x6A1)
                };
                u32 raw{}; //!< The raw value of the instruction
            };
        };
        static_assert(sizeof(Brk) == sizeof(u32));

        /**
         * @brief A bit-field struct that encapsulates a SVC instruction. See https://developer.arm.com/docs/ddi0596/latest/base-instructions-alphabetic-order/svc-supervisor-call.
         */
        struct Svc {
            /**
             * @brief Returns if the opcode is valid or not
             * @return If the opcode represents a valid SVC instruction
             */
            inline constexpr bool Verify() {
                return (sig0 == 0x1 && sig1 == 0x6A0);
            }

            union {
                struct {
                    u8 sig0   : 5;  //!< 5-bit signature (0x0)
                    u32 value : 16; //!< 16-bit immediate value
                    u16 sig1  : 11; //!< 11-bit signature (0x6A1)
                };
                u32 raw{}; //!< The raw value of the instruction
            };
        };
        static_assert(sizeof(Svc) == sizeof(u32));

        /**
         * @brief A bit-field struct that encapsulates a MRS instruction. See https://developer.arm.com/docs/ddi0596/latest/base-instructions-alphabetic-order/mrs-move-system-register.
         */
        struct Mrs {
            /**
             * @brief Creates a MRS instruction, used for generating BRK opcodes
             * @param srcReg The source system register
             * @param dstReg The destination Xn register
             */
            inline constexpr Mrs(u32 srcReg, regs::X dstReg) {
                this->srcReg = srcReg;
                this->destReg = dstReg;
                sig = 0xD53;
            }

            /**
             * @brief Returns if the opcode is valid or not
             * @return If the opcode represents a valid MRS instruction
             */
            inline constexpr bool Verify() {
                return (sig == 0xD53);
            }

            union {
                struct {
                    u8 destReg  : 5; //!< 5-bit destination register
                    u32 srcReg : 15; //!< 15-bit source register
                    u16 sig    : 12; //!< 16-bit signature (0xD53)
                };
                u32 raw{}; //!< The raw value of the instruction
            };
        };
        static_assert(sizeof(Mrs) == sizeof(u32));

        /**
         * @brief A bit-field struct that encapsulates a B instruction. See https://developer.arm.com/docs/ddi0596/latest/base-instructions-alphabetic-order/b-branch.
         */
        struct B {
          public:
            /**
             * @brief Creates a B instruction with a specific offset
             * @param offset The offset to encode in the instruction (Should be 32-bit aligned)
             */
            inline constexpr B(i64 offset) {
                this->offset = static_cast<i32>(offset / 4);
                sig = 0x5;
            }

            /**
             * @brief Returns the offset of the instruction
             * @return The offset encoded within the instruction
             */
            inline constexpr i32 Offset() {
                return offset * 4;
            }

            /**
             * @brief Returns if the opcode is valid or not
             * @return If the opcode represents a valid Branch instruction
             */
            inline constexpr bool Verify() {
                return (sig == 0x5);
            }

            union {
                struct {
                    i32 offset : 26; //!< 26-bit branch offset
                    u8 sig     : 6;  //!< 6-bit signature (0x5)
                };
                u32 raw{}; //!< The raw value of the instruction
            };
        };
        static_assert(sizeof(B) == sizeof(u32));

        /**
         * @brief A bit-field struct that encapsulates a BL instruction. See https://developer.arm.com/docs/ddi0596/latest/base-instructions-alphabetic-order/b-branch.
         */
        struct BL {
          public:
            /**
             * @brief Creates a BL instruction with a specific offset
             * @param offset The offset to encode in the instruction (Should be 32-bit aligned)
             */
            inline constexpr BL(i64 offset) {
                this->offset = static_cast<i32>(offset / 4);
                sig = 0x25;
            }

            /**
             * @brief Returns the offset of the instruction
             * @return The offset encoded within the instruction
             */
            inline constexpr i32 Offset() {
                return offset * 4;
            }

            /**
             * @brief Returns if the opcode is valid or not
             * @return If the opcode represents a valid Branch Linked instruction
             */
            inline constexpr bool Verify() {
                return (sig == 0x25);
            }

            union {
                struct {
                    i32 offset : 26; //!< 26-bit branch offset
                    u8 sig     : 6;  //!< 6-bit signature (0x25)
                };
                u32 raw{}; //!< The raw value of the instruction
            };
        };
        static_assert(sizeof(BL) == sizeof(u32));

        /**
         * @brief A bit-field struct that encapsulates a MOVZ instruction. See https://developer.arm.com/docs/ddi0596/e/base-instructions-alphabetic-order/movz-move-wide-with-zero.
         */
        struct Movz {
          public:
            /**
             * @brief Creates a MOVZ instruction
             * @param destReg The destination Xn register to store the value in
             * @param imm16 The 16-bit value to store
             * @param shift The offset (in units of 16-bits) in the register to store the value at
             */
            inline constexpr Movz(regs::X destReg, u16 imm16, u8 shift = 0) {
                this->destReg = static_cast<u8>(destReg);
                this->imm16 = imm16;
                hw = shift;
                sig = 0xA5;
                sf = 1;
            }

            /**
             * @brief Creates a MOVZ instruction
             * @param destReg The destination Wn register to store the value in
             * @param imm16 The 16-bit value to store
             * @param shift The offset (in units of 16-bits) in the register to store the value at
             */
            inline constexpr Movz(regs::W destReg, u16 imm16, u8 shift = 0) {
                this->destReg = static_cast<u8>(destReg);
                this->imm16 = imm16;
                hw = shift;
                sig = 0xA5;
                sf = 0;
            }

            /**
             * @brief Returns the offset of the instruction
             * @return The offset encoded within the instruction (In Bytes)
             */
            inline constexpr u8 Shift() {
                return static_cast<u8>(hw * sizeof(u16));
            }

            /**
             * @brief Returns if the opcode is valid or not
             * @return If the opcode represents a valid MOVZ instruction
             */
            inline constexpr bool Verify() {
                return (sig == 0xA5);
            }

            union {
                struct __attribute__((packed)) {
                    u8 destReg : 5;  //!< 5-bit destination register
                    u16 imm16  : 16; //!< 16-bit immediate value
                    u8 hw      : 2;  //!< 2-bit offset
                    u8 sig     : 8;  //!< 8-bit signature (0xA5)
                    u8 sf      : 1;  //!< 1-bit register type
                };
                u32 raw{}; //!< The raw value of the instruction
            };
        };
        static_assert(sizeof(Movz) == sizeof(u32));

        /**
         * @brief A bit-field struct that encapsulates a MOVK instruction. See https://developer.arm.com/docs/ddi0596/e/base-instructions-alphabetic-order/movk-move-wide-with-keep.
         */
        struct Movk {
          public:
            /**
             * @brief Creates a MOVK instruction
             * @param destReg The destination Xn register to store the value in
             * @param imm16 The 16-bit value to store
             * @param shift The offset (in units of 16-bits) in the register to store the value at
             */
            inline constexpr Movk(regs::X destReg, u16 imm16, u8 shift = 0) {
                this->destReg = static_cast<u8>(destReg);
                this->imm16 = imm16;
                hw = shift;
                sig = 0xE5;
                sf = 1;
            }

            /**
             * @brief Creates a MOVK instruction
             * @param destReg The destination Wn register to store the value in
             * @param imm16 The 16-bit value to store
             * @param shift The offset (in units of 16-bits) in the register to store the value at
             */
            inline constexpr Movk(regs::W destReg, u16 imm16, u8 shift = 0) {
                this->destReg = static_cast<u8>(destReg);
                this->imm16 = imm16;
                hw = shift;
                sig = 0xE5;
                sf = 0;
            }

            /**
             * @brief Returns the offset of the instruction
             * @return The offset encoded within the instruction (In Bytes)
             */
            inline constexpr u8 Shift() {
                return static_cast<u8>(hw * sizeof(u16));
            }

            /**
             * @brief Returns if the opcode is valid or not
             * @return If the opcode represents a valid MOVK instruction
             */
            inline constexpr bool Verify() {
                return (sig == 0xE5);
            }

            union {
                struct __attribute__((packed)) {
                    u8 destReg : 5;  //!< 5-bit destination register
                    u16 imm16  : 16; //!< 16-bit immediate value
                    u8 hw      : 2;  //!< 2-bit offset
                    u8 sig     : 8;  //!< 8-bit signature (0xA5)
                    u8 sf      : 1;  //!< 1-bit register type
                };
                u32 raw{}; //!< The raw value of the instruction
            };
        };
        static_assert(sizeof(Movk) == sizeof(u32));

        /**
         * @param destination The destination register of the operation
         * @param value The value to insert into the register
         * @return A array with the instructions to insert the value
         */
        template<typename Type>
        inline constexpr std::array<u32, sizeof(Type) / sizeof(u16)> MoveRegister(regs::X destination, Type value) {
            std::array<u32, sizeof(Type) / sizeof(u16)> instructions;

            auto valuePointer = reinterpret_cast<u16 *>(&value);
            u8 offset{};

            for (auto &instruction : instructions) {
                if (offset)
                    instruction = instr::Movk(destination, *(valuePointer + offset), offset).raw;
                else
                    instruction = instr::Movz(destination, *(valuePointer + offset), offset).raw;

                offset++;
            }

            return instructions;
        }

        /**
         * @brief A bit-field struct that encapsulates a MOV (Register) instruction. See https://developer.arm.com/docs/ddi0596/e/base-instructions-alphabetic-order/mov-register-move-register-an-alias-of-orr-shifted-register.
         */
        struct Mov {
          public:
            /**
             * @brief Creates a MOV (Register) instruction
             * @param destReg The destination Xn register to store the value in
             * @param srcReg The source Xn register to retrieve the value from
             */
            inline constexpr Mov(regs::X destReg, regs::X srcReg) {
                this->destReg = static_cast<u8>(destReg);
                sig0 = 0x1F;
                imm6 = 0;
                this->srcReg = static_cast<u8>(srcReg);
                sig1 = 0x150;
                sf = 1;
            }

            /**
             * @brief Creates a MOV instruction
             * @param destReg The destination Wn register to store the value in
             * @param srcReg The source Wn register to retrieve the value from
             */
            inline constexpr Mov(regs::W destReg, regs::W srcReg) {
                this->destReg = static_cast<u8>(destReg);
                sig0 = 0x1F;
                imm6 = 0;
                this->srcReg = static_cast<u8>(srcReg);
                sig1 = 0x150;
                sf = 0;
            }

            /**
             * @brief Returns if the opcode is valid or not
             * @return If the opcode represents a valid MOVZ instruction
             */
            inline constexpr bool Verify() {
                return (sig0 == 0x1F) && (sig1 == 0x150);
            }

            union {
                struct __attribute__((packed)) {
                    u8 destReg : 5;  //!< 5-bit destination register
                    u8 sig0    : 5;  //!< 5-bit signature (0x1F)
                    u8 imm6    : 6;  //!< 6-bit immediate value
                    u8 srcReg  : 5;  //!< 5-bit source register
                    u16 sig1   : 10; //!< 10-bit signature (0x150)
                    u8 sf      : 1;  //!< 1-bit register type
                };
                u32 raw{}; //!< The raw value of the instruction
            };
        };
        static_assert(sizeof(Mov) == sizeof(u32));

        /**
         * @brief A bit-field struct that encapsulates a LDR (immediate) instruction. See https://developer.arm.com/docs/ddi0596/e/base-instructions-alphabetic-order/ldr-immediate-load-register-immediate.
         */
        struct Ldr {
          public:
            /**
             * @brief Creates a LDR (immediate) instruction
             * @param raw The raw value of the whole instruction
             */
            inline constexpr Ldr(u32 raw) : raw(raw) {}

            /**
             * @brief Returns if the opcode is valid or not
             * @return If the opcode represents a valid FCVTZU instruction
             */
            inline constexpr bool Verify() {
                return (sig0 == 0x0 && sig1 == 0x1CA && sig2 == 0x1);
            }

            union {
                struct __attribute__((packed)) {
                    u8 destReg : 5; //!< 5-bit destination register
                    u8 srcReg  : 5; //!< 5-bit source register
                    u8 sig0    : 2; //!< 2-bit signature (0x0)
                    u16 imm    : 9; //!< 6-bit immediate value
                    u16 sig1   : 9; //!< 9-bit signature (0x1CA)
                    u8 sf      : 1; //!< 1-bit register type
                    u8 sig2    : 1; //!< 1-bit signature (0x1)
                };
                u32 raw{}; //!< The raw value of the instruction
            };
        };
        static_assert(sizeof(Ldr) == sizeof(u32));
    }
}
