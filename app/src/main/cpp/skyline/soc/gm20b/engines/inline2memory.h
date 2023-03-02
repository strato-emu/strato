// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>
#include <gpu/interconnect/inline2memory.h>
#include "engine.h"

namespace skyline::soc::gm20b {
    struct ChannelContext;
}

namespace skyline::soc::gm20b::engine {
    /**
     * @brief Implements the actual behaviour of the I2M engine, allowing it to be shared between other engines which also contain the I2M block (3D, compute)
     */
    class Inline2MemoryBackend {
      private:
        std::vector<u32> buffer; //!< Temporary buffer to hold data being currently uploaded
        u32 writeOffset{}; //!< Current write offset in words into `buffer`
        gpu::interconnect::Inline2Memory interconnect;
        ChannelContext &channelCtx;

      public:
        /**
         * @brief The I2M register state that can be included as part of an engines register state
         * @url https://github.com/devkitPro/deko3d/blob/master/source/maxwell/engine_inline.def
         */
        struct RegisterState {
            enum class DmaDstMemoryLayout : u8 {
                BlockLinear = 0,
                Pitch = 1
            };

            enum class DmaReductionFormat : u8 {
                Unsigned32 = 0,
                Signed32 = 1
            };

            enum class DmaCompletionType : u8 {
                FlushDisable = 0,
                FlushOnly = 1,
                ReleaseSemaphore = 2
            };

            enum class DmaInterruptType : u8 {
                None = 0,
                Interrupt = 1
            };

            enum class DmaSemaphoreStructSize : u8 {
                FourWords = 0,
                OneWord = 1
            };

            enum class DmaReductionOp : u8 {
                Add = 0,
                Min = 1,
                Max = 2,
                Inc = 3,
                Dec = 4,
                And = 5,
                Or = 6,
                Xor = 7
            };

            u32 lineLengthIn;
            u32 lineCount;
            Address offsetOut;
            u32 pitchOut;
            struct {
                u32 width : 4;
                u32 height : 4;
                u32 depth : 4;
                u32 _pad1_ : 20;

                size_t Height() const {
                    return 1 << height;
                }
                size_t Depth() const {
                    return 1 << depth;
                }
            } dstBlockSize;
            u32 dstWidth;
            u32 dstHeight;
            u32 dstDepth;
            u32 dstLayer;
            u32 originBytesX;
            u32 originSamplesY;
            struct {
                DmaDstMemoryLayout layout : 1;
                bool reductionEnable : 1;
                DmaReductionFormat format : 2;
                DmaCompletionType completion : 2;
                bool sysmemBarDisable : 1;
                u8 _pad0_ : 1;
                DmaInterruptType interrupt : 2;
                u8 _pad1_ : 2;
                DmaSemaphoreStructSize semaphore : 1;
                DmaReductionOp reductionOp : 3;
            } launchDma;
            u32 loadInlineData;
        };
        static_assert(sizeof(RegisterState) == (0xE * 0x4));

      private:
        /**
         * @brief Ran after all the inline data has been pushed and handles writing that data into memory
         */
        void CompleteDma(RegisterState &state);

      public:
        Inline2MemoryBackend(const DeviceState &state, ChannelContext &channelCtx);

        /**
         * @brief Should be called when launchDma in `state` is written to
         */
        void LaunchDma(RegisterState &state);

        /**
         * @brief Should be called when loadInlineData in `state` is written to (non batch version)
         */
        void LoadInlineData(RegisterState &state, u32 value);

        /**
         * @brief Should be called when loadInlineData in `state` is written to (batch version)
         */
        void LoadInlineData(RegisterState &state, span<u32> data);
    };

    /**
     * @brief Implements the actual I2M engine block that is located on subchannel 2 and handles uploading data from a pushbuffer into GPU memory
     */
    class Inline2Memory {
      private:
        Inline2MemoryBackend backend;

        void HandleMethod(u32 method, u32 argument);

        /**
         * @url https://github.com/devkitPro/deko3d/blob/master/source/maxwell/engine_inline.def
         */
        union Registers {
            std::array<u32, EngineMethodsEnd> raw;

            template<size_t Offset, typename Type>
            using Register = util::OffsetMember<Offset, Type, u32>;

            Register<0x60, Inline2MemoryBackend::RegisterState> i2m;
        } registers{};

      public:
        Inline2Memory(const DeviceState &state, ChannelContext &channelCtx);

        void CallMethod(u32 method, u32 argument);

        void CallMethodBatchNonInc(u32 method, span<u32> arguments);
    };
}
