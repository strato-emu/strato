// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Copyright © 2018-2020 fincs (https://github.com/devkitPro/deko3d)

#pragma once

#include <common.h>

namespace skyline::soc::gm20b::engine::maxwell3d::type {
    #pragma pack(push, 1)

    /**
     * @brief A 40-bit GMMU virtual address with register-packing
     */
    struct Address {
        u32 high;
        u32 low;

        u64 Pack() {
            return (static_cast<u64>(high) << 32) | low;
        }
    };
    static_assert(sizeof(Address) == sizeof(u64));

    enum class MmeShadowRamControl : u32 {
        MethodTrack = 0, //!< Tracks all writes to registers in shadow RAM
        MethodTrackWithFilter = 1, //!< Tracks all writes to registers in shadow RAM with a filter
        MethodPassthrough = 2, //!< Does nothing, no write tracking or hooking
        MethodReplay = 3, //!< Replays older tracked writes for any new writes to registers, discarding the contents of the new write
    };

    constexpr static size_t RenderTargetCount{8}; //!< Maximum amount of render targets that can be bound at once on Maxwell 3D

    /**
     * @brief The target image's metadata for any rendering operations
     * @note Any render target with ColorFormat::None as their format are effectively disabled
     */
    struct RenderTarget {
        Address address;
        u32 width;
        u32 height;

        enum class ColorFormat : u32 {
            None = 0x0,
            R32B32G32A32Float = 0xC0,
            R16G16B16A16Unorm = 0xC6,
            R16G16B16A16Snorm = 0xC7,
            R16G16B16A16Sint = 0xC8,
            R16G16B16A16Uint = 0xC9,
            R16G16B16A16Float = 0xCA,
            A2B10G10R10Unorm = 0xD1,
            R8G8B8A8Unorm = 0xD5,
            A8B8G8R8Srgb = 0xD6,
            A8B8G8R8Snorm = 0xD7,
            R16G16Snorm = 0xDB,
            R16G16Float = 0xDE,
            B10G11R11Float = 0xE0,
            R32Float = 0xE5,
            R8G8Unorm = 0xEA,
            R8G8Snorm = 0xEB,
            R16Unorm = 0xEE,
            R16Float = 0xF2,
            R8Unorm = 0xF3,
            R8Snorm = 0xF4,
            R8Sint = 0xF5,
            R8Uint = 0xF6,
        } format;

        struct TileMode {
            u8 blockWidthLog2 : 4; //!< The width of a block in GOBs with log2 encoding, this is always assumed to be 1 as it is the only configuration the X1 supports
            u8 blockHeightLog2 : 4; //!< The height of a block in GOBs with log2 encoding
            u8 blockDepthLog2 : 4; //!< The depth of a block in GOBs with log2 encoding
            bool isLinear : 1;
            u8 _pad0_ : 3;
            bool is3d : 1;
            u16 _pad1_ : 15;
        } tileMode;

        struct ArrayMode {
            u16 layerCount;
            bool volume : 1;
            u16 _pad_ : 15;
        } arrayMode;

        u32 layerStrideLsr2; //!< The length of the stride of a layer shifted right by 2 bits
        u32 baseLayer;
        u32 _pad_[0x7];
    };
    static_assert(sizeof(RenderTarget) == (0x10 * sizeof(u32)));

    constexpr static size_t ViewportCount{16}; //!< Amount of viewports on Maxwell 3D, array size for any per-viewport parameter such as transform, scissors, etc

    /**
     * @brief The transformations applied on any primitive sent to a viewport
     */
    struct ViewportTransform {
        float scaleX; //!< Scales all X-axis primitive coordinates by this factor
        float scaleY;
        float scaleZ;
        float translateX; //!< Translates all X-axis primitive coordinates by this value
        float translateY;
        float translateZ;

        /**
         * @brief A component swizzle applied to primitive coordinates prior to clipping/perspective divide with optional negation
         * @note This functionality is exposed via GL_NV_viewport_swizzle (OpenGL) and VK_NV_viewport_swizzle (Vulkan)
         */
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

        /**
         * @brief The amount of subpixel bits on screen-space axes that bias if a pixel is inside a primitive for conservative rasterization
         * @note This functionality is exposed via GL_NV_conservative_raster (OpenGL) using SubpixelPrecisionBiasNV
         */
        struct {
            u8 x : 5;
            u8 _pad0_ : 3;
            u8 y : 5;
            u32 _pad1_ : 19;
        } subpixelPrecisionBias;
    };
    static_assert(sizeof(ViewportTransform) == (0x8 * sizeof(u32)));

    /**
     * @brief The offset and extent of the viewport for transformation of coordinates from NDC-space (Normalized Device Coordinates) to screen-space
     * @note This is effectively unused since all this data can be derived from the viewport transform, this misses crucial data that the transform has such as depth range order and viewport axis inverse transformations
     */
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

    /**
     * @brief The method used to rasterize polygons, not to be confused with the primitive type
     * @note This functionality is exposed via glPolygonMode (OpenGL)
     */
    enum class PolygonMode : u32 {
        Point = 0x1B00, //!< Draw a point for every vertex
        Line = 0x1B01, //!< Draw a line between all vertices
        Fill = 0x1B02, //!< Fill the area bounded by the vertices
    };

    /**
     * @brief A scissor which is used to reject all writes to non-masked regions
     * @note All coordinates are in screen-space as defined by the viewport
     */
    struct Scissor {
        u32 enable; //!< Rejects non-masked writes when enabled and allows all writes otherwise
        struct ScissorBounds {
            u16 minimum; //!< The lower bound of the masked region in a dimension
            u16 maximum; //!< The higher bound of the masked region in a dimension
        } horizontal, vertical;
        u32 _pad_;
    };
    static_assert(sizeof(Scissor) == (0x4 * sizeof(u32)));

    union VertexAttribute {
        u32 raw;

        enum class Size : u8 {
            Size_1x32 = 0x12,
            Size_2x32 = 0x04,
            Size_3x32 = 0x02,
            Size_4x32 = 0x01,
            Size_1x16 = 0x1B,
            Size_2x16 = 0x0F,
            Size_3x16 = 0x05,
            Size_4x16 = 0x03,
            Size_1x8 = 0x1D,
            Size_2x8 = 0x18,
            Size_3x8 = 0x13,
            Size_4x8 = 0x0A,
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

    /**
     * @brief A descriptor that controls how the RenderTarget array (at 0x200) will be interpreted
     */
    struct RenderTargetControl {
        u8 count : 4; //!< The amount of active render targets, doesn't necessarily mean bound
        u8 map0 : 3; //!< The index of the render target that maps to slot 0
        u8 map1 : 3;
        u8 map2 : 3;
        u8 map3 : 3;
        u8 map4 : 3;
        u8 map5 : 3;
        u8 map6 : 3;
        u8 map7 : 3;

        size_t operator[](size_t index) {
            switch (index) {
                case 0:
                    return map0;
                case 1:
                    return map1;
                case 2:
                    return map2;
                case 3:
                    return map3;
                case 4:
                    return map4;
                case 5:
                    return map5;
                case 6:
                    return map6;
                case 7:
                    return map7;
                default:
                    throw exception("Invalid RT index is being mapped: {}", index);
            }
        }
    };
    static_assert(sizeof(RenderTargetControl) == sizeof(u32));

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
            MinimumGL = 0x800A,
            MaximumGL = 0x800B,
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
            OneMinusDestColor = 0xA,
            SourceAlphaSaturate = 0xB,
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
            ConstantColorGL = 0xC001,
            OneMinusConstantColorGL = 0xC002,
            ConstantAlphaGL = 0xC003,
            OneMinusConstantAlphaGL = 0xC004,
            Source1ColorGL = 0xC900,
            OneMinusSource1ColorGL = 0xC901,
            Source1AlphaGL = 0xC902,
            OneMinusSource1AlphaGL = 0xC903,
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

    /**
     * @brief A method call which causes a layer of an RT to be cleared with a channel mask
     */
    struct ClearBuffers {
        bool depth : 1; //!< If the depth channel should be cleared
        bool stencil : 1;
        bool red : 1;
        bool green : 1;
        bool blue : 1;
        bool alpha : 1;
        u8 renderTargetId : 4; //!< The ID of the render target to clear
        u16 layerId : 11; //!< The index of the layer of the render target to clear
        u16 _pad_ : 10;
    };
    static_assert(sizeof(ClearBuffers) == sizeof(u32));

    struct SemaphoreInfo {
        enum class Op : u8 {
            Release = 0,
            Acquire = 1,
            Counter = 2,
            Trap = 3,
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
            ZcullStats0 = 0xA,
            TransformFeedbackPrimitivesWritten = 0xB,
            ZcullStats1 = 0xC,
            ZcullStats2 = 0xE,
            ClipperInputPrimitives = 0xF,
            ZcullStats3 = 0x10,
            ClipperOutputPrimitives = 0x11,
            PrimitivesGenerated = 0x12,
            FragmentShaderInvocations = 0x13,
            SamplesPassed = 0x15,
            TransformFeedbackOffset = 0x1A,
            TessControlShaderInvocations = 0x1B,
            TessEvaluationShaderInvocations = 0x1D,
            TessEvaluationShaderPrimitives = 0x1F,
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
        UpperLeft = 1,
    };

    #pragma pack(pop)
}
