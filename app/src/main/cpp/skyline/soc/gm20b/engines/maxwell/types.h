// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Copyright © 2018-2020 fincs (https://github.com/devkitPro/deko3d)

#pragma once

#include <common.h>
#include <soc/gm20b/engines/engine.h>

namespace skyline::soc::gm20b::engine::maxwell3d::type {
    #pragma pack(push, 1)

    enum class MmeShadowRamControl : u32 {
        MethodTrack = 0, //!< Tracks all writes to registers in shadow RAM
        MethodTrackWithFilter = 1, //!< Tracks all writes to registers in shadow RAM with a filter
        MethodPassthrough = 2, //!< Does nothing, no write tracking or hooking
        MethodReplay = 3, //!< Replays older tracked writes for any new writes to registers, discarding the contents of the new write
    };

    struct SyncpointAction {
        u16 id : 12;
        u8 _pad0_ : 4;
        bool flushCache : 1;
        u8 _pad1_ : 3;
        bool increment : 1;
        u16 _pad2_ : 11;
    };
    static_assert(sizeof(SyncpointAction) == sizeof(u32));

    /**
     * @brief The input primitive for a tessellated surface
     */
    enum class TessellationPrimitive : u8 {
        Isoline = 0,
        Triangle = 1,
        Quad = 2,
    };

    /**
     * @brief The spacing between tessellated vertices during primitive generation
     */
    enum class TessellationSpacing : u8 {
        Equal = 0,
        FractionalOdd = 1,
        FractionalEven = 2,
    };

    /**
     * @brief The winding order and connectivity of tessellated primitives during primitive generation
     */
    enum class TessellationWinding : u8 {
        CounterClockwiseAndNotConnected = 0, //!< Counter-clockwise, not connected
        ConnectedIsoline = 1, //!< Counter-clockwise, connected (Only for Isolines)
        ClockwiseTriangle = 1, //<! Clockwise, not connected (Only for Triangles)
        ConnectedTriangle = 2, //!< Counter-clockwise, connected (Only for Triangles)
        ClockwiseConnectedTriangle = 3, //!< Clockwise, connected (Only for Triangles)
    };

    constexpr static size_t RenderTargetCount{8}; //!< Maximum amount of render targets that can be bound at once on Maxwell 3D

    struct RenderTargetTileMode {
        u8 blockWidthLog2 : 4; //!< The width of a block in GOBs with log2 encoding, this is always assumed to be 1 as it is the only configuration the X1 supports
        u8 blockHeightLog2 : 4; //!< The height of a block in GOBs with log2 encoding
        u8 blockDepthLog2 : 4; //!< The depth of a block in GOBs with log2 encoding
        bool isLinear : 1;
        u8 _pad0_ : 3;
        bool is3d : 1;
        u16 _pad1_ : 15;
    };

    struct RenderTargetArrayMode {
        u16 depthOrlayerCount; //!< The 3D depth or layer count of the render target depending on if it's 3D or not
        bool volume : 1;
        u16 _pad_ : 15;
    };

    /**
     * @brief The target image's metadata for any rendering operations
     * @note Any render target with ColorFormat::None as their format are effectively disabled
     */
    struct ColorRenderTarget {
        Address address;
        u32 width;
        u32 height;

        enum class Format : u32 {
            None = 0x0,
            R32G32B32A32Float = 0xC0,
            R32G32B32A32Sint = 0xC1,
            R32G32B32A32Uint = 0xC2,
            R32G32B32X32Float = 0xC3,
            R32G32B32X32Sint = 0xC4,
            R32G32B32X32Uint = 0xC5,
            R16G16B16X16Unorm = 0xC6,
            R16G16B16X16Snorm = 0xC7,
            R16G16B16X16Sint = 0xC8,
            R16G16B16X16Uint = 0xC9,
            R16G16B16A16Float = 0xCA,
            R32G32Float = 0xCB,
            R32G32Sint = 0xCC,
            R32G32Uint = 0xCD,
            R16G16B16X16Float = 0xCE,
            B8G8R8A8Unorm = 0xCF,
            B8G8R8A8Srgb = 0xD0,
            A2B10G10R10Unorm = 0xD1,
            A2B10G10R10Uint = 0xD2,
            R8G8B8A8Unorm = 0xD5,
            R8G8B8A8Srgb = 0xD6,
            R8G8B8X8Snorm = 0xD7,
            R8G8B8X8Sint = 0xD8,
            R8G8B8X8Uint = 0xD9,
            R16G16Unorm = 0xDA,
            R16G16Snorm = 0xDB,
            R16G16Sint = 0xDC,
            R16G16Uint = 0xDD,
            R16G16Float = 0xDE,
            B10G11R11Float = 0xE0,
            R32Sint = 0xE3,
            R32Uint = 0xE4,
            R32Float = 0xE5,
            B8G8R8X8Unorm = 0xE6,
            B8G8R8X8Srgb = 0xE7,
            B5G6R5Unorm = 0xE8,
            B5G5R5A1Unorm = 0xE9,
            R8G8Unorm = 0xEA,
            R8G8Snorm = 0xEB,
            R8G8Sint = 0xEC,
            R8G8Uint = 0xED,
            R16Unorm = 0xEE,
            R16Snorm = 0xEF,
            R16Sint = 0xF0,
            R16Uint = 0xF1,
            R16Float = 0xF2,
            R8Unorm = 0xF3,
            R8Snorm = 0xF4,
            R8Sint = 0xF5,
            R8Uint = 0xF6,
            B5G5R5X1Unorm = 0xF8,
            R8G8B8X8Unorm = 0xF9,
            R8G8B8X8Srgb = 0xFA
        } format;

        RenderTargetTileMode tileMode;

        RenderTargetArrayMode arrayMode;

        u32 layerStrideLsr2; //!< The length of the stride of a layer shifted right by 2 bits
        u32 baseLayer;
        u32 _pad_[0x7];
    };
    static_assert(sizeof(ColorRenderTarget) == (0x10 * sizeof(u32)));

    enum class DepthRtFormat : u32 {
        D32Float = 0x0A,
        D16Unorm = 0x13,
        S8D24Unorm = 0x14,
        D24X8Unorm = 0x15,
        D24S8Unorm = 0x16,
        S8Uint = 0x17,
        D24C8Unorm = 0x18,
        D32S8X24Float = 0x19,
        D24X8S8C8X16Unorm = 0x1D,
        D32X8C8X16Float = 0x1E,
        D32S8C8X16Float = 0x1F,
    };

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

        ENUM_STRING(Swizzle, {
            ENUM_CASE_PAIR(PositiveX, "+X");
            ENUM_CASE_PAIR(NegativeX, "-X");
            ENUM_CASE_PAIR(PositiveY, "+Y");
            ENUM_CASE_PAIR(NegativeY, "-Y");
            ENUM_CASE_PAIR(PositiveZ, "+Z");
            ENUM_CASE_PAIR(NegativeZ, "-Z");
            ENUM_CASE_PAIR(PositiveW, "+W");
            ENUM_CASE_PAIR(NegativeW, "-W");
        })

        struct Swizzles {
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

    constexpr static size_t VertexBufferCount{16}; //!< The maximum amount of vertex buffers that can be bound at once
    constexpr static size_t VertexAttributeCount{32}; //!< The amount of vertex attributes that can be set

    union VertexAttribute {
        u32 raw;

        enum class ElementSize : u16 {
            e0 = 0x0,
            e1x8 = 0x1D,
            e2x8 = 0x18,
            e3x8 = 0x13,
            e4x8 = 0x0A,
            e1x16 = 0x1B,
            e2x16 = 0x0F,
            e3x16 = 0x05,
            e4x16 = 0x03,
            e1x32 = 0x12,
            e2x32 = 0x04,
            e3x32 = 0x02,
            e4x32 = 0x01,
            e10_10_10_2 = 0x30,
            e11_11_10 = 0x31,
        };

        ENUM_STRING(ElementSize, {
            ENUM_CASE_PAIR(e1x8, "1x8");
            ENUM_CASE_PAIR(e2x8, "2x8");
            ENUM_CASE_PAIR(e3x8, "3x8");
            ENUM_CASE_PAIR(e4x8, "4x8");
            ENUM_CASE_PAIR(e1x16, "1x16");
            ENUM_CASE_PAIR(e2x16, "2x16");
            ENUM_CASE_PAIR(e3x16, "3x16");
            ENUM_CASE_PAIR(e4x16, "4x16");
            ENUM_CASE_PAIR(e1x32, "1x32");
            ENUM_CASE_PAIR(e2x32, "2x32");
            ENUM_CASE_PAIR(e3x32, "3x32");
            ENUM_CASE_PAIR(e4x32, "4x32");
            ENUM_CASE_PAIR(e10_10_10_2, "10_10_10_2");
            ENUM_CASE_PAIR(e11_11_10, "11_11_10");
        })

        enum class ElementType : u16 {
            None = 0,
            Snorm = 1,
            Unorm = 2,
            Sint = 3,
            Uint = 4,
            Uscaled = 5,
            Sscaled = 6,
            Float = 7,
        };

        ENUM_STRING(ElementType, {
            ENUM_CASE(None);
            ENUM_CASE(Snorm);
            ENUM_CASE(Unorm);
            ENUM_CASE(Sint);
            ENUM_CASE(Uint);
            ENUM_CASE(Uscaled);
            ENUM_CASE(Sscaled);
            ENUM_CASE(Float);
        })

        struct {
            u8 bufferId : 5;
            u8 _pad0_ : 1;
            bool isConstant : 1;
            u16 offset : 14;
            ElementSize elementSize : 6;
            ElementType type : 3;
            u8 _pad1_ : 1;
            bool bgra : 1;
        };
    };
    static_assert(sizeof(VertexAttribute) == sizeof(u32));

    constexpr u16 operator|(VertexAttribute::ElementSize elementSize, VertexAttribute::ElementType type) {
        return static_cast<u16>(static_cast<u16>(elementSize) | (static_cast<u16>(type) << 6));
    }

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

    constexpr static size_t BlendColorChannelCount{4}; //!< The amount of color channels in operations such as blending

    enum class BlendOp : u32 {
        Add = 1,
        Subtract = 2,
        ReverseSubtract = 3,
        Minimum = 4,
        Maximum = 5,

        AddGL = 0x8006,
        MinimumGL = 0x8007,
        MaximumGL = 0x8008,
        SubtractGL = 0x800A,
        ReverseSubtractGL = 0x800B,
    };

    enum class BlendFactor : u32 {
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

    struct MultisampleControl {
        bool alphaToCoverage : 1;
        u8 _pad0_ : 3;
        bool alphaToOne : 1;
        u32 _pad1_ : 27;
    };
    static_assert(sizeof(MultisampleControl) == sizeof(u32));

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

    struct PointCoordReplace {
        u8 _unk_ : 2;
        enum class CoordOrigin : u8 {
            LowerLeft = 0,
            UpperLeft = 1,
        };
        CoordOrigin origin : 1;
        u16 enable : 10;
        u32 _pad_ : 19;
    };
    static_assert(sizeof(PointCoordReplace) == sizeof(u32));

    enum class PrimitiveTopology : u16 {
        PointList = 0,
        LineList = 1,
        LineLoop = 2,
        LineStrip = 3,
        TriangleList = 4,
        TriangleStrip = 5,
        TriangleFan = 6,
        QuadList = 7,
        QuadStrip = 8,
        Polygon = 9,
        LineListWithAdjacency = 10,
        LineStripWithAdjacency = 11,
        TriangleListWithAdjacency = 12,
        TriangleStripWithAdjacency = 13,
        PatchList = 14,
    };

    ENUM_STRING(PrimitiveTopology, {
        ENUM_CASE(PointList);
        ENUM_CASE(LineList);
        ENUM_CASE(LineLoop);
        ENUM_CASE(LineStrip);
        ENUM_CASE(TriangleList);
        ENUM_CASE(TriangleStrip);
        ENUM_CASE(TriangleFan);
        ENUM_CASE(QuadList);
        ENUM_CASE(QuadStrip);
        ENUM_CASE(Polygon);
        ENUM_CASE(LineListWithAdjacency);
        ENUM_CASE(LineStripWithAdjacency);
        ENUM_CASE(TriangleListWithAdjacency);
        ENUM_CASE(TriangleStripWithAdjacency);
        ENUM_CASE(PatchList);
    })

    union VertexBeginGl {
        u32 raw;
        struct {
            PrimitiveTopology topology;
            u16 pad : 10;
            bool instanceNext : 1;
            bool instanceContinue : 1;
        };
    };
    static_assert(sizeof(VertexBeginGl) == sizeof(u32));

    enum class FrontFace : u32 {
        Clockwise = 0x900,
        CounterClockwise = 0x901,
    };

    enum class CullFace : u32 {
        Front = 0x404,
        Back = 0x405,
        FrontAndBack = 0x408,
    };

    union ViewVolumeClipControl {
        u32 raw;
        struct {
            bool forceDepthRangeZeroToOne : 1;
            u8 _unk0_ : 2;
            bool depthClampNear : 1;
            bool depthClampFar : 1;
            u8 _unk1_ : 6;
            bool depthClampDisable : 1;
        };
    };
    static_assert(sizeof(ViewVolumeClipControl) == sizeof(u32));

    union ColorWriteMask {
        u32 raw;

        struct {
            u8 red : 4;
            u8 green : 4;
            u8 blue : 4;
            u8 alpha : 4;
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

    /**
     * @brief The logical operations that can be performed on the framebuffer after the fragment shader
     */
    enum class ColorLogicOp : u32 {
        Clear = 0x1500,
        And = 0x1501,
        AndReverse = 0x1502,
        Copy = 0x1503,
        AndInverted = 0x1504,
        Noop = 0x1505,
        Xor = 0x1506,
        Or = 0x1507,
        Nor = 0x1508,
        Equiv = 0x1509,
        Invert = 0x150A,
        OrReverse = 0x150B,
        CopyInverted = 0x150C,
        OrInverted = 0x150D,
        Nand = 0x150E,
        Set = 0x150F,
    };

    constexpr static size_t PipelineStageCount{5}; //!< Amount of pipeline stages on Maxwell 3D

    /**
     * @brief All the pipeline stages that Maxwell3D supports for draws
     */
    enum class PipelineStage {
        Vertex = 0,
        TessellationControl = 1,
        TessellationEvaluation = 2,
        Geometry = 3,
        Fragment = 4,
    };
    static_assert(static_cast<size_t>(PipelineStage::Fragment) + 1 == PipelineStageCount);

    struct Bind {
        u32 _pad0_[4];
        union {
            struct {
                u32 valid : 1;
                u32 _pad1_ : 3;
                u32 index : 5; //!< The index of the constant buffer in the pipeline stage to bind to
            };
            u32 raw;
        } constantBuffer;
        u32 _pad2_[3];
    };
    static_assert(sizeof(Bind) == (sizeof(u32) * 8));

    constexpr static size_t PipelineStageConstantBufferCount{18}; //!< Maximum amount of constant buffers that can be bound to a single pipeline stage

    constexpr static size_t ShaderStageCount{6}; //!< Amount of shader stages on Maxwell 3D

    /**
     * @brief All the shader programs stages that Maxwell3D supports for draws
     * @note As opposed to pipeline stages, there are two shader programs for the vertex stage
     */
    enum class ShaderStage {
        VertexA = 0,
        VertexB = 1,
        TessellationControl = 2,
        TessellationEvaluation = 3,
        Geometry = 4,
        Fragment = 5,
    };
    static_assert(static_cast<size_t>(ShaderStage::Fragment) + 1 == ShaderStageCount);

    /**
     * @brief The arguments to set a shader program for a pipeline stage
     */
    struct SetProgramInfo {
        struct {
            bool enable : 1;
            u8 _pad0_ : 3;
            ShaderStage stage : 4;
            u32 _pad1_ : 24;
        } info;
        u32 offset; //!< Offset from the base shader memory IOVA
        u32 _pad2_;
        u32 gprCount; //!< Amount of GPRs used by the shader program
        u32 _pad3_[12];
    };
    static_assert(sizeof(SetProgramInfo) == (sizeof(u32) * 0x10));

    struct IndexBuffer {
        Address start, limit; //!< The IOVA bounds of the index buffer
        enum class Format : u32 {
            Uint8 = 0,
            Uint16 = 1,
            Uint32 = 2,
        } format;
    };

    enum class DepthMode : u32 {
        MinusOneToOne = 0,
        ZeroToOne = 1
    };

    constexpr static size_t TransformFeedbackBufferCount{4}; //!< Number of supported transform feecback buffers in the 3D engine

    struct TransformFeedbackBuffer {
        u32 enable;
        Address iova;
        u32 size;
        u32 offset;
        u32 _pad_[3];
    };
    static_assert(sizeof(TransformFeedbackBuffer) == (sizeof(u32) * 8));

    struct TransformFeedbackBufferState {
        u32 bufferIndex;
        u32 varyingCount;
        u32 stride;
        u32 _pad_;
    };
    static_assert(sizeof(TransformFeedbackBufferState) == (sizeof(u32) * 4));

    constexpr static size_t TransformFeedbackVaryingCount{0x80};

    #pragma pack(pop)
}
