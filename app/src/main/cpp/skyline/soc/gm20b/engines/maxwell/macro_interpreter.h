// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>

namespace skyline::soc::gm20b::engine::maxwell3d {
    class Maxwell3D; // A forward declaration of Maxwell3D as we don't want to import it here

    /**
     * @brief The MacroInterpreter class handles interpreting macros. Macros are small programs that run on the GPU and are used for things like instanced rendering
     */
    class MacroInterpreter {
      private:
        #pragma pack(push, 1)
        union Opcode {
            u32 raw;

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

            struct {
                Operation operation : 3;
                u8 _pad0_ : 1;
                AssignmentOperation assignmentOperation : 3;
            };

            struct {
                u8 _pad1_ : 4;
                BranchCondition branchCondition : 1;
                bool noDelay : 1;
                u8 _pad2_ : 1;
                u8 exit : 1;
                u8 dest : 3;
                u8 srcA : 3;
                u8 srcB : 3;
                AluOperation aluOperation : 5;
            };

            struct {
                u16 _pad3_ : 14;
                i32 immediate : 18;
            };

            struct {
                u32 _pad_ : 17;
                u8 srcBit : 5;
                u8 size : 5;
                u8 destBit : 5;

                u32 GetMask() {
                    return (1 << size) - 1;
                }
            } bitfield;
        };
        static_assert(sizeof(Opcode) == sizeof(u32));
        #pragma pack(pop)

        /**
         * @brief Metadata about the Maxwell 3D method to be called in 'Send'
         */
        union MethodAddress {
            u32 raw;
            struct {
                u16 address : 12;
                u8 increment : 6;
            };
        };

        Maxwell3D &maxwell3D; //!< A reference to the parent engine object

        Opcode *opcode{}; //!< A pointer to the instruction that is currently being executed
        std::array<u32, 8> registers{}; //!< The state of all the general-purpose registers in the macro interpreter
        const u32 *argument{}; //!< A pointer to the argument buffer for the program, it is read from sequentially
        MethodAddress methodAddress{};
        bool carryFlag{}; //!< A flag representing if an arithmetic operation has set the most significant bit

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

        /**
         * @brief Writes to the specified register with sanity checking
         */
        void WriteRegister(u8 reg, u32 value);

      public:
        MacroInterpreter(Maxwell3D &maxwell3D) : maxwell3D(maxwell3D) {}

        /**
         * @brief Executes a GPU macro from macro memory with the given arguments
         */
        void Execute(size_t offset, const std::vector<u32> &args);
    };
}
