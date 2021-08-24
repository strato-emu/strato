// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Copyright © 2018-2020 fincs (https://github.com/devkitPro/deko3d)

#pragma once

#include <gpu/context/graphics_context.h>
#include "engine.h"
#include "maxwell/macro_interpreter.h"

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

        gpu::context::GraphicsContext context;

        /**
         * @brief Writes back a semaphore result to the guest with an auto-generated timestamp (if required)
         * @note If the semaphore is OneWord then the result will be downcasted to a 32-bit unsigned integer
         */
        void WriteSemaphoreResult(u64 result);

      public:
        static constexpr u32 RegisterCount{0xE00}; //!< The number of Maxwell 3D registers

        /**
         * @url https://github.com/devkitPro/deko3d/blob/master/source/maxwell/engine_3d.def
         * @note To ease the extension of this structure, padding may follow both _padN_ and _padN_M_ formats
         */
        #pragma pack(push, 1)
        union Registers {
            std::array<u32, RegisterCount> raw;

            struct {
                u32 _pad0_[0x40]; // 0x0
                u32 noOperation; // 0x40
                u32 _pad1_[0x3]; // 0x41
                u32 waitForIdle; // 0x44

                struct {
                    u32 instructionRamPointer; // 0x45
                    u32 instructionRamLoad; // 0x46
                    u32 startAddressRamPointer; // 0x47
                    u32 startAddressRamLoad; // 0x48
                    type::MmeShadowRamControl shadowRamControl; // 0x49
                } mme;

                u32 _pad2_[0x68]; // 0x4A

                struct {
                    u16 id : 12;
                    u8 _pad0_ : 4;
                    bool flushCache : 1;
                    u8 _pad1_ : 3;
                    bool increment : 1;
                    u16 _pad2_ : 11;
                } syncpointAction; // 0xB2

                u32 _pad3_[0x2C]; // 0xB3
                u32 rasterizerEnable; // 0xDF
                u32 _pad4_[0x120]; // 0xE0
                std::array<type::RenderTarget, type::RenderTargetCount> renderTargets; // 0x200
                std::array<type::ViewportTransform, type::ViewportCount> viewportTransforms; // 0x280
                std::array<type::Viewport, type::ViewportCount> viewports; // 0x300
                u32 _pad5_[0x20]; // 0x340

                std::array<u32, 4> clearColorValue; // 0x360
                u32 clearDepthValue; // 0x364

                u32 _pad5_1_[0x6]; // 0x365

                struct {
                    type::PolygonMode front; // 0x36B
                    type::PolygonMode back; // 0x36C
                } polygonMode;

                u32 _pad6_[0x13]; // 0x36D
                std::array<type::Scissor, type::ViewportCount> scissors; // 0x380
                u32 _pad6_1_[0x15]; // 0x3C0

                struct {
                    u32 compareRef; // 0x3D5
                    u32 writeMask; // 0x3D6
                    u32 compareMask; // 0x3D7
                } stencilBackExtra;

                u32 _pad7_[0x13]; // 0x3D8
                u32 rtSeparateFragData; // 0x3EB
                u32 _pad8_[0x6C]; // 0x3EC
                std::array<type::VertexAttribute, 0x20> vertexAttributeState; // 0x458
                u32 _pad9_[0xF]; // 0x478
                type::RenderTargetControl renderTargetControl; // 0x487
                u32 _pad9_1_[0x3B]; // 0x488
                type::CompareOp depthTestFunc; // 0x4C3
                float alphaTestRef; // 0x4C4
                type::CompareOp alphaTestFunc; // 0x4C5
                u32 drawTFBStride; // 0x4C6

                struct {
                    float r; // 0x4C7
                    float g; // 0x4C8
                    float b; // 0x4C9
                    float a; // 0x4CA
                } blendConstant;

                u32 _pad10_[0x4]; // 0x4CB

                struct {
                    u32 seperateAlpha; // 0x4CF
                    type::Blend::Op colorOp; // 0x4D0
                    type::Blend::Factor colorSrcFactor; // 0x4D1
                    type::Blend::Factor colorDestFactor; // 0x4D2
                    type::Blend::Op alphaOp; // 0x4D3
                    type::Blend::Factor alphaSrcFactor; // 0x4D4
                    u32 _pad_; // 0x4D5
                    type::Blend::Factor alphaDestFactor; // 0x4D6

                    u32 enableCommon; // 0x4D7
                    std::array<u32, 8> enable; // 0x4D8 For each render target
                } blend;

                u32 stencilEnable; // 0x4E0

                struct {
                    type::StencilOp failOp; // 0x4E1
                    type::StencilOp zFailOp; // 0x4E2
                    type::StencilOp zPassOp; // 0x4E3

                    struct {
                        type::CompareOp op; // 0x4E4
                        i32 ref; // 0x4E5
                        u32 mask; // 0x4E6
                    } compare;

                    u32 writeMask; // 0x4E7
                } stencilFront;

                u32 _pad11_[0x4]; // 0x4E8
                float lineWidthSmooth; // 0x4EC
                float lineWidthAliased; // 0x4D
                u32 _pad12_[0x1F]; // 0x4EE
                u32 drawBaseVertex; // 0x50D
                u32 drawBaseInstance; // 0x50E
                u32 _pad13_[0x35]; // 0x50F
                u32 clipDistanceEnable; // 0x544
                u32 sampleCounterEnable; // 0x545
                float pointSpriteSize; // 0x546
                u32 zCullStatCountersEnable; // 0x547
                u32 pointSpriteEnable; // 0x548
                u32 _pad14_; // 0x549
                u32 shaderExceptions; // 0x54A
                u32 _pad15_[0x2]; // 0x54B
                u32 multisampleEnable; // 0x54D
                u32 depthTargetEnable; // 0x54E

                struct {
                    bool alphaToCoverage : 1;
                    u8 _pad0_ : 3;
                    bool alphaToOne : 1;
                    u32 _pad1_ : 27;
                } multisampleControl; // 0x54F

                u32 _pad16_[0x7]; // 0x550

                struct {
                    type::Address address; // 0x557
                    u32 maximumIndex; // 0x559
                } texSamplerPool;

                u32 _pad17_; // 0x55A
                u32 polygonOffsetFactor; // 0x55B
                u32 lineSmoothEnable; // 0x55C

                struct {
                    type::Address address; // 0x55D
                    u32 maximumIndex; // 0x55F
                } texHeaderPool;

                u32 _pad18_[0x5]; // 0x560

                u32 stencilTwoSideEnable; // 0x565

                struct {
                    type::StencilOp failOp; // 0x566
                    type::StencilOp zFailOp; // 0x567
                    type::StencilOp zPassOp; // 0x568
                    type::CompareOp compareOp; // 0x569
                } stencilBack;

                u32 _pad19_[0x17]; // 0x56A

                struct {
                    u8 _unk_ : 2;
                    type::CoordOrigin origin : 1;
                    u16 enable : 10;
                    u32 _pad_ : 19;
                } pointCoordReplace; // 0x581

                u32 _pad20_[0xC4]; // 0x582
                u32 cullFaceEnable; // 0x646
                type::FrontFace frontFace; // 0x647
                type::CullFace cullFace; // 0x648
                u32 pixelCentreImage; // 0x649
                u32 _pad21_; // 0x64A
                u32 viewportTransformEnable; // 0x64B
                u32 _pad22_[0x28]; // 0x64C
                type::ClearBuffers clearBuffers; // 0x674
                u32 _pad22_1_[0xB]; // 0x675
                std::array<type::ColorWriteMask, type::RenderTargetCount> colorMask; // 0x680
                u32 _pad23_[0x38]; // 0x688

                struct {
                    type::Address address; // 0x6C0
                    u32 payload; // 0x6C2
                    type::SemaphoreInfo info; // 0x6C3
                } semaphore;

                u32 _pad24_[0xBC]; // 0x6C4
                std::array<type::Blend, type::RenderTargetCount> independentBlend; // 0x780
                u32 _pad25_[0x100]; // 0x7C0
                u32 firmwareCall[0x20]; // 0x8C0
            };
        };
        static_assert(sizeof(Registers) == (RegisterCount * sizeof(u32)));
        static_assert(U32_OFFSET(Registers, firmwareCall) == 0x8C0);
        #pragma pack(pop)

        Registers registers{};
        Registers shadowRegisters{}; //!< A shadow-copy of the registers, their function is controlled by the 'shadowRamControl' register

        std::array<u32, 0x2000> macroCode{}; //!< Stores GPU macros, writes to it will wraparound on overflow

        Maxwell3D(const DeviceState &state, GMMU &gmmu);

        /**
         * @brief Resets the Maxwell 3D registers to their default values
         */
        void ResetRegs();

        void CallMethod(u32 method, u32 argument, bool lastCall);
    };
}
