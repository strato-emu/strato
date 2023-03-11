// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>
#include <soc/gm20b/macro/macro_state.h>

#define U32_OFFSET(regs, field) (offsetof(regs, field) / sizeof(u32))
#define ENGINE_OFFSET(field) (sizeof(decltype(Registers::field)) - sizeof(std::remove_reference_t<decltype(*Registers::field)>)) / sizeof(u32)
#define ENGINE_STRUCT_OFFSET(field, member) ENGINE_OFFSET(field) + U32_OFFSET(std::remove_reference_t<decltype(*Registers::field)>, member)
#define ENGINE_STRUCT_STRUCT_OFFSET(field, member, submember) ENGINE_STRUCT_OFFSET(field, member) + U32_OFFSET(std::remove_reference_t<decltype(Registers::field->member)>, submember)
#define ENGINE_STRUCT_ARRAY_OFFSET(field, member, index) ENGINE_STRUCT_OFFSET(field, member) + ((sizeof(std::remove_reference_t<decltype(Registers::field->member[0])>) / sizeof(u32)) * index)
#define ENGINE_ARRAY_OFFSET(field, index) ENGINE_OFFSET(field) + ((sizeof(std::remove_reference_t<decltype(Registers::field[0])>) / sizeof(u32)) * index)
#define ENGINE_ARRAY_STRUCT_OFFSET(field, index, member) ENGINE_ARRAY_OFFSET(field, index) + U32_OFFSET(std::remove_reference_t<decltype(Registers::field[0])>, member)
#define ENGINE_ARRAY_STRUCT_STRUCT_OFFSET(field, index, member, submember) ENGINE_ARRAY_STRUCT_OFFSET(field, index, member) + U32_OFFSET(decltype(Registers::field[0].member), submember)

#define ENGINE_CASE(field, content) case ENGINE_OFFSET(field): {                        \
        auto field{util::BitCast<std::remove_reference_t<decltype(*registers.field)>>(argument)}; \
        content                                                                                   \
        return;                                                                                   \
    }
#define ENGINE_CASE_BASE(fieldName, fieldAccessor, offset, content) case offset: {                    \
        auto fieldName{util::BitCast<std::remove_reference_t<decltype(registers.fieldAccessor)>>(argument)}; \
        content                                                                                              \
        return;                                                                                              \
    }
#define ENGINE_STRUCT_CASE(field, member, content) ENGINE_CASE_BASE(member, field->member, ENGINE_STRUCT_OFFSET(field, member), content)
#define ENGINE_STRUCT_STRUCT_CASE(field, member, submember, content) ENGINE_CASE_BASE(submember, field->member.submember, ENGINE_STRUCT_STRUCT_OFFSET(field, member, submember), content)
#define ENGINE_STRUCT_ARRAY_CASE(field, member, index, content) ENGINE_CASE_BASE(member, field->member[index], ENGINE_STRUCT_ARRAY_OFFSET(field, member, index), content)
#define ENGINE_ARRAY_CASE(field, index, content) ENGINE_CASE_BASE(field, field[index], ENGINE_ARRAY_OFFSET(field, index), content)
#define ENGINE_ARRAY_STRUCT_CASE(field, index, member, content) ENGINE_CASE_BASE(member, field[index].member, ENGINE_ARRAY_STRUCT_OFFSET(field, index, member), content)
#define ENGINE_ARRAY_STRUCT_STRUCT_CASE(field, index, member, submember, content) ENGINE_CASE_BASE(submember, field[index].member.submember, ENGINE_ARRAY_STRUCT_STRUCT_OFFSET(field, index, member, submember), content)

namespace skyline::soc::gm20b::engine {
    /**
     * @brief A 40-bit GMMU virtual address with register-packing
     * @note The registers pack the address with big-endian ordering (but with 32 bit words)
     */
    struct Address {
        u32 high;
        u32 low;

        operator u64() const {
            return (static_cast<u64>(high) << 32) | low;
        }
    };
    static_assert(sizeof(Address) == sizeof(u64));

    struct TexSamplerPool {
        Address offset;
        u32 maximumIndex;
    };
    static_assert(sizeof(TexSamplerPool) == sizeof(u32) * 3);

    struct TexHeaderPool {
        Address offset;
        u32 maximumIndex;
    };
    static_assert(sizeof(TexHeaderPool) == sizeof(u32) * 3);

    struct BindlessTexture {
        u8 constantBufferSlotSelect : 5;
        u32 _pad0_ : 27;
    };
    static_assert(sizeof(BindlessTexture) == sizeof(u32));

    constexpr u32 EngineMethodsEnd{0xE00}; //!< All methods above this are passed to the MME on supported engines

    /**
     * @brief Returns current time in GPU ticks
     */
    u64 GetGpuTimeTicks();

    /**
     * @brief The MacroEngineBase interface provides an interface that can be used by engines to allow interfacing with the macro executer
     */
    struct MacroEngineBase {
        MacroState &macroState;

        struct {
            u32 index{std::numeric_limits<u32>::max()};
            std::vector<GpfifoArgument> arguments;

            bool Valid() {
                return index != std::numeric_limits<u32>::max();
            }

            void Reset() {
                index = std::numeric_limits<u32>::max();
                arguments.clear();
            }
        } macroInvocation{}; //!< Data for a macro that is pending execution

        MacroEngineBase(MacroState &macroState);

        virtual ~MacroEngineBase() = default;

        /**
         * @brief Calls an engine method with the given parameters
         */
        virtual void CallMethodFromMacro(u32 method, u32 argument) = 0;

        /**
         * @brief Reads the current value for the supplied method
         */
        virtual u32 ReadMethodFromMacro(u32 method) = 0;

        virtual void DrawInstanced(u32 drawTopology, u32 vertexArrayCount, u32 instanceCount, u32 vertexArrayStart, u32 globalBaseInstanceIndex) {
            throw exception("DrawInstanced is not implemented for this engine");
        }

        virtual void DrawIndexedInstanced(u32 drawTopology, u32 indexBufferCount, u32 instanceCount, u32 globalBaseVertexIndex, u32 indexBufferFirst, u32 globalBaseInstanceIndex) {
            throw exception("DrawIndexedInstanced is not implemented for this engine");
        }

        virtual void DrawIndexedIndirect(u32 drawTopology, span<u8> indirectBuffer, u32 count, u32 stride) {
            throw exception("DrawIndexedIndirect is not implemented for this engine");
        }

        /**
         * @brief Handles a call to a method in the MME space
         * @param macroMethodOffset The target offset from EngineMethodsEnd
         * @return If flushes should be skipped for subsequent GPFIFO argument fetches
         */
        bool HandleMacroCall(u32 macroMethodOffset, GpfifoArgument argument, bool lastCall, const std::function<void(void)> &flushCallback);
    };
}
