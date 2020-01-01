#pragma once

namespace skyline {
    namespace guest {
        constexpr size_t saveCtxSize = 20 * sizeof(u32);
        constexpr size_t loadCtxSize = 20 * sizeof(u32);
        extern "C" void saveCtx(void);
        extern "C" void loadCtx(void);
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
                start = 0x0; // First 5 bits of a BRK instruction are 0
                this->value = value;
                end = 0x6A1; // Last 11 bits of a BRK instruction stored as u16
            }

            /**
             * @brief Returns if the opcode is valid or not
             * @return If the opcode represents a valid BRK instruction
             */
            inline bool Verify() {
                return (start == 0x0 && end == 0x6A1);
            }

            union {
                struct {
                    u8 start  : 5;
                    u32 value : 16;
                    u16 end   : 11;
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
                return (start == 0x1 && end == 0x6A0);
            }

            union {
                struct {
                    u8 start  : 5;
                    u32 value : 16;
                    u16 end   : 11;
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
            Mrs(u32 srcReg, u8 dstReg) {
                this->srcReg = srcReg;
                this->dstReg = dstReg;
                end = 0xD53; // Last 12 bits of a MRS instruction stored as u16
            }

            /**
             * @brief Returns if the opcode is valid or not
             * @return If the opcode represents a valid MRS instruction
             */
            inline bool Verify() {
                return (end == 0xD53);
            }

            union {
                struct {
                    u8 dstReg  : 5;
                    u32 srcReg : 15;
                    u16 end    : 12;
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
            explicit B(i64 offset) {
                this->offset = static_cast<i32>(offset / 4);
                end = 0x5;
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
                return (end == 0x5);
            }

            union {
                struct {
                    i32 offset : 26;
                    u8 end     : 6;
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
            explicit BL(i64 offset) {
                this->offset = static_cast<i32>(offset / 4);
                end = 0x25;
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
                return (end == 0x85);
            }

            union {
                struct {
                    i32 offset : 26;
                    u8 end     : 6;
                };
                u32 raw{};
            };
        };

        static_assert(sizeof(BL) == sizeof(u32));
    }
}
