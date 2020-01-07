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
            explicit Brk(u16 value) {
                sig0 = 0x0; // First 5 bits of a BRK instruction are 0
                this->value = value;
                sig1 = 0x6A1; // Last 11 bits of a BRK instruction stored as u16
            }

            /**
             * @brief Returns if the opcode is valid or not
             * @return If the opcode represents a valid BRK instruction
             */
            inline bool Verify() {
                return (sig0 == 0x0 && sig1 == 0x6A1);
            }

            union {
                struct {
                    u8 sig0   : 5;
                    u32 value : 16;
                    u16 sig1  : 11;
                };
                u32 raw{};
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
            inline bool Verify() {
                return (sig0 == 0x1 && sig1 == 0x6A0);
            }

            union {
                struct {
                    u8 sig0   : 5;
                    u32 value : 16;
                    u16 sig1  : 11;
                };
                u32 raw{};
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
            Mrs(u32 srcReg, regs::X dstReg) {
                this->srcReg = srcReg;
                this->destReg = dstReg;
                sig = 0xD53; // Last 12 bits of a MRS instruction stored as u16
            }

            /**
             * @brief Returns if the opcode is valid or not
             * @return If the opcode represents a valid MRS instruction
             */
            inline bool Verify() {
                return (sig == 0xD53);
            }

            union {
                struct {
                    u8 destReg  : 5;
                    u32 srcReg : 15;
                    u16 sig    : 12;
                };
                u32 raw{};
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
            explicit B(i64 offset) {
                this->offset = static_cast<i32>(offset / 4);
                sig = 0x5;
            }

            /**
             * @brief Returns the offset of the instruction
             * @return The offset encoded within the instruction
             */
            inline i32 Offset() {
                return offset * 4;
            }

            /**
             * @brief Returns if the opcode is valid or not
             * @return If the opcode represents a valid Branch instruction
             */
            inline bool Verify() {
                return (sig == 0x5);
            }

            union {
                struct {
                    i32 offset : 26;
                    u8 sig     : 6;
                };
                u32 raw{};
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
            explicit BL(i64 offset) {
                this->offset = static_cast<i32>(offset / 4);
                sig = 0x25;
            }

            /**
             * @brief Returns the offset of the instruction
             * @return The offset encoded within the instruction
             */
            inline i32 Offset() {
                return offset * 4;
            }

            /**
             * @brief Returns if the opcode is valid or not
             * @return If the opcode represents a valid Branch Linked instruction
             */
            inline bool Verify() {
                return (sig == 0x85);
            }

            union {
                struct {
                    i32 offset : 26;
                    u8 sig     : 6;
                };
                u32 raw{};
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
             * @param shift The offset (in bits and 16-bit aligned) in the register to store the value at
             */
            Movz(regs::X destReg, u16 imm16, u8 shift = 0) {
                this->destReg = static_cast<u8>(destReg);
                this->imm16 = imm16;
                hw = static_cast<u8>(shift / 16);
                sig = 0xA5;
                sf = 1;
            }

            /**
             * @brief Creates a MOVZ instruction
             * @param destReg The destination Wn register to store the value in
             * @param imm16 The 16-bit value to store
             * @param shift The offset (in bits and 16-bit aligned) in the register to store the value at
             */
            Movz(regs::W destReg, u16 imm16, u8 shift = 0) {
                this->destReg = static_cast<u8>(destReg);
                this->imm16 = imm16;
                hw = static_cast<u8>(shift / 16);
                sig = 0xA5;
                sf = 0;
            }

            /**
             * @brief Returns the offset of the instruction
             * @return The offset encoded within the instruction
             */
            inline u8 Shift() {
                return static_cast<u8>(hw * 16);
            }

            /**
             * @brief Returns if the opcode is valid or not
             * @return If the opcode represents a valid MOVZ instruction
             */
            inline bool Verify() {
                return (sig == 0xA5);
            }

            union {
                struct __attribute__((packed)) {
                    u8 destReg : 5;
                    u16 imm16  : 16;
                    u8 hw      : 2;
                    u8 sig     : 8;
                    u8 sf      : 1;
                };
                u32 raw{};
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
             * @param shift The offset (in bits and 16-bit aligned) in the register to store the value at
             */
            Movk(regs::X destReg, u16 imm16, u8 shift = 0) {
                this->destReg = static_cast<u8>(destReg);
                this->imm16 = imm16;
                hw = static_cast<u8>(shift / 16);
                sig = 0xE5;
                sf = 1;
            }

            /**
             * @brief Creates a MOVK instruction
             * @param destReg The destination Wn register to store the value in
             * @param imm16 The 16-bit value to store
             * @param shift The offset (in bits and 16-bit aligned) in the register to store the value at
             */
            Movk(regs::W destReg, u16 imm16, u8 shift = 0) {
                this->destReg = static_cast<u8>(destReg);
                this->imm16 = imm16;
                hw = static_cast<u8>(shift / 16);
                sig = 0xE5;
                sf = 0;
            }

            /**
             * @brief Returns the offset of the instruction
             * @return The offset encoded within the instruction
             */
            inline u8 Shift() {
                return static_cast<u8>(hw * 16);
            }

            /**
             * @brief Returns if the opcode is valid or not
             * @return If the opcode represents a valid MOVK instruction
             */
            inline bool Verify() {
                return (sig == 0xE5);
            }

            union {
                struct __attribute__((packed)) {
                    u8 destReg : 5;
                    u16 imm16  : 16;
                    u8 hw      : 2;
                    u8 sig     : 8;
                    u8 sf      : 1;
                };
                u32 raw{};
            };
        };
        static_assert(sizeof(Movk) == sizeof(u32));

        const std::array<u32, 4> MoveU64Reg(regs::X destReg, u64 value) {
            union {
                u64 val;
                struct {
                    u16 v0;
                    u16 v16;
                    u16 v32;
                    u16 v64;
                };
            } val;
            val.val = value;
            std::array<u32, 4> instr;
            instr::Movz mov0(destReg, val.v0, 0);
            instr[0] = mov0.raw;
            instr::Movk mov16(destReg, val.v16, 16);
            instr[1] = mov16.raw;
            instr::Movk mov32(destReg, val.v32, 32);
            instr[2] = mov32.raw;
            instr::Movk mov64(destReg, val.v64, 48);
            instr[3] = mov64.raw;
            return instr;
        }

        const std::array<u32, 2> MoveU32Reg(regs::X destReg, u32 value) {
            union {
                u32 val;
                struct {
                    u16 v0;
                    u16 v16;
                };
            } val;
            val.val = value;
            std::array<u32, 2> instr;
            instr::Movz mov0(destReg, val.v0, 0);
            instr[0] = mov0.raw;
            instr::Movk mov16(destReg, val.v16, 16);
            instr[1] = mov16.raw;
            return instr;
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
            Mov(regs::X destReg, regs::X srcReg) {
                this->destReg = static_cast<u8>(destReg);
                zeroReg = 0x1F;
                imm6 = 0;
                this->srcReg = static_cast<u8>(srcReg);
                sig = 0x150;
                sf = 1;
            }

            /**
             * @brief Creates a MOV instruction
             * @param destReg The destination Wn register to store the value in
             * @param srcReg The source Wn register to retrieve the value from
             */
            Mov(regs::W destReg, regs::W srcReg) {
                this->destReg = static_cast<u8>(destReg);
                zeroReg = 0x1F;
                imm6 = 0;
                this->srcReg = static_cast<u8>(srcReg);
                sig = 0x150;
                sf = 0;
            }

            /**
             * @brief Returns if the opcode is valid or not
             * @return If the opcode represents a valid MOVZ instruction
             */
            inline bool Verify() {
                return (sig == 0x150);
            }

            union {
                struct __attribute__((packed)) {
                    u8 destReg : 5;
                    u8 zeroReg : 5;
                    u8 imm6    : 6;
                    u8 srcReg  : 5;
                    u16 sig    : 10;
                    u8 sf      : 1;
                };
                u32 raw{};
            };
        };
        static_assert(sizeof(Mov) == sizeof(u32));
    }
}
