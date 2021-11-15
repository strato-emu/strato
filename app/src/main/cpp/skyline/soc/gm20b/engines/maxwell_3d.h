// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Copyright © 2018-2020 fincs (https://github.com/devkitPro/deko3d)

#pragma once

#include <gpu/interconnect/graphics_context.h>
#include "engine.h"
#include "maxwell/macro_interpreter.h"

namespace skyline::soc::gm20b {
    struct ChannelContext;
}

namespace skyline::soc::gm20b::engine::maxwell3d {
    /**
     * @brief The Maxwell 3D engine handles processing 3D graphics
     */
    class Maxwell3D : public Engine {
      private:
        std::array<size_t, 0x80> macroPositions{}; //!< The positions of each individual macro in macro memory, there can be a maximum of 0x80 macros at any one time

        struct {
            i32 index{-1};
            std::vector<u32> arguments;
        } macroInvocation{}; //!< Data for a macro that is pending execution

        MacroInterpreter macroInterpreter;

        gpu::interconnect::GraphicsContext context;

        /**
         * @brief Calls the appropriate function corresponding to a certain method with the supplied argument
         */
        void HandleMethod(u32 method, u32 argument);

        /**
         * @brief Writes back a semaphore result to the guest with an auto-generated timestamp (if required)
         * @note If the semaphore is OneWord then the result will be downcasted to a 32-bit unsigned integer
         */
        void WriteSemaphoreResult(u64 result);

      public:
        static constexpr u32 RegisterCount{0xE00}; //!< The number of Maxwell 3D registers

        /**
         * @url https://github.com/devkitPro/deko3d/blob/master/source/maxwell/engine_3d.def
         */
        #pragma pack(push, 1)
        union Registers {
            std::array<u32, RegisterCount> raw;

            template<size_t Offset, typename Type>
            using Register = util::OffsetMember<Offset, Type, u32>;

            Register<0x40, u32> noOperation;
            Register<0x44, u32> waitForIdle;

            struct MME {
                u32 instructionRamPointer; // 0x45
                u32 instructionRamLoad; // 0x46
                u32 startAddressRamPointer; // 0x47
                u32 startAddressRamLoad; // 0x48
                type::MmeShadowRamControl shadowRamControl; // 0x49
            };
            Register<0x45, MME> mme;

            Register<0xB2, type::SyncpointAction> syncpointAction;

            Register<0xDF, u32> rasterizerEnable;
            Register<0x200, std::array<type::RenderTarget, type::RenderTargetCount>> renderTargets;
            Register<0x280, std::array<type::ViewportTransform, type::ViewportCount>> viewportTransforms;
            Register<0x300, std::array<type::Viewport, type::ViewportCount>> viewports;

            Register<0x360, std::array<u32, 4>> clearColorValue;
            Register<0x364, u32> clearDepthValue;

            struct PolygonMode {
                type::PolygonMode front; // 0x36B
                type::PolygonMode back; // 0x36C
            };
            Register<0x36B, PolygonMode> polygonMode;

            Register<0x380, std::array<type::Scissor, type::ViewportCount>> scissors;

            struct DepthBiasEnable {
                u32 point; // 0x370
                u32 line; // 0x371
                u32 fill; // 0x372
            };
            Register<0x370, DepthBiasEnable> depthBiasEnable;

            struct StencilBackExtra {
                u32 compareRef; // 0x3D5
                u32 writeMask; // 0x3D6
                u32 compareMask; // 0x3D7
            };
            Register<0x3D5, StencilBackExtra> stencilBackExtra;

            Register<0x3D8, u32> tiledCacheEnable;
            struct TiledCacheSize {
                u16 width;
                u16 height;
            };
            Register<0x3D9, TiledCacheSize> tiledCacheSize;

            Register<0x3E4, u32> commonColorWriteMask; //!< If enabled, the color write masks for all RTs must be set to that of the first RT
            Register<0x3EB, u32> rtSeparateFragData;
            Register<0x458, std::array<type::VertexAttribute, 0x20>> vertexAttributeState;
            Register<0x487, type::RenderTargetControl> renderTargetControl;
            Register<0x4B9, u32> independentBlendEnable;
            Register<0x4BB, u32> alphaTestEnable;
            Register<0x4C3, type::CompareOp> depthTestFunc;
            Register<0x4C4, float> alphaTestRef;
            Register<0x4C5, type::CompareOp> alphaTestFunc;
            Register<0x4C6, u32> drawTFBStride;

            struct BlendConstant {
                float red; // 0x4C7
                float green; // 0x4C8
                float blue; // 0x4C9
                float alpha; // 0x4CA
            };
            Register<0x4C7, std::array<float, type::BlendColorChannelCount>> blendConstant;

            struct BlendStateCommon {
                u32 seperateAlpha; // 0x4CF
                type::BlendOp colorOp; // 0x4D0
                type::BlendFactor colorSrcFactor; // 0x4D1
                type::BlendFactor colorDstFactor; // 0x4D2
                type::BlendOp alphaOp; // 0x4D3
                type::BlendFactor alphaSrcFactor; // 0x4D4
                u32 pad; // 0x4D5
                type::BlendFactor alphaDstFactor; // 0x4D6

                u32 enable; // 0x4D7
            };
            Register<0x4CF, BlendStateCommon> blendStateCommon;

            Register<0x4D8, std::array<u32, type::RenderTargetCount>> rtBlendEnable;

            Register<0x4E0, u32> stencilEnable;
            struct StencilFront {
                type::StencilOp failOp; // 0x4E1
                type::StencilOp zFailOp; // 0x4E2
                type::StencilOp zPassOp; // 0x4E3

                struct {
                    type::CompareOp op; // 0x4E4
                    i32 ref; // 0x4E5
                    u32 mask; // 0x4E6
                } compare;

                u32 writeMask; // 0x4E7
            };
            Register<0x4E1, StencilFront> stencilFront;

            Register<0x4EC, float> lineWidthSmooth;
            Register<0x4ED, float> lineWidthAliased;

            Register<0x50D, u32> drawBaseVertex;
            Register<0x50E, u32> drawBaseInstance;

            Register<0x544, u32> clipDistanceEnable;
            Register<0x545, u32> sampleCounterEnable;
            Register<0x546, float> pointSpriteSize;
            Register<0x547, u32> zCullStatCountersEnable;
            Register<0x548, u32> pointSpriteEnable;
            Register<0x54A, u32> shaderExceptions;
            Register<0x54D, u32> multisampleEnable;
            Register<0x54E, u32> depthTargetEnable;

            Register<0x54F, type::MultisampleControl> multisampleControl;

            struct SamplerPool {
                type::Address address; // 0x557
                u32 maximumIndex; // 0x559
            };
            Register<0x557, SamplerPool> samplerPool;

            Register<0x55B, float> depthBiasFactor;
            Register<0x55C, u32> lineSmoothEnable;

            struct TexturePool {
                type::Address address; // 0x55D
                u32 maximumIndex; // 0x55F
            };
            Register<0x55D, TexturePool> texturePool;

            Register<0x565, u32> stencilTwoSideEnable;

            struct StencilBack {
                type::StencilOp failOp; // 0x566
                type::StencilOp zFailOp; // 0x567
                type::StencilOp zPassOp; // 0x568
                type::CompareOp compareOp; // 0x569
            };
            Register<0x566, StencilBack> stencilBack;

            Register<0x56F, float> depthBiasUnits;

            Register<0x581, type::PointCoordReplace> pointCoordReplace;
            Register<0x582, type::Address> setProgramRegion;

            Register<0x5A1, u32> provokingVertexIsLast;

            Register<0x646, u32> cullFaceEnable;
            Register<0x647, type::FrontFace> frontFace;
            Register<0x648, type::CullFace> cullFace;

            Register<0x649, u32> pixelCentreImage;
            Register<0x64B, u32> viewportTransformEnable;
            Register<0x674, type::ClearBuffers> clearBuffers;
            Register<0x680, std::array<type::ColorWriteMask, type::RenderTargetCount>> colorWriteMask;

            Register<0x61F, float> depthBiasClamp;
            Register<0x64F, type::ViewVolumeClipControl> viewVolumeClipControl;

            struct Semaphore {
                type::Address address; // 0x6C0
                u32 payload; // 0x6C2
                type::SemaphoreInfo info; // 0x6C3
            };
            Register<0x6C0, Semaphore> semaphore;

            struct ColorLogicOp {
                u32 enable;
                type::ColorLogicOp type;
            };
            Register<0x671, ColorLogicOp> colorLogicOp;

            struct IndependentBlend {
                u32 seperateAlpha;
                type::BlendOp colorOp;
                type::BlendFactor colorSrcFactor;
                type::BlendFactor colorDstFactor;
                type::BlendOp alphaOp;
                type::BlendFactor alphaSrcFactor;
                type::BlendFactor alphaDstFactor;
                u32 _pad_;
            };
            Register<0x780, std::array<IndependentBlend, type::RenderTargetCount>> independentBlend;

            Register<0x800, std::array<type::SetProgramInfo, type::StageCount>> setProgram;

            Register<0x8C0, u32[0x20]> firmwareCall;
        };
        static_assert(sizeof(Registers) == (RegisterCount * sizeof(u32)));
        #pragma pack(pop)

        Registers registers{};
        Registers shadowRegisters{}; //!< A shadow-copy of the registers, their function is controlled by the 'shadowRamControl' register

        ChannelContext &channelCtx;

        std::array<u32, 0x2000> macroCode{}; //!< Stores GPU macros, writes to it will wraparound on overflow

        Maxwell3D(const DeviceState &state, ChannelContext &channelCtx, gpu::interconnect::CommandExecutor &executor);

        /**
         * @brief Initializes Maxwell 3D registers to their default values
         */
        void InitializeRegisters();

        void CallMethod(u32 method, u32 argument, bool lastCall = false);
    };
}
