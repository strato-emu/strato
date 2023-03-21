// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Copyright © 2022 Ryujinx Team and Contributors (https://github.com/Ryujinx/)
// Copyright © 2018-2020 fincs (https://github.com/devkitPro/deko3d)

#pragma once

#include <gpu/interconnect/maxwell_3d/maxwell_3d.h>
#include <soc/host1x/syncpoint.h>
#include "engine.h"
#include "inline2memory.h"
#include "maxwell/types.h"

namespace skyline::soc::gm20b {
    struct ChannelContext;
}

namespace skyline::soc::gm20b::engine::maxwell3d {
    /**
     * @brief The Maxwell 3D engine handles processing 3D graphics
     */
    class Maxwell3D : public MacroEngineBase {
      private:
        host1x::SyncpointSet &syncpoints;
        Inline2MemoryBackend i2m;
        gpu::interconnect::DirtyManager dirtyManager;
        gpu::interconnect::maxwell3d::Maxwell3D interconnect;

        union BatchEnableState {
            u8 raw{};

            struct {
                bool constantBufferActive : 1;
                bool drawActive : 1;
            };
        } batchEnableState{};

        struct BatchLoadConstantBufferState {
            std::vector<u32> buffer;
            u32 startOffset{};

            void Reset() {
                buffer.clear();
            }
        } batchLoadConstantBuffer; //!< Holds state for updating constant buffer data in a batch rather than word by word

        /**
         * @brief In the Maxwell 3D engine, instanced draws are implemented by repeating the exact same draw in sequence with special flag set in vertexBeginGl. This flag allows either incrementing the instance counter or resetting it, since we need to supply an instance count to the host API we defer all draws until state changes occur. If there are no state changes between draws we can skip them and count the occurences to get the number of instances to draw.
         */
        struct DeferredDrawState {
            bool indexed; //!< If the deferred draw is indexed
            type::DrawTopology drawTopology; //!< Topology of draw at draw time
            u32 instanceCount{1}; //!< Number of instances in the final draw
            u32 drawCount; //!< indexed ? drawIndexCount : drawVertexCount
            u32 drawFirst; //!< indexed ? drawIndexFirst : drawVertexFirst
            u32 drawBaseVertex; //!< Only applicable to indexed draws
            u32 drawBaseInstance;

            /**
             * @brief Sets up the state necessary to defer a new draw
             */
            void Set(u32 pDrawCount, u32 pDrawFirst, u32 pDrawBaseVertex, u32 pDrawBaseInstance, type::DrawTopology pDrawTopology, bool pIndexed) {
                indexed = pIndexed;
                drawTopology = pDrawTopology;
                drawCount = pDrawCount;
                drawFirst = pDrawFirst;
                drawBaseVertex = pDrawBaseVertex;
                drawBaseInstance = pDrawBaseInstance;
            }
        } deferredDraw{};

        bool CheckRenderEnable();

        type::DrawTopology ApplyTopologyOverride(type::DrawTopology beginMethodTopology);

        void FlushDeferredDraw();

        /**
         * @brief Calls the appropriate function corresponding to a certain method with the supplied argument
         */
        void HandleMethod(u32 method, u32 argument);

      public:
        /**
         * @url https://github.com/devkitPro/deko3d/blob/master/source/maxwell/engine_3d.def
         */
        #pragma pack(push, 1)
        union Registers {
            std::array<u32, EngineMethodsEnd> raw;

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

            Register<0x60, Inline2MemoryBackend::RegisterState> i2m;

            Register<0x84, u32> apiMandatedEarlyZEnable;

            Register<0xB2, type::SyncpointAction> syncpointAction;

            struct DrawZeroIndex {
                u32 count;
            };
            Register<0xC1, DrawZeroIndex> drawZeroIndex;

            Register<0xC8, type::TessellationParameters> tessellationParameters;

            Register<0xDF, u32> rasterEnable;

            Register<0xE0, std::array<type::StreamOutBuffer, type::StreamOutBufferCount>> streamOutBuffers;
            Register<0x1C0, std::array<type::StreamOutControl, type::StreamOutBufferCount>> streamOutControls;
            Register<0x1D1, u32> streamOutputEnable;

            Register<0x200, std::array<type::ColorTarget, type::ColorTargetCount>> colorTargets;
            Register<0x280, std::array<type::Viewport, type::ViewportCount>> viewports;
            Register<0x300, std::array<type::ViewportClip, type::ViewportCount>> viewportClips;

            Register<0x35B, type::ClearRect> clearRect;

            Register<0x35D, u32> vertexArrayStart; //!< The first vertex to draw

            struct DrawVertexArray {
                u32 count;
            };

            Register<0x35E, DrawVertexArray> drawVertexArray; //!< The amount of vertices to draw, calling this method triggers non-indexed drawing

            Register<0x35F, type::ZClipRange> zClipRange;

            Register<0x360, std::array<u32, 4>> colorClearValue;
            Register<0x364, float> zClearValue;
            Register<0x368, u32> stencilClearValue;

            Register<0x36B, type::PolygonMode> frontPolygonMode;
            Register<0x36C, type::PolygonMode> backPolygonMode;

            Register<0x370, type::PolyOffset> polyOffset;

            Register<0x373, u32> patchSize;

            Register<0x380, std::array<type::Scissor, type::ViewportCount>> scissors;

            struct DepthBiasEnable {
                u32 point; // 0x370
                u32 line; // 0x371
                u32 fill; // 0x372
            };
            Register<0x370, DepthBiasEnable> depthBiasEnable;

            Register<0x3D5, type::BackStencilValues> backStencilValues;

            Register<0x3D8, u32> tiledCacheEnable;
            struct TiledCacheSize {
                u16 width;
                u16 height;
            };
            Register<0x3D9, TiledCacheSize> tiledCacheSize;

            Register<0x3E4, u32> singleCtWriteControl; //!< If enabled, the color write masks for all RTs must be set to that of the first RT

            Register<0x3E7, float> depthBoundsMin;
            Register<0x3E8, float> depthBoundsMax;

            Register<0x3EB, u32> ctMrtEnable;

            Register<0x3F8, Address> ztOffset;
            Register<0x3FA, type::ZtFormat> ztFormat;
            Register<0x3FB, type::ZtBlockSize> ztBlockSize;
            Register<0x3FC, u32> ztArrayPitch;
            Register<0x3FD, type::SurfaceClip> surfaceClip;

            Register<0x43E, type::ClearSurfaceControl> clearSurfaceControl;

            Register<0x458, std::array<type::VertexAttribute, type::VertexAttributeCount>> vertexAttributes;

            Register<0x483, u32> invalidateSamplerCacheAll;
            Register<0x484, u32> invalidateTextureHeaderCacheAll;

            struct DrawVertexArrayBeginEndInstance {
                u16 startIndex;
                u16 count : 12;
                type::DrawTopology topology : 4;
            };

            Register<0x485, DrawVertexArrayBeginEndInstance> drawVertexArrayBeginEndInstanceFirst;
            Register<0x486, DrawVertexArrayBeginEndInstance> drawVertexArrayBeginEndInstanceSubsequent;

            Register<0x487, type::CtSelect> ctSelect;

            Register<0x48A, type::ZtSize> ztSize;

            struct DrawAuto {
                u32 byteCount;
            };
            Register<0x48F, DrawAuto> drawAuto;

            Register<0x48D, type::SamplerBinding> samplerBinding; //!< If enabled, the TSC index in a bindless texture handle is ignored and the TIC index is used as the TSC index, otherwise the TSC index from the bindless texture handle is used

            Register<0x490, std::array<u32, 8>> postVtgShaderAttributeSkipMask;

            Register<0x4B3, u32> depthTestEnable;
            Register<0x4B9, u32> blendStatePerTargetEnable;
            Register<0x4BA, u32> depthWriteEnable;
            Register<0x4BB, u32> alphaTestEnable;

            struct InlineIndexAlign {
                u32 count : 30;
                u8 start : 2;
            };

            struct DrawInlineIndex {
                u8 index0;
                u8 index1;
                u8 index2;
                u8 index3;
            };

            Register<0x4C1, DrawInlineIndex> drawInlineIndex4X8;

            Register<0x4C3, type::CompareFunc> depthFunc;
            Register<0x4C4, float> alphaRef;
            Register<0x4C5, type::CompareFunc> alphaFunc;

            Register<0x4C6, u32> drawAutoStride;

            struct BlendConstant {
                float red; // 0x4C7
                float green; // 0x4C8
                float blue; // 0x4C9
                float alpha; // 0x4CA
            };
            Register<0x4C7, std::array<float, type::BlendColorChannelCount>> blendConsts;

            Register<0x4CF, type::Blend> blend;
            Register<0x4E0, u32> stencilTestEnable;

            Register<0x4E1, type::StencilOps> stencilOps;
            Register<0x4E5, type::StencilValues> stencilValues;

            Register<0x4EB, type::WindowOrigin> windowOrigin;

            Register<0x4EC, float> lineWidth;
            Register<0x4ED, float> lineWidthAliased;

            Register<0x50D, u32> globalBaseVertexIndex;
            Register<0x50E, u32> globalBaseInstanceIndex;

            Register<0x544, u32> clipDistanceEnable;
            Register<0x545, u32> sampleCounterEnable;
            Register<0x546, float> pointSize;
            Register<0x547, u32> zCullStatCountersEnable;
            Register<0x548, u32> pointSpriteEnable;
            Register<0x54A, u32> shaderExceptions;

            Register<0x54C, type::ClearReportValue> clearReportValue;

            Register<0x54D, u32> multisampleEnable;
            Register<0x54E, type::ZtSelect> ztSelect;

            Register<0x54F, type::MultisampleControl> multisampleControl;

            struct RenderEnable {
                enum class Mode : u32 {
                    False = 0,
                    True = 1,
                    Conditional = 2,
                    RenderIfEqual = 3,
                    RenderIfNotEqual = 4,
                };
                Address offset;
                Mode mode;
            };
            Register<0x554, RenderEnable> renderEnable;

            Register<0x557, TexSamplerPool> texSamplerPool;

            Register<0x55B, float> slopeScaleDepthBias;
            Register<0x55C, u32> aliasedLineWidthEnable;

            Register<0x55D, TexHeaderPool> texHeaderPool;

            Register<0x565, u32> twoSidedStencilTestEnable; //!< Determines if the back-facing stencil state uses the front facing stencil state or independent stencil state

            Register<0x566, type::StencilOps> stencilBack;

            Register<0x56F, float> depthBias;

            Register<0x57A, u32> drawInlineIndex;

            struct InlineIndex2X16Align {
                u32 count : 31;
                bool startOdd : 1;
            };

            Register<0x57B, InlineIndex2X16Align> inlineIndex2X16Align;

            struct DrawInlineIndex2X16 {
                u16 even;
                u16 odd;
            };

            Register<0x57C, DrawInlineIndex2X16> drawInlineIndex2X16;

            Register<0x581, type::PointCoordReplace> pointCoordReplace;

            Register<0x582, Address> programRegion;

            Register<0x585, u32> end; //!< Method-only register with no real value, used after calling vertexBeginGl to invoke the draw

            union Begin {
                u32 raw;

                enum class PrimitiveId : u8 {
                    First = 0,
                    Unchanged = 1,
                };

                enum class InstanceId : u8 {
                    First = 0,
                    Subsequent = 1,
                    Unchanged = 2
                };

                enum class SplitMode : u8 {
                    NormalBeginNormalEnd = 0,
                    NormalBeginOpenEnd = 1,
                    OpenBeginOpenEnd = 2,
                    OpenBeginNormalEnd = 3
                };

                struct {
                    type::DrawTopology op;
                    u16 _pad0_ : 8;
                    PrimitiveId primitiveId : 1;
                    u8 _pad1_ : 1;
                    InstanceId instanceId : 2;
                    SplitMode splitMode : 4;
                };
            };
            static_assert(sizeof(Begin) == sizeof(u32));
            Register<0x586, Begin> begin; //!< Similar to glVertexBegin semantically, supplies a primitive topology for draws alongside instancing data

            Register<0x591, u32> primitiveRestartEnable;
            Register<0x592, u32> primitiveRestartIndex;

            Register<0x5A1, type::ProvokingVertex> provokingVertex;

            Register<0x5E7, type::ZtLayer> ztLayer;

            Register<0x5F2, type::IndexBuffer> indexBuffer;

            struct DrawIndexBuffer {
                u32 count;
            };
            Register<0x5F8, DrawIndexBuffer> drawIndexBuffer;

            struct DrawIndexBufferBeginEndInstance {
                u16 first;
                u16 count : 12;
                type::DrawTopology topology : 4;
            };

            Register<0x5F9, DrawIndexBufferBeginEndInstance> drawIndexBuffer32BeginEndInstanceFirst;
            Register<0x5FA, DrawIndexBufferBeginEndInstance> drawIndexBuffer16BeginEndInstanceFirst;
            Register<0x5FB, DrawIndexBufferBeginEndInstance> drawIndexBuffer8BeginEndInstanceFirst;
            Register<0x5FC, DrawIndexBufferBeginEndInstance> drawIndexBuffer32BeginEndInstanceSubsequent;
            Register<0x5FD, DrawIndexBufferBeginEndInstance> drawIndexBuffer16BeginEndInstanceSubsequent;
            Register<0x5FE, DrawIndexBufferBeginEndInstance> drawIndexBuffer8BeginEndInstanceSubsequent;

            Register<0x61F, float> depthBiasClamp;

            Register<0x620, std::array<type::VertexStreamInstance, type::VertexStreamCount>> vertexStreamInstance; //!< A per-VBO boolean denoting if the vertex input rate should be per vertex or per instance

            Register<0x646, u32> oglCullEnable;
            Register<0x647, type::FrontFace> oglFrontFace;
            Register<0x648, type::CullFace> oglCullFace;

            Register<0x649, u32> pixelCentreImage;
            Register<0x64B, u32> viewportScaleOffsetEnable;
            Register<0x64F, type::ViewportClipControl> viewportClipControl;

            struct RenderEnableOverride {
                enum class Mode : u8 {
                    UseRenderEnable = 0,
                    AlwaysRender = 1,
                    NeverRender = 2,
                };

                Mode mode : 2;
                u32 _pad_ : 30;
            };
            Register<0x651, RenderEnableOverride> renderEnableOverride;


            Register<0x652, type::PrimitiveTopologyControl> primitiveTopologyControl;
            Register<0x65C, type::PrimitiveTopology> primitiveTopology;

            Register<0x66F, u32> depthBoundsTestEnable;

            Register<0x671, type::LogicOp> logicOp;

            Register<0x674, type::ClearSurface> clearSurface;
            Register<0x680, std::array<type::CtWrite, type::ColorTargetCount>> ctWrites;

            struct Semaphore {
                Address address; // 0x6C0
                u32 payload; // 0x6C2
                type::SemaphoreInfo info; // 0x6C3
            };
            Register<0x6C0, Semaphore> semaphore;

            Register<0x700, std::array<type::VertexStream, type::VertexStreamCount>> vertexStreams;

            Register<0x780, std::array<type::BlendPerTarget, type::ColorTargetCount>> blendPerTargets;

            Register<0x7C0, std::array<Address, type::VertexStreamCount>> vertexStreamLimits; //!< A per-VBO IOVA denoting the end of the vertex buffer

            Register<0x800, std::array<type::Pipeline, type::PipelineCount>> pipelines;

            Register<0x8C0, u32[0x20]> firmwareCall;


            Register<0x8E0, type::ConstantBufferSelector> constantBufferSelector;

            /**
             * @brief Allows updating the currently selected constant buffer inline with an offset and up to 16 words of data
             */
            struct LoadConstantBuffer {
                u32 offset;
                std::array<u32, 16> data;
            };
            Register<0x8E3, LoadConstantBuffer> loadConstantBuffer;

            Register<0x900, std::array<type::BindGroup, type::ShaderStageCount>> bindGroups; //!< Binds constant buffers to pipeline stages

            Register<0x982, BindlessTexture> bindlessTexture; //!< The index of the constant buffer containing bindless texture descriptors

            Register<0xA00, std::array<std::array<u8, type::StreamOutLayoutSelectAttributeCount>, type::StreamOutBufferCount>> streamOutLayoutSelect;
        };
        static_assert(sizeof(Registers) == (EngineMethodsEnd * sizeof(u32)));
        #pragma pack(pop)

      private:
        /**
         * @brief Writes back a semaphore result to the guest with an auto-generated timestamp (if required)
         * @note If the semaphore is OneWord then the result will be downcasted to a 32-bit unsigned integer
         */
        void WriteSemaphoreResult(const Registers::Semaphore &semaphore, u64 result);

      public:
        Registers registers{};
        Registers shadowRegisters{}; //!< A shadow-copy of the registers, their function is controlled by the 'shadowRamControl' register

        ChannelContext &channelCtx;

        Maxwell3D(const DeviceState &state, ChannelContext &channelCtx, MacroState &macroState);

        /**
         * @brief Initializes Maxwell 3D registers to their default values
         */
        void InitializeRegisters();

        /**
         * @brief Flushes any batched constant buffer update or instanced draw state
         */
        void FlushEngineState();

        void CallMethod(u32 method, u32 argument);

        void CallMethodBatchNonInc(u32 method, span<u32> arguments);

        void CallMethodFromMacro(u32 method, u32 argument) override;

        u32 ReadMethodFromMacro(u32 method) override;

        void DrawInstanced(u32 drawTopology, u32 vertexArrayCount, u32 instanceCount, u32 vertexArrayStart, u32 globalBaseInstanceIndex) override;

        void DrawIndexedInstanced(u32 drawTopology, u32 indexBufferCount, u32 instanceCount, u32 globalBaseVertexIndex, u32 indexBufferFirst, u32 globalBaseInstanceIndex) override;

        void DrawIndexedIndirect(u32 drawTopology, span<u8> indirectBuffer, u32 count, u32 stride) override;
    };
}
