// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Copyright © 2018-2020 fincs (https://github.com/devkitPro/deko3d)

#pragma once

#include <common.h>
#include <common/macros.h>
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

    constexpr static size_t ColorTargetCount{8}; //!< Maximum amount of render targets that can be bound at once on Maxwell 3D

    struct TargetMemory {
        enum class Layout : u8 {
            BlockLinear = 0,
            Pitch = 1
        };

        enum class ThirdDimensionControl : u8 {
            ThirdDimensionDefinesArraySize = 0,
            ThirdDimensionDefinesDepthSize = 1
        };

        u8 blockWidthLog2 : 4; //!< The width of a block in GOBs with log2 encoding, this is always assumed to be 1 as it is the only configuration the X1 supports
        u8 blockHeightLog2 : 4; //!< The height of a block in GOBs with log2 encoding
        u8 blockDepthLog2 : 4; //!< The depth of a block in GOBs with log2 encoding
        Layout layout : 1;
        u8 _pad0_ : 3;
        ThirdDimensionControl thirdDimensionControl : 1;
        u16 _pad1_ : 15;

        u8 BlockWidth() const {
            return 1; // blockWidthLog2 only supports a value of 0
        }

        u8 BlockHeight() const {
            return static_cast<u8>(1 << blockHeightLog2);
        }

        u8 BlockDepth() const {
            return static_cast<u8>(1 << blockDepthLog2);
        }
    };

    /**
     * @brief The target image's metadata for any rendering operations
     * @note Any render target with ColorFormat::None as their format are effectively disabled
     */
    struct ColorTarget {
        Address offset;
        u32 width;
        u32 height;

        enum class Format : u32 {
            // F - SFloat
            // S - SInt
            // U - UInt
            // L - sRGB
            // N - SNorm
            // Z - 0
            // O - 1
            //   - UNorm
            // X - Ignored
            // 8 bit formats are in BE - opposite to VK!
            Disabled = 0x0,
            RF32_GF32_BF32_AF32 = 0xC0,
            RS32_GS32_BS32_AS32 = 0xC1,
            RU32_GU32_BU32_AU32 = 0xC2,
            RF32_GF32_BF32_X32 = 0xC3,
            RS32_GS32_BS32_X32 = 0xC4,
            RU32_GU32_BU32_X32 = 0xC5,
            R16_G16_B16_A16 = 0xC6,
            RN16_GN16_BN16_AN16 = 0xC7,
            RS16_GS16_BS16_AS16 = 0xC8,
            RU16_GU16_BU16_AU16 = 0xC9,
            RF16_GF16_BF16_AF16 = 0xCA,
            RF32_GF32 = 0xCB,
            RS32_GS32 = 0xCC,
            RU32_GU32 = 0xCD,
            RF16_GF16_BF16_X16 = 0xCE,
            A8R8G8B8 = 0xCF,
            A8RL8GL8BL8 = 0xD0,
            A2B10G10R10 = 0xD1,
            AU2BU10GU10RU10 = 0xD2,
            A8B8G8R8 = 0xD5,
            A8BL8GL8RL8 = 0xD6,
            AN8BN8GN8RN8 = 0xD7,
            AS8BS8GS8RS8 = 0xD8,
            AU8BU8GU8RU8 = 0xD9,
            R16_G16 = 0xDA,
            RN16_GN16 = 0xDB,
            RS16_GS16 = 0xDC,
            RU16_GU16 = 0xDD,
            RF16_GF16 = 0xDE,
            A2R10G10B10 = 0xDF,
            BF10GF11RF11 = 0xE0,
            RS32 = 0xE3,
            RU32 = 0xE4,
            RF32 = 0xE5,
            X8R8G8B8 = 0xE6,
            X8RL8GL8BL8 = 0xE7,
            R5G6B5 = 0xE8,
            A1R5G5B5 = 0xE9,
            G8R8 = 0xEA,
            GN8RN8 = 0xEB,
            GS8RS8 = 0xEC,
            GU8RU8 = 0xED,
            R16 = 0xEE,
            RN16 = 0xEF,
            RS16 = 0xF0,
            RU16 = 0xF1,
            RF16 = 0xF2,
            R8 = 0xF3,
            RN8 = 0xF4,
            RS8 = 0xF5,
            RU8 = 0xF6,
            A8 = 0xF7,
            X1R5G5B5 = 0xF8,
            X8B8G8R8 = 0xF9,
            X8BL8GL8RL8 = 0xFA,
            Z1R5G5B5 = 0xFB,
            O1R5G5B5 = 0xFC,
            Z8R8G8B8 = 0xFD,
            O8R8G8B8 = 0xFE,
            R32 = 0xFF,
            A16 = 0x40,
            AF16 = 0x41,
            AF32 = 0x42,
            A8R8 = 0x43,
            R16_A16 = 0x44,
            RF16_AF16 = 0x45,
            RF32_AF32 = 0x46,
            B8G8R8A8 = 0x47,
        } format;

        TargetMemory memory;

        u32 thirdDimension : 28;
        u8 _pad0_ : 4;

        u32 arrayPitchLsr2; //!< The length of the stride of a layer shifted right by 2 bits

        u32 layerOffset;
        u32 _pad1_[0x7];

        u32 ArrayPitch() const {
            return arrayPitchLsr2 << 2;
        }
    };
    static_assert(sizeof(ColorTarget) == (0x10 * sizeof(u32)));

    enum class ZtFormat : u32 {
        Z16 = 0x13,
        Z24S8 = 0x14,
        X8Z24 = 0x15,
        S8Z24 = 0x16,
        S8 = 0x17,
        V8Z24 = 0x18,
        ZF32 = 0x0A,
        ZF32_X24S8 = 0x19,
        X8Z24_X16V8S8 = 0x1D,
        ZF32_X16V8X8 = 0x1E,
        ZF32_X16V8S8 = 0x1F,
    };

    struct ZtBlockSize {
        u8 blockWidthLog2 : 4; //!< The width of a block in GOBs with log2 encoding, this is always assumed to be 1 as it is the only configuration the X1 supports
        u8 blockHeightLog2 : 4; //!< The height of a block in GOBs with log2 encoding
        u8 blockDepthLog2 : 4; //!< The depth of a block in GOBs with log2 encoding, this is always assumed to be 1 as it is the only configuration the X1 supports
        u32 _pad_ : 20;

        u8 BlockWidth() const {
            return 1; // blockWidthLog2 only supports a value of 0
        }

        u8 BlockHeight() const {
            return static_cast<u8>(1 << blockHeightLog2);
        }

        u8 BlockDepth() const {
            return 1;
        }
    };

    struct ZtSize {
        enum class Control : u8 {
            ThirdDimensionDefinesArraySize = 0,
            ArraySizeIsOne = 1,
        };

        u32 width : 28;
        u8 _pad0_ : 4;
        u32 height : 17;
        u16 _pad1_ : 15;
        u16 thirdDimension;
        Control control : 1;
        u16 _pad2_ : 15;
    };

    constexpr static size_t ViewportCount{16}; //!< Amount of viewports on Maxwell 3D, array size for any per-viewport parameter such as transform, scissors, etc

    /**
     * @brief The transformations applied on any primitive sent to a viewport
     */
    struct Viewport {
        float scaleX; //!< Scales all X-axis primitive coordinates by this factor
        float scaleY;
        float scaleZ;
        float offsetX; //!< Translates all X-axis primitive coordinates by this value
        float offsetY;
        float offsetZ;

        /**
         * @brief A component swizzle applied to primitive coordinates prior to clipping/perspective divide with optional negation
         * @note This functionality is exposed via GL_NV_viewport_swizzle (OpenGL) and VK_NV_viewport_swizzle (Vulkan)
         */
        enum class CoordinateSwizzle : u8 {
            PosX = 0,
            NegX = 1,
            PosY = 2,
            NegY = 3,
            PosZ = 4,
            NegZ = 5,
            PosW = 6,
            NegW = 7,
        };

        ENUM_STRING(CoordinateSwizzle, {
            ENUM_CASE_PAIR(PosX, "+X");
            ENUM_CASE_PAIR(NegX, "-X");
            ENUM_CASE_PAIR(PosY, "+Y");
            ENUM_CASE_PAIR(NegY, "-Y");
            ENUM_CASE_PAIR(PosZ, "+Z");
            ENUM_CASE_PAIR(NegZ, "-Z");
            ENUM_CASE_PAIR(PosW, "+W");
            ENUM_CASE_PAIR(NegW, "-W");
        })

        struct {
            CoordinateSwizzle x : 3;
            u8 _pad0_ : 1;
            CoordinateSwizzle y : 3;
            u8 _pad1_ : 1;
            CoordinateSwizzle z : 3;
            u8 _pad2_ : 1;
            CoordinateSwizzle w : 3;
            u32 _pad3_ : 17;
        } swizzle;

        /**
         * @brief The amount of subpixel bits on screen-space axes that bias if a pixel is inside a primitive for conservative rasterization
         * @note This functionality is exposed via GL_NV_conservative_raster (OpenGL) using SubpixelPrecisionBiasNV
         */
        struct {
            u8 x : 5;
            u8 _pad0_ : 3;
            u8 y : 5;
            u32 _pad1_ : 19;
        } snapGridPrecision;
    };
    static_assert(sizeof(Viewport) == (0x8 * sizeof(u32)));

    /**
     * @brief The offset and extent of the viewport for transformation of coordinates from NDC-space (Normalized Device Coordinates) to screen-space
     * @note This is effectively unused since all this data can be derived from the viewport transform, this misses crucial data that the transform has such as depth range order and viewport axis inverse transformations
     */
    struct ViewportClip {
        struct {
            u16 x0;
            u16 width;
        } horizontal;

        struct {
            u16 y0;
            u16 height;
        } vertical;

        float minZ;
        float maxZ;
    };
    static_assert(sizeof(ViewportClip) == (0x4 * sizeof(u32)));

    struct ClearRect {
        struct {
            u16 xMin;
            u16 xMax;
        } horizontal;

        struct {
            u16 yMin;
            u16 yMax;
        } vertical;
    };

    /**
     * @brief The method used to rasterize polygons, not to be confused with the primitive type
     * @note This functionality is exposed via glPolygonMode (OpenGL)
     */
    enum class PolygonMode : u32 {
        Point = 0x1B00, //!< Draw a point for every vertex
        Line = 0x1B01, //!< Draw a line between all vertices
        Fill = 0x1B02, //!< Fill the area bounded by the vertices
    };

    struct PolyOffset {
        u32 pointEnable;
        u32 lineEnable;
        u32 fillEnable;
    };
    static_assert(sizeof(PolyOffset) == (0x3 * sizeof(u32)));

    /**
     * @brief A scissor which is used to reject all writes to non-masked regions
     * @note All coordinates are in screen-space as defined by the viewport
     */
    struct Scissor {
        u32 enable; //!< Rejects non-masked writes when enabled and allows all writes otherwise
        struct {
            u16 xMin;
            u16 xMax;
        } horizontal;

        struct {
            u16 yMin;
            u16 yMax;
        } vertical;
        u32 _pad_;
    };
    static_assert(sizeof(Scissor) == (0x4 * sizeof(u32)));

    constexpr static size_t VertexStreamCount{16}; //!< The maximum amount of vertex buffers that can be bound at once
    constexpr static size_t VertexAttributeCount{32}; //!< The amount of vertex attributes that can be set

    union VertexAttribute {
        u32 raw;

        enum class Source : u8 {
            Active = 0,
            Inactive = 1,
        };

        enum class ComponentBitWidths : u8 {
            R32_G32_B32_A32 = 0x1,
            R32_G32_B32 = 0x2,
            R16_G16_B16_A16 = 0x3,
            R32_G32 = 0x4,
            R16_G16_B16 = 0x5,
            A8B8G8R8 = 0x2F,
            R8_G8_B8_A8 = 0xA,
            X8B8G8R8 = 0x33,
            A2B10G10R10 = 0x30,
            B10G11R11 = 0x31,
            R16_G16 = 0xF,
            R32 = 0x12,
            R8_G8_B8 = 0x13,
            G8R8 = 0x32,
            R8_G8 = 0x18,
            R16 = 0x1B,
            R8 = 0x1D,
            A8 = 0x34
        };

        enum class NumericalType : u8 {
            UnusedEnumDoNotUseBecaseItWillGoAway = 0,
            Snorm = 1,
            Unorm = 2,
            Sint = 3,
            Uint = 4,
            Uscaled = 5,
            Sscaled = 6,
            Float = 7,
        };

        struct {
            u8 stream : 5;
            u8 _pad0_ : 1;
            Source source : 1;
            u16 offset : 14;
            ComponentBitWidths componentBitWidths : 6;
            NumericalType numericalType : 3;
            u8 _pad1_ : 1;
            bool swapRAndB : 1;
        };
    };
    static_assert(sizeof(VertexAttribute) == sizeof(u32));

    constexpr u16 operator|(VertexAttribute::ComponentBitWidths componentBitWidths, VertexAttribute::NumericalType numericalType) {
        return static_cast<u16>(static_cast<u16>(componentBitWidths) | (static_cast<u16>(numericalType) << 6));
    }

    /**
     * @brief A descriptor that controls how the RenderTarget array (at 0x200) will be interpreted
     */
    struct CtSelect {
        u8 count : 4; //!< The amount of active render targets, doesn't necessarily mean bound
        u8 target0 : 3; //!< The index of the render target that maps to slot 0
        u8 target1 : 3;
        u8 target2 : 3;
        u8 target3 : 3;
        u8 target4 : 3;
        u8 target5 : 3;
        u8 target6 : 3;
        u8 target7 : 3;

        size_t operator[](size_t index) const {
            switch (index) {
                case 0:
                    return target0;
                case 1:
                    return target1;
                case 2:
                    return target2;
                case 3:
                    return target3;
                case 4:
                    return target4;
                case 5:
                    return target5;
                case 6:
                    return target6;
                case 7:
                    return target7;
                default:
                    throw exception("Invalid RT index is being mapped: {}", index);
            }
        }
    };
    static_assert(sizeof(CtSelect) == sizeof(u32));



    constexpr static size_t BlendColorChannelCount{4}; //!< The amount of color channels in operations such as blending

    enum class BlendOp : u32 {
        OglFuncSubtract = 0x0000800A,
        OglFuncReverseSubtract = 0x0000800B,
        OglFuncAdd = 0x00008006,
        OglMin = 0x00008007,
        OglMax = 0x00008008,
        D3DAdd = 0x00000001,
        D3DSubtract = 0x00000002,
        D3DRevSubtract = 0x00000003,
        D3DMin = 0x00000004,
        D3DMax = 0x00000005,
    };

    enum class BlendCoeff : u32 {
        OglZero = 0x4000,
        OglOne = 0x4001,
        OglSrcColor = 0x4300,
        OglOneMinusSrcColor = 0x4301,
        OglSrcAlpha = 0x4302,
        OglOneMinusSrcAlpha = 0x4303,
        OglDstAlpha = 0x4304,
        OglOneMinusDstAlpha = 0x4305,
        OglDstColor = 0x4306,
        OglOneMinusDstColor = 0x4307,
        OglSrcAlphaSaturate = 0x4308,
        OglConstantColor = 0xC001,
        OglOneMinusConstantColor = 0xC002,
        OglConstantAlpha = 0xC003,
        OglOneMinusConstantAlpha = 0xC004,
        OglSrc1Color = 0xC900,
        OglInvSrc1Color = 0xC901,
        OglSrc1Alpha = 0xC902,
        OglInvSrc1Alpha = 0xC903,
        D3DZero = 0x1,
        D3DOne = 0x2,
        D3DSrcColor = 0x3,
        D3DInvSrcColor = 0x4,
        D3DSrcAlpha = 0x5,
        D3DInvSrcAlpha = 0x6,
        D3DDstAlpha = 0x7,
        D3DInvDstAlpha = 0x8,
        D3DDstColor = 0x9,
        D3DInvDstColor = 0xA,
        D3DSrcAlphaSaturate = 0xB,
        D3DBlendCoeff = 0xE,
        D3DInvBlendCoeff = 0xF,
        D3DSrc1Color = 0x10,
        D3DInvSrc1Color = 0x11,
        D3DSrc1Alpha = 0x12,
        D3DInvSrc1Alpha = 0x13,
    };

    struct ZtSelect {
        u8 targetCount : 1;
        u32 _pad_ : 31;
    };

    struct MultisampleControl {
        bool alphaToCoverage : 1;
        u8 _pad0_ : 3;
        bool alphaToOne : 1;
        u32 _pad1_ : 27;
    };
    static_assert(sizeof(MultisampleControl) == sizeof(u32));

    struct SamplerBinding {
        enum class Value : u8 {
            Independently = 0,
            ViaHeaderBinding = 1
        };

        Value value : 1;
        u32 _pad_ : 31;
    };
    static_assert(sizeof(SamplerBinding) == sizeof(u32));

    enum class CompareFunc : u32 {
        D3DNever = 1,
        D3DLess = 2,
        D3DEqual = 3,
        D3DLessEqual = 4,
        D3DGreater = 5,
        D3DNotEqual = 6,
        D3DGreaterEqual = 7,
        D3DAlways = 8,

        OglNever = 0x200,
        OglLess = 0x201,
        OglEqual = 0x202,
        OglLEqual = 0x203,
        OglGreater = 0x204,
        OglNotEqual = 0x205,
        OglGEqual = 0x206,
        OglAlways = 0x207,
    };

    struct StencilOps {
        enum class Op : u32 {
            OglZero = 0,
            D3DKeep = 1,
            D3DZero = 2,
            D3DReplace = 3,
            D3DIncrSat = 4,
            D3DDecrSat = 5,
            D3DInvert = 6,
            D3DIncr = 7,
            D3DDecr = 8,
            OglKeep = 0x1E00,
            OglReplace = 0x1E01,
            OglIncrSat = 0x1E02,
            OglDecrSat = 0x1E03,
            OglInvert = 0x150A,
            OglIncr = 0x8507,
            OglDecr = 0x8508
        };


        Op fail; // 0x4E1
        Op zFail; // 0x4E2
        Op zPass; // 0x4E3
        CompareFunc func; // 0x4E4
    };

    struct StencilValues {
        u32 funcRef;
        u32 funcMask;
        u32 mask;
    };

    struct BackStencilValues {
        u32 funcRef;
        u32 mask;
        u32 funcMask;
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

    enum class DrawTopology : u16 {
        Points = 0x0,
        Lines = 0x1,
        LineLoop = 0x2,
        LineStrip = 0x3,
        Triangles = 0x4,
        TriangleStrip = 0x5,
        TriangleFan = 0x6,
        Quads = 0x7,
        QuadStrip = 0x8,
        Polygon = 0x9,
        LineListAdjcy = 0xA,
        LineStripAdjcy = 0xB,
        TriangleListAdjcy = 0xC,
        TriangleStripAdjcy = 0xD,
        Patch = 0xE,
    };

    ENUM_STRING(DrawTopology, {
        ENUM_CASE(Points);
        ENUM_CASE(Lines);
        ENUM_CASE(LineLoop);
        ENUM_CASE(LineStrip);
        ENUM_CASE(Triangles);
        ENUM_CASE(TriangleStrip);
        ENUM_CASE(TriangleFan);
        ENUM_CASE(Quads);
        ENUM_CASE(QuadStrip);
        ENUM_CASE(Polygon);
        ENUM_CASE(LineListAdjcy);
        ENUM_CASE(LineStripAdjcy);
        ENUM_CASE(TriangleListAdjcy);
        ENUM_CASE(TriangleStripAdjcy);
        ENUM_CASE(Patch);
    })

    enum class FrontFace : u32 {
        CW = 0x900,
        CCW = 0x901,
    };

    enum class CullFace : u32 {
        Front = 0x404,
        Back = 0x405,
        FrontAndBack = 0x408,
    };

    union ViewportClipControl {
        u32 raw;
        enum class PixelZ : u8 {
            Clip = 0,
            Clamp = 1
        };

        enum class GuardbandScale : u8 {
            Scale256,
            Scale1
        };

        enum class GeometryClip : u8 {
            WZeroClip = 0,
            Passthru = 1,
            FrustrumXYClip = 2,
            FrustrumXYZClip = 3,
            WZeroClipNoZCull = 4,
            FrustrumZClip = 5,
            WZeroTriFillOrClip = 6
        };

        enum class GuardbandZScale : u8 {
            SameAsXYGuardband = 0,
            Scale256 = 1,
            Scale1 = 2
        };

        struct {
            bool minZZeroMaxZOne : 1;
            GuardbandZScale guardbandZScale : 2;
            PixelZ pixelMinZ : 1;
            PixelZ pixelMaxZ : 1;
            u8 _pad1_ : 2;
            GuardbandScale geometryGuardbandScale : 1;
            u8 _pad2_ : 2;
            GuardbandScale linePointCullGuardbandScale : 1;
            GeometryClip geometryClip : 3;
            u32 _pad3_ : 18;
        };
    };
    static_assert(sizeof(ViewportClipControl) == sizeof(u32));

    union CtWrite {
        bool Any() const {
            return rEnable || gEnable || bEnable || aEnable;
        }

        u32 raw;

        struct {
            bool rEnable : 1;
            u8 _pad0_ : 3;
            bool gEnable : 1;
            u8 _pad1_ : 3;
            bool bEnable : 1;
            u8 _pad2_ : 3;
            bool aEnable : 1;
            u32 _pad3_ : 19;
        };
    };
    static_assert(sizeof(CtWrite) == sizeof(u32));

    /**
     * @brief A method call which causes a layer of an RT to be cleared with a channel mask
     */
    struct ClearSurface {
        bool zEnable : 1; //!< If the depth channel should be cleared
        bool stencilEnable : 1;
        bool rEnable : 1;
        bool gEnable : 1;
        bool bEnable : 1;
        bool aEnable : 1;
        u8 mrtSelect : 4; //!< The ID of the render target to clear
        u16 rtArrayIndex : 11; //!< The index of the layer of the render target to clear
        u16 _pad_ : 10;
    };
    static_assert(sizeof(ClearSurface) == sizeof(u32));

    struct ClearReportValue {
        enum class Type : u32 {
            ZPassPixelCount = 0x01,
            ZCullStats = 0x02,
            StreamingPrimitvesNeededMinusSucceeded = 0x03,
            AlphaBetaClocks = 0x04,
            StreamingPrimitivesSucceeded = 0x10,
            StreamingPrimitivesNeeded = 0x11,
            VerticesGenerated = 0x12,
            PrimitivesGenerated = 0x13,
            VertexShaderInvocations = 0x15,
            TessellationInitInvocations = 0x16,
            TessellationShaderInvocations = 0x17,
            TessellationShaderPrimitivesGenerated = 0x18,
            GeometryShaderInvocations = 0x1A,
            GeometryShaderPrimitivesGenerated = 0x1B,
            ClipperInvocations = 0x1C,
            ClipperPrimitivesGenerated = 0x1D,
            PixelShaderInvocations = 0x1E,
            VtgPrimitivesOut = 0x1F
        };

        Type type : 5;
        u32 _pad_ : 27;
    };
    static_assert(sizeof(ClearReportValue) == sizeof(u32));

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

    constexpr static size_t ShaderStageCount{5}; //!< Amount of pipeline stages on Maxwell 3D

    /**
     * @brief All the pipeline stages that Maxwell3D supports for draws
     */
    enum class ShaderStage {
        Vertex = 0,
        TessellationControl = 1,
        TessellationEvaluation = 2,
        Geometry = 3,
        Fragment = 4,
    };

    struct ConstantBufferSelector {
        u32 size : 17;
        u16 _pad_ : 15;
        Address address;
    };
    static_assert(sizeof(ConstantBufferSelector) == sizeof(u32) * 3);

    struct BindGroup {
        u32 _pad0_[4];
        union {
            struct {
                u32 valid : 1;
                u32 _pad1_ : 3;
                u32 shaderSlot : 5; //!< The index of the constant buffer in the pipeline stage to bind to
            };
            u32 raw;
        } constantBuffer;
        u32 _pad2_[3];
    };
    static_assert(sizeof(BindGroup) == (sizeof(u32) * 8));

    constexpr static size_t ShaderStageConstantBufferCount{18}; //!< Maximum amount of constant buffers that can be bound to a single pipeline stage

    constexpr static size_t PipelineCount{6}; //!< Amount of shader stages on Maxwell 3D

    /**
     * @brief The arguments to set a shader program for a pipeline stage
     */
    struct Pipeline {
        struct Shader {
            enum class Type : u8 {
                VertexCullBeforeFetch = 0,
                Vertex = 1,
                TessellationInit = 2,
                Tessellation = 3,
                Geometry = 4,
                Pixel = 5,
            };

            bool enable : 1;
            u8 _pad0_ : 3;
            Type type : 4;
            u32 _pad1_ : 24;
        } shader;
        u32 programOffset; //!< Offset from the base shader memory IOVA
        u32 _pad2_;
        u8 registerCount; //!< Amount of GPRs used by the shader program
        u32 _pad3_ : 24;
        u8 bindingGroup : 3;
        u32 _pad4_ : 29;
        u32 _pad5_[11];
    };
    static_assert(sizeof(Pipeline) == (sizeof(u32) * 0x10));

    struct ProvokingVertex {
        enum class Value : u8 {
            First = 0,
            Last = 1,
        } value : 1;
        u32 _pad_ : 31;
    };
    static_assert(sizeof(ProvokingVertex) == sizeof(u32));

    struct ZtLayer {
        u16 offset;
        u16 _pad_;
    };

    struct IndexBuffer {
        Address address, limit; //!< The IOVA bounds of the index buffer
        enum class IndexSize : u32 {
            OneByte = 0,
            TwoBytes = 1,
            FourBytes = 2,
        } indexSize;
        u32 first;
    };

    enum class ZClipRange : u32 {
        NegativeWToPositiveW = 0,
        ZeroToPositiveW = 1
    };

    constexpr static size_t StreamOutBufferCount{4}; //!< Number of supported transform feecback buffers in the 3D engine

    struct StreamOutBuffer {
        u32 enable;
        Address address;
        u32 size;
        u32 loadWritePointerStartOffset;
        u32 _pad_[3];
    };
    static_assert(sizeof(StreamOutBuffer) == (sizeof(u32) * 8));

    struct StreamOutControl {
        u8 streamSelect : 2;
        u32 _pad0_ : 30;
        u8 componentCount : 8;
        u32 _pad1_ : 24;
        u32 strideBytes;
        u32 _pad_;
    };
    static_assert(sizeof(StreamOutControl) == (sizeof(u32) * 4));

    constexpr static size_t StreamOutLayoutSelectAttributeCount{0x80};

    struct VertexStream {
        union {
            u32 raw;
            struct {
                u32 stride : 12;
                u32 enable : 1;
            };
        } format;
        Address location;
        u32 frequency;
    };
    static_assert(sizeof(VertexStream) == sizeof(u32) * 4);

    struct BlendPerTarget {
        bool seperateForAlpha : 1;
        u32 _pad0_ : 31;
        BlendOp colorOp;
        BlendCoeff colorSourceCoeff;
        BlendCoeff colorDestCoeff;
        BlendOp alphaOp;
        BlendCoeff alphaSourceCoeff;
        BlendCoeff alphaDestCoeff;
        u32 _pad_;
    };

    struct Blend {
        bool seperateForAlpha : 1;
        u32 _pad0_ : 31;
        BlendOp colorOp;
        BlendCoeff colorSourceCoeff;
        BlendCoeff colorDestCoeff;
        BlendOp alphaOp;
        BlendCoeff alphaSourceCoeff;
        bool globalColorKeyEnable : 1;
        u32 _pad1_ : 31;
        BlendCoeff alphaDestCoeff;
        bool singleRopControlEnable : 1;
        u32 _pad2_ : 31;
        std::array<u32, ColorTargetCount> enable;
    };

    struct WindowOrigin {
        bool lowerLeft : 1;
        u8 _pad0_ : 3;
        bool flipY : 1;
        u32 _pad1_ : 27;
    };
    static_assert(sizeof(WindowOrigin) == sizeof(u32));

    struct SurfaceClip {
        struct {
            u16 x;
            u16 width;
        } horizontal;

        struct {
            u16 y;
            u16 height;
        } vertical;
    };
    static_assert(sizeof(SurfaceClip) == sizeof(u32) * 2);

    struct ClearSurfaceControl {
        bool respectStencilMask : 1;
        u8 _pad0_ : 3;
        bool useClearRect : 1;
        u8 _pad1_ : 3;
        bool useScissor0 : 1;
        u8 _pad2_ : 3;
        bool useViewportClip0 : 1;
        u32 _pad3_ : 19;
    };
    static_assert(sizeof(ClearSurfaceControl) == sizeof(u32));

    struct VertexStreamInstance {
        bool isInstanced : 1;
        u32 _pad0_ : 31;
    };
    static_assert(sizeof(VertexStreamInstance) == sizeof(u32));

    struct PrimitiveTopologyControl {
        enum class Override : u8 {
            UseTopologyInBeginMethods = 0,
            UseSeperateTopologyState = 1
        };

        Override override : 1;
        u32 _pad0_ : 31;
    };
    static_assert(sizeof(PrimitiveTopologyControl) == sizeof(u32));

    enum class PrimitiveTopology : u16 {
        PointList = 0x1,
        LineList = 0x2,
        LineStrip = 0x3,
        TriangleList = 0x4,
        TriangleStrip = 0x5,
        LineListAdjcy = 0xA,
        LineStripAdjcy = 0xB,
        TriangleListAdjcy = 0xC,
        TriangleStripAdjcy = 0xD,
        PatchList = 0xE,
        LegacyPoints = 0x1001,
        LegacyIndexedLineList = 0x1002,
        LegacyIndexedTriangleList = 0x1003,
        LegacyLineList = 0x100F,
        LegacyLineStrip = 0x1010,
        LegacyIndexedLineStrip = 0x1011,
        LegacyTriangleList = 0x1012,
        LegacyTriangleStrip = 0x1013,
        LegacyIndexedTriangleStrip = 0x1014,
        LegacyTriangleFan = 0x1015,
        LegacyIndexedTriangleFan = 0x1016,
        LegacyTriangleFanImm = 0x1017,
        LegacyLineListImm = 0x1018,
        LegacyIndexedTriangleList2 = 0x101A,
        LegacyIndexedLineList2 = 0x101B
    };

    inline DrawTopology ConvertPrimitiveTopologyToDrawTopology(PrimitiveTopology topology) {
        switch (topology) {
            case PrimitiveTopology::PointList:
            case PrimitiveTopology::LegacyPoints:
                return DrawTopology::Points;
            case PrimitiveTopology::LineList:
            case PrimitiveTopology::LegacyLineList:
                return DrawTopology::Lines;
            case PrimitiveTopology::LineStrip:
            case PrimitiveTopology::LegacyLineStrip:
                return DrawTopology::LineStrip;
            case PrimitiveTopology::TriangleList:
            case PrimitiveTopology::LegacyTriangleList:
                return DrawTopology::Triangles;
            case PrimitiveTopology::TriangleStrip:
            case PrimitiveTopology::LegacyTriangleStrip:
                return DrawTopology::TriangleStrip;
            case PrimitiveTopology::LegacyTriangleFan:
                return DrawTopology::TriangleFan;
            case PrimitiveTopology::LineListAdjcy:
                return DrawTopology::LineListAdjcy;
            case PrimitiveTopology::LineStripAdjcy:
                return DrawTopology::LineStripAdjcy;
            case PrimitiveTopology::TriangleListAdjcy:
                return DrawTopology::TriangleListAdjcy;
            case PrimitiveTopology::TriangleStripAdjcy:
                return DrawTopology::TriangleStripAdjcy;
            case PrimitiveTopology::PatchList:
                return DrawTopology::Patch;
            default:
                throw exception("Unsupported primitive topology 0x{:X}", static_cast<u16>(topology));
        }
    }

    struct TessellationParameters {
        enum class DomainType : u8 {
            Isoline = 0,
            Triangle = 1,
            Quad = 2
        };

        enum class Spacing : u8 {
            Integer = 0,
            FractionalOdd = 1,
            FractionalEven = 2
        };

        enum class OutputPrimitives : u8 {
            Points = 0,
            Lines = 1,
            TrianglesCW = 2,
            TrianglesCCW = 3
        };

        DomainType domainType : 2;
        u8 _pad0_ : 2;
        Spacing spacing : 2;
        u8 _pad1_ : 2;
        OutputPrimitives outputPrimitives : 2;
        u32 _pad2_ : 22;
    };
    static_assert(sizeof(TessellationParameters) == sizeof(u32));

    struct LogicOp {
        bool enable : 1;
        u32 _pad0_ : 31;
        enum class Func : u32 {
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
            Set = 0x150F
        } func;
    };

    #pragma pack(pop)
}
