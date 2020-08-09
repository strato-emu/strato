// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>

namespace skyline::gpu {
    namespace engine {
        class Maxwell3D;
    }

    class MacroInterpreter {
      private:
        /**
         * @brief This holds a single macro opcode
         */
        union Opcode {
            enum class Operation : u8 {
                AluRegister = 0,
                AddImmediate = 1,
                BitfieldReplace = 2,
                BitfieldExtractShiftLeftImmediate = 3,
                BitfieldExtractShiftLeftRegister = 4,
                ReadImmediate = 5,
                Branch = 7,
            };

            enum class AssignmentOperation : u8 {
                IgnoreAndFetch = 0,
                Move = 1,
                MoveAndSetMethod = 2,
                FetchAndSend = 3,
                MoveAndSend = 4,
                FetchAndSetMethod = 5,
                MoveAndSetMethodThenFetchAndSend = 6,
                MoveAndSetMethodThenSendHigh = 7,
            };

            enum class AluOperation : u8 {
                Add = 0,
                AddWithCarry = 1,
                Subtract = 2,
                SubtractWithBorrow = 3,
                BitwiseXor = 8,
                BitwiseOr = 9,
                BitwiseAnd = 10,
                BitwiseAndNot = 11,
                BitwiseNand = 12,
            };

            enum class BranchCondition : u8 {
                Zero = 0,
                NonZero = 1,
            };

            struct __attribute__((__packed__)) {
                Operation operation : 3;
                u8 _pad0_ : 1;
                AssignmentOperation assignmentOperation : 3;
            };

            struct __attribute__((__packed__)) {
                u8 _pad1_ : 4;
                BranchCondition branchCondition : 1;
                u8 noDelay : 1;
                u8 _pad2_ : 1;
                u8 exit : 1;
                u8 dest : 3;
                u8 srcA : 3;
                u8 srcB : 3;
                AluOperation aluOperation : 5;
            };

            struct __attribute__((__packed__)) {
                u16 _pad3_ : 14;
                i32 immediate : 18;
            };

            struct __attribute__((__packed__)) {
                u32 _pad_ : 17;
                u8 srcBit : 5;
                u8 size : 5;
                u8 destBit : 5;

                u32 GetMask() {
                    return (1 << size) - 1;
                }
            } bitfield;

            u32 raw;
        };
        static_assert(sizeof(Opcode) == sizeof(u32));

        /**
         * @brief This holds information about the Maxwell 3D method to be called in 'Send'
         */
        union MethodAddress {
            struct {
                u16 address : 12;
                u8 increment : 6;
            };

            u32 raw;
        };

        engine::Maxwell3D &maxwell3D;

        std::array<u32, 8> registers{};

        Opcode *opcode{};
        const u32 *argument{};
        MethodAddress methodAddress{};
        bool carryFlag{};

        /**
         * @brief Steps forward one macro instruction, including delay slots
         * @param delayedOpcode The target opcode to be jumped to after executing the instruction
         */
        bool Step(Opcode *delayedOpcode = nullptr);

        /**
         * @brief Performs an ALU operation on the given source values and returns the result as a u32
         */
        u32 HandleAlu(Opcode::AluOperation operation, u32 srcA, u32 srcB);

        /**
         * @brief Handles an opcode's assignment operation
         */
        void HandleAssignment(Opcode::AssignmentOperation operation, u8 reg, u32 result);

        /**
         * @brief Sends a method call to the Maxwell 3D
         */
        void Send(u32 argument);

        void WriteRegister(u8 reg, u32 value);

      public:
        MacroInterpreter(engine::Maxwell3D &maxwell3D) : maxwell3D(maxwell3D) {}

        /**
         * @brief Executes a GPU macro from macro memory with the given arguments
         */
        void Execute(size_t offset, const std::vector<u32> &args);
    };
}
