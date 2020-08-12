// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <array>
#include <common.h>
#include <gpu/texture.h>
#include <gpu/macro_interpreter.h>
#include "engine.h"

#define MAXWELL3D_OFFSET(field) U32_OFFSET(skyline::gpu::engine::Maxwell3D::Registers, field)

namespace skyline {
    namespace constant {
        constexpr u32 Maxwell3DRegisterCounter = 0xe00; //!< The number of Maxwell 3D registers
    }

    namespace gpu::engine {
        /**
        * @brief The Maxwell 3D engine handles processing 3D graphics
        */
        class Maxwell3D : public Engine {
          private:
            std::array<size_t, 0x80> macroPositions{}; //!< This holds the positions of each individual macro in macro memory, there can be a maximum of 0x80 macros at any one time

            struct {
                u32 index;
                std::vector<u32> arguments;
            } macroInvocation{}; //!< This hold the index and arguments of the macro that is pending execution

            MacroInterpreter macroInterpreter;

            void HandleSemaphoreCounterOperation();

            void WriteSemaphoreResult(u64 result);

          public:
            /**
            * @brief This holds the Maxwell3D engine's register space
            * @url https://github.com/devkitPro/deko3d/blob/master/source/maxwell/engine_3d.def#L478
            */
#pragma pack(push, 1)
            union Registers {
                std::array<u32, constant::Maxwell3DRegisterCounter> raw;

                struct Address {
                    u32 high;
                    u32 low;

                    u64 Pack() {
                        return (static_cast<u64>(high) << 32) | low;
                    }
                };
                static_assert(sizeof(Address) == sizeof(u64));

                enum class MmeShadowRamControl : u32 {
                    MethodTrack = 0,
                    MethodTrackWithFilter = 1,
                    MethodPassthrough = 2,
                    MethodReplay = 3
                };

                struct ViewportTransform {
                    enum class Swizzle : u8 {
                        PositiveX = 0,
                        NegativeX = 1,
                        PositiveY = 2,
                        NegativeY = 3,
                        PositiveZ = 4,
                        NegativeZ = 5,
                        PositiveW = 6,
                        NegativeW = 7,
                    };

                    float scaleX;
                    float scaleY;
                    float scaleZ;
                    float translateX;
                    float translateY;
                    float translateZ;

                    struct {
                        Swizzle x : 3;
                        u8 _pad0_ : 1;
                        Swizzle y : 3;
                        u8 _pad1_ : 1;
                        Swizzle z : 3;
                        u8 _pad2_ : 1;
                        Swizzle w : 3;
                        u32 _pad3_ : 17;
                    } swizzles;

                    struct {
                        u8 x : 5;
                        u8 _pad0_ : 3;
                        u8 y : 5;
                        u32 _pad1_ : 19;
                    } subpixelPrecisionBias;
                };
                static_assert(sizeof(ViewportTransform) == (0x8 * sizeof(u32)));

                struct Viewport {
                    struct {
                        u16 x;
                        u16 width;
                    };

                    struct {
                        u16 y;
                        u16 height;
                    };

                    float depthRangeNear;
                    float depthRangeFar;
                };
                static_assert(sizeof(Viewport) == (0x4 * sizeof(u32)));

                enum class PolygonMode : u32 {
                    Point = 0x1b00,
                    Line = 0x1b01,
                    Fill = 0x1b02,
                };

                union VertexAttribute {
                    u32 raw;

                    enum class Size : u8 {
                        Size_1x32 = 0x12,
                        Size_2x32 = 0x04,
                        Size_3x32 = 0x02,
                        Size_4x32 = 0x01,
                        Size_1x16 = 0x1b,
                        Size_2x16 = 0x0f,
                        Size_3x16 = 0x05,
                        Size_4x16 = 0x03,
                        Size_1x8 = 0x1d,
                        Size_2x8 = 0x18,
                        Size_3x8 = 0x13,
                        Size_4x8 = 0x0a,
                        Size_10_10_10_2 = 0x30,
                        Size_11_11_10 = 0x31,
                    };

                    enum class Type : u8 {
                        None = 0,
                        SNorm = 1,
                        UNorm = 2,
                        SInt = 3,
                        UInt = 4,
                        UScaled = 5,
                        SScaled = 6,
                        Float = 7,
                    };

                    struct {
                        u8 bufferId : 5;
                        u8 _pad0_ : 1;
                        bool fixed : 1;
                        u16 offset : 14;
                        Size size : 6;
                        Type type : 3;
                        u8 _pad1_ : 1;
                        bool bgra : 1;
                    };
                };
                static_assert(sizeof(VertexAttribute) == sizeof(u32));

                enum class CompareOp : u32 {
                    Never = 1,
                    Less = 2,
                    Equal = 3,
                    LessOrEqual = 4,
                    Greater = 5,
                    NotEqual = 6,
                    GreaterOrEqual = 7,
                    Always = 8,

                    NeverGL = 0x200,
                    LessGL = 0x201,
                    EqualGL = 0x202,
                    LessOrEqualGL = 0x203,
                    GreaterGL = 0x204,
                    NotEqualGL = 0x205,
                    GreaterOrEqualGL = 0x206,
                    AlwaysGL = 0x207,
                };

                struct Blend {
                    enum class Op : u32 {
                        Add = 1,
                        Subtract = 2,
                        ReverseSubtract = 3,
                        Minimum = 4,
                        Maximum = 5,

                        AddGL = 0x8006,
                        SubtractGL = 0x8007,
                        ReverseSubtractGL = 0x8008,
                        MinimumGL = 0x800a,
                        MaximumGL = 0x800b
                    };

                    enum class Factor : u32 {
                        Zero = 0x1,
                        One = 0x2,
                        SourceColor = 0x3,
                        OneMinusSourceColor = 0x4,
                        SourceAlpha = 0x5,
                        OneMinusSourceAlpha = 0x6,
                        DestAlpha = 0x7,
                        OneMinusDestAlpha = 0x8,
                        DestColor = 0x9,
                        OneMinusDestColor = 0xa,
                        SourceAlphaSaturate = 0xb,
                        Source1Color = 0x10,
                        OneMinusSource1Color = 0x11,
                        Source1Alpha = 0x12,
                        OneMinusSource1Alpha = 0x13,
                        ConstantColor = 0x61,
                        OneMinusConstantColor = 0x62,
                        ConstantAlpha = 0x63,
                        OneMinusConstantAlpha = 0x64,

                        ZeroGL = 0x4000,
                        OneGL = 0x4001,
                        SourceColorGL = 0x4300,
                        OneMinusSourceColorGL = 0x4301,
                        SourceAlphaGL = 0x4302,
                        OneMinusSourceAlphaGL = 0x4303,
                        DestAlphaGL = 0x4304,
                        OneMinusDestAlphaGL = 0x4305,
                        DestColorGL = 0x4306,
                        OneMinusDestColorGL = 0x4307,
                        SourceAlphaSaturateGL = 0x4308,
                        ConstantColorGL = 0xc001,
                        OneMinusConstantColorGL = 0xc002,
                        ConstantAlphaGL = 0xc003,
                        OneMinusConstantAlphaGL = 0xc004,
                        Source1ColorGL = 0xc900,
                        OneMinusSource1ColorGL = 0xc901,
                        Source1AlphaGL = 0xc902,
                        OneMinusSource1AlphaGL = 0xc903,
                    };

                    struct {
                        u32 seperateAlpha;
                        Op colorOp;
                        Factor colorSrcFactor;
                        Factor colorDestFactor;
                        Op alphaOp;
                        Factor alphaSrcFactor;
                        Factor alphaDestFactor;
                        u32 _pad_;
                    };
                };
                static_assert(sizeof(Blend) == (sizeof(u32) * 8));

                enum class StencilOp : u32 {
                    Keep = 1,
                    Zero = 2,
                    Replace = 3,
                    IncrementAndClamp = 4,
                    DecrementAndClamp = 5,
                    Invert = 6,
                    IncrementAndWrap = 7,
                    DecrementAndWrap = 8,
                };

                enum class FrontFace : u32 {
                    Clockwise = 0x900,
                    CounterClockwise = 0x901,
                };

                enum class CullFace : u32 {
                    Front = 0x404,
                    Back = 0x405,
                    FrontAndBack = 0x408,
                };

                union ColorWriteMask {
                    u32 raw;

                    struct {
                        u8 r : 4;
                        u8 g : 4;
                        u8 b : 4;
                        u8 a : 4;
                    };
                };
                static_assert(sizeof(ColorWriteMask) == sizeof(u32));

                struct SemaphoreInfo {
                    enum class Op : u8 {
                        Release = 0,
                        Acquire = 1,
                        Counter = 2,
                        Trap = 3
                    };

                    enum class ReductionOp : u8 {
                        Add = 0,
                        Min = 1,
                        Max = 2,
                        Inc = 3,
                        Dec = 4,
                        And = 5,
                        Or = 6,
                        Xor = 7,
                    };

                    enum class Unit : u8 {
                        VFetch = 1,
                        VP = 2,
                        Rast = 4,
                        StrmOut = 5,
                        GP = 6,
                        ZCull = 7,
                        Prop = 10,
                        Crop = 15,
                    };

                    enum class SyncCondition : u8 {
                        NotEqual = 0,
                        GreaterThan = 1,
                    };

                    enum class Format : u8 {
                        U32 = 0,
                        I32 = 1,
                    };

                    enum class CounterType : u8 {
                        Zero = 0x0,
                        InputVertices = 0x1,
                        InputPrimitives = 0x3,
                        VertexShaderInvocations = 0x5,
                        GeometryShaderInvocations = 0x7,
                        GeometryShaderPrimitives = 0x9,
                        ZcullStats0 = 0xa,
                        TransformFeedbackPrimitivesWritten = 0xb,
                        ZcullStats1 = 0xc,
                        ZcullStats2 = 0xe,
                        ClipperInputPrimitives = 0xf,
                        ZcullStats3 = 0x10,
                        ClipperOutputPrimitives = 0x11,
                        PrimitivesGenerated = 0x12,
                        FragmentShaderInvocations = 0x13,
                        SamplesPassed = 0x15,
                        TransformFeedbackOffset = 0x1a,
                        TessControlShaderInvocations = 0x1b,
                        TessEvaluationShaderInvocations = 0x1d,
                        TessEvaluationShaderPrimitives = 0x1f
                    };

                    enum class StructureSize : u8 {
                        FourWords = 0,
                        OneWord = 1,
                    };

                    Op op : 2;
                    bool flushDisable : 1;
                    bool reductionEnable : 1;
                    bool fenceEnable : 1;
                    u8 _pad0_ : 4;
                    ReductionOp reductionOp : 3;
                    Unit unit : 4;
                    SyncCondition syncCondition : 1;
                    Format format : 2;
                    u8 _pad1_ : 1;
                    bool awakenEnable : 1;
                    u8 _pad2_ : 2;
                    CounterType counterType : 5;
                    StructureSize structureSize : 1;
                };
                static_assert(sizeof(SemaphoreInfo) == sizeof(u32));

                enum class CoordOrigin : u8 {
                    LowerLeft = 0,
                    UpperLeft = 1
                };

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
                        MmeShadowRamControl shadowRamControl; // 0x49
                    } mme;

                    u32 _pad2_[0x68]; // 0x4a

                    struct {
                        u16 id : 12;
                        u8 _pad0_ : 4;
                        bool flushCache : 1;
                        u8 _pad1_ : 3;
                        bool increment : 1;
                        u16 _pad2_ : 11;
                    } syncpointAction; // 0xb2

                    u32 _pad3_[0x2c]; // 0xb3
                    u32 rasterizerEnable; // 0xdf
                    u32 _pad4_[0x1a0]; // 0xe0
                    std::array<ViewportTransform, 0x10> viewportTransform; // 0x280
                    std::array<Viewport, 0x10> viewport; // 0x300
                    u32 _pad5_[0x2b]; // 0x340

                    struct {
                        PolygonMode front; // 0x36b
                        PolygonMode back; // 0x36c
                    } polygonMode;

                    u32 _pad6_[0x68]; // 0x36d

                    struct {
                        u32 compareRef; // 0x3d5
                        u32 writeMask; // 0x3d6
                        u32 compareMask; // 0x3d7
                    } stencilBackExtra;

                    u32 _pad7_[0x13]; // 0x3d8
                    u32 rtSeparateFragData; // 0x3eb
                    u32 _pad8_[0x6c]; // 0x3ec
                    std::array<VertexAttribute, 0x20> vertexAttributeState; // 0x458
                    u32 _pad9_[0x4b]; // 0x478
                    CompareOp depthTestFunc; // 0x4c3
                    float alphaTestRef; // 0x4c4
                    CompareOp alphaTestFunc; // 0x4c5
                    u32 drawTFBStride; // 0x4c6

                    struct {
                        float r; // 0x4c7
                        float g; // 0x4c8
                        float b; // 0x4c9
                        float a; // 0x4ca
                    } blendConstant;

                    u32 _pad10_[0x4]; // 0x4cb

                    struct {
                        u32 seperateAlpha; // 0x4cf
                        Blend::Op colorOp; // 0x4d0
                        Blend::Factor colorSrcFactor; // 0x4d1
                        Blend::Factor colorDestFactor; // 0x4d2
                        Blend::Op alphaOp; // 0x4d3
                        Blend::Factor alphaSrcFactor; // 0x4d4
                        u32 _pad_; // 0x4d5
                        Blend::Factor alphaDestFactor; // 0x4d6

                        u32 enableCommon; // 0x4d7
                        std::array<u32, 8> enable; // 0x4d8 For each render target
                    } blend;

                    u32 stencilEnable; // 0x4e0

                    struct {
                        StencilOp failOp; // 0x4e1
                        StencilOp zFailOp; // 0x4e2
                        StencilOp zPassOp; // 0x4e3

                        struct {
                            CompareOp op; // 0x4e4
                            i32 ref; // 0x4e5
                            u32 mask; // 0x4e6
                        } compare;

                        u32 writeMask; // 0x4e7
                    } stencilFront;

                    u32 _pad11_[0x4]; // 0x4e8
                    float lineWidthSmooth; // 0x4ec
                    float lineWidthAliased; // 0x4d
                    u32 _pad12_[0x1f]; // 0x4ee
                    u32 drawBaseVertex; // 0x50d
                    u32 drawBaseInstance; // 0x50e
                    u32 _pad13_[0x35]; // 0x50f
                    u32 clipDistanceEnable; // 0x544
                    u32 sampleCounterEnable; // 0x545
                    float pointSpriteSize; // 0x546
                    u32 zCullStatCountersEnable; // 0x547
                    u32 pointSpriteEnable; // 0x548
                    u32 _pad14_; // 0x549
                    u32 shaderExceptions; // 0x54a
                    u32 _pad15_[0x2]; // 0x54b
                    u32 multisampleEnable; // 0x54d
                    u32 depthTargetEnable; // 0x54e

                    struct {
                        bool alphaToCoverage : 1;
                        u8 _pad0_ : 3;
                        bool alphaToOne : 1;
                        u32 _pad1_ : 27;
                    } multisampleControl; // 0x54f

                    u32 _pad16_[0x7]; // 0x550

                    struct {
                        Address address; // 0x557
                        u32 maximumIndex; // 0x559
                    } texSamplerPool;

                    u32 _pad17_; // 0x55a
                    u32 polygonOffsetFactor; // 0x55b
                    u32 lineSmoothEnable; // 0x55c

                    struct {
                        Address address; // 0x55d
                        u32 maximumIndex; // 0x55f
                    } texHeaderPool;

                    u32 _pad18_[0x5]; // 0x560

                    u32 stencilTwoSideEnable; // 0x565

                    struct {
                        StencilOp failOp; // 0x566
                        StencilOp zFailOp; // 0x567
                        StencilOp zPassOp; // 0x568
                        CompareOp compareOp; // 0x569
                    } stencilBack;

                    u32 _pad19_[0x17]; // 0x56a

                    struct {
                        u8 _unk_ : 2;
                        CoordOrigin origin : 1;
                        u16 enable : 10;
                        u32 _pad_ : 19;
                    } pointCoordReplace; // 0x581

                    u32 _pad20_[0xc4]; // 0x582
                    u32 cullFaceEnable; // 0x646
                    FrontFace frontFace; // 0x647
                    CullFace cullFace; // 0x648
                    u32 pixelCentreImage; // 0x649
                    u32 _pad21_; // 0x64a
                    u32 viewportTransformEnable; // 0x64b
                    u32 _pad22_[0x34]; // 0x64a
                    std::array<ColorWriteMask, 8> colorMask; // 0x680 For each render target
                    u32 _pad23_[0x38]; // 0x688

                    struct {
                        Address address; // 0x6c0
                        u32 payload; // 0x6c2
                        SemaphoreInfo info; // 0x6c3
                    } semaphore;

                    u32 _pad24_[0xbc]; // 0x6c4
                    std::array<Blend, 8> independentBlend; // 0x780 For each render target
                    u32 _pad25_[0x100]; // 0x7c0
                    u32 firmwareCall[0x20]; // 0x8c0
                };
            };
            static_assert(sizeof(Registers) == (constant::Maxwell3DRegisterCounter * sizeof(u32)));
#pragma pack(pop)

            Registers registers{}; //!< The maxwell 3D register space
            Registers shadowRegisters{}; //!< The shadow registers, their function is controlled by the 'shadowRamControl' register

            std::array<u32, 0x10000> macroCode{}; //!< This is used to store GPU macros, the 256kb size is from Ryujinx

            Maxwell3D(const DeviceState &state);

            /**
             * @brief Resets the Maxwell 3D registers to their default values
             */
            void ResetRegs();

            void CallMethod(MethodParams params);
        };
    }
}