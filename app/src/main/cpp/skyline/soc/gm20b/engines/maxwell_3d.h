// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Copyright © 2022 Ryujinx Team and Contributors (https://github.com/Ryujinx/)
// Copyright © 2018-2020 fincs (https://github.com/devkitPro/deko3d)

#pragma once

#include <gpu/interconnect/graphics_context.h>
#include "engine.h"
#include "inline2memory.h"

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
        gpu::interconnect::GraphicsContext context;
        Inline2MemoryBackend i2m;

        struct BatchConstantBufferUpdateState {
            std::vector<u32> buffer;
            u32 startOffset{std::numeric_limits<u32>::max()};

            bool Active() {
                return startOffset != std::numeric_limits<u32>::max();
            }

            u32 Invalidate() {
                return std::exchange(startOffset, std::numeric_limits<u32>::max());
            }

            void Reset() {
                buffer.clear();
            }
        } batchConstantBufferUpdate; //!< Holds state for updating constant buffer data in a batch rather than word by word

        /**
         * @brief In the Maxwell 3D engine, instanced draws are implemented by repeating the exact same draw in sequence with special flag set in vertexBeginGl. This flag allows either incrementing the instance counter or resetting it, since we need to supply an instance count to the host API we defer all draws until state changes occur. If there are no state changes between draws we can skip them and count the occurences to get the number of instances to draw.
         */
        struct DeferredDrawState {
            bool pending;
            bool indexed; //!< If the deferred draw is indexed
            type::PrimitiveTopology drawTopology; //!< Topology of draw at draw time
            u32 instanceCount{1}; //!< Number of instances in the final draw
            u32 drawCount; //!< indexed ? drawIndexCount : drawVertexCount
            u32 drawFirst; //!< indexed ? drawIndexFirst : drawVertexFirst
            i32 drawBaseVertex; //!< Only applicable to indexed draws

            /**
             * @brief Sets up the state necessary to defer a new draw
             */
            void Set(u32 pDrawCount, u32 pDrawFirst, i32 pDrawBaseVertex, type::PrimitiveTopology pDrawTopology, bool pIndexed) {
                pending = true;
                indexed = pIndexed;
                drawTopology = pDrawTopology;
                drawCount = pDrawCount;
                drawFirst = pDrawFirst;
                drawBaseVertex = pDrawBaseVertex;
            }
        } deferredDraw{};

        void FlushDeferredDraw();

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

            Register<0xB2, type::SyncpointAction> syncpointAction;

            union TessellationMode {
                struct {
                    type::TessellationPrimitive primitive : 2;
                    u8 _pad0_ : 2;
                    type::TessellationSpacing spacing : 2;
                    u8 _pad1_ : 2;
                    type::TessellationWinding winding : 2;
                };
                u32 raw;
            };
            Register<0xC8, TessellationMode> tessellationMode;

            Register<0xDF, u32> rasterizerEnable;

            Register<0xE0, std::array<type::TransformFeedbackBuffer, type::TransformFeedbackBufferCount>> transformFeedbackBuffers;
            Register<0x1C0, std::array<type::TransformFeedbackBufferState, type::TransformFeedbackBufferCount>> transformFeedbackBufferStates;
            Register<0x1D1, u32> transformFeedbackEnable;

            Register<0x200, std::array<type::ColorRenderTarget, type::RenderTargetCount>> renderTargets;
            Register<0x280, std::array<type::ViewportTransform, type::ViewportCount>> viewportTransforms;
            Register<0x300, std::array<type::Viewport, type::ViewportCount>> viewports;

            Register<0x35D, u32> drawVertexFirst; //!< The first vertex to draw
            Register<0x35E, u32> drawVertexCount; //!< The amount of vertices to draw, calling this method triggers non-indexed drawing

            Register<0x35F, type::DepthMode> depthMode;

            Register<0x360, std::array<u32, 4>> clearColorValue;
            Register<0x364, float> clearDepthValue;
            Register<0x368, u32> clearStencilValue;

            struct PolygonMode {
                type::PolygonMode front; // 0x36B
                type::PolygonMode back; // 0x36C
            };
            Register<0x36B, PolygonMode> polygonMode;

            Register<0x373, u32> tessellationPatchSize;

            Register<0x380, std::array<type::Scissor, type::ViewportCount>> scissors;

            struct DepthBiasEnable {
                u32 point; // 0x370
                u32 line; // 0x371
                u32 fill; // 0x372
            };
            Register<0x370, DepthBiasEnable> depthBiasEnable;

            struct StencilBackExtra {
                u32 compareReference; // 0x3D5
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

            Register<0x3E7, float> depthBoundsNear;
            Register<0x3E8, float> depthBoundsFar;

            Register<0x3EB, u32> rtSeparateFragData;

            Register<0x3F8, Address> depthTargetAddress;
            Register<0x3FA, type::DepthRtFormat> depthTargetFormat;
            Register<0x3FB, type::RenderTargetTileMode> depthTargetTileMode;
            Register<0x3FC, u32> depthTargetLayerStride;

            Register<0x458, std::array<type::VertexAttribute, type::VertexAttributeCount>> vertexAttributeState;

            Register<0x487, type::RenderTargetControl> renderTargetControl;

            Register<0x48A, u32> depthTargetWidth;
            Register<0x48B, u32> depthTargetHeight;
            Register<0x48C, type::RenderTargetArrayMode> depthTargetArrayMode;

            Register<0x48D, bool> linkedTscHandle; //!< If enabled, the TSC index in a bindless texture handle is ignored and the TIC index is used as the TSC index, otherwise the TSC index from the bindless texture handle is used

            Register<0x4B3, u32> depthTestEnable;
            Register<0x4B9, u32> independentBlendEnable;
            Register<0x4BA, u32> depthWriteEnable;
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
                type::StencilOp passOp; // 0x4E3

                type::CompareOp compareOp; // 0x4E4
                u32 compareReference; // 0x4E5
                u32 compareMask; // 0x4E6

                u32 writeMask; // 0x4E7
            };
            Register<0x4E1, StencilFront> stencilFront;

            struct WindowOriginMode {
                bool isOriginLowerLeft : 1;
                u8 _pad_ : 3;
                bool flipFrontFace : 1;
            };
            Register<0x4EB, WindowOriginMode> windowOriginMode;

            Register<0x4EC, float> lineWidthSmooth;
            Register<0x4ED, float> lineWidthAliased;

            Register<0x50D, i32> drawBaseVertex;
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
                Address address; // 0x557
                u32 maximumIndex; // 0x559
            };
            Register<0x557, SamplerPool> samplerPool;

            Register<0x55B, float> depthBiasFactor;
            Register<0x55C, u32> lineSmoothEnable;

            struct TexturePool {
                Address address; // 0x55D
                u32 maximumIndex; // 0x55F
            };
            Register<0x55D, TexturePool> texturePool;

            Register<0x565, u32> stencilTwoSideEnable; //!< Determines if the back-facing stencil state uses the front facing stencil state or independent stencil state

            struct StencilBack {
                type::StencilOp failOp; // 0x566
                type::StencilOp zFailOp; // 0x567
                type::StencilOp passOp; // 0x568
                type::CompareOp compareOp; // 0x569
            };
            Register<0x566, StencilBack> stencilBack;

            Register<0x56F, float> depthBiasUnits;

            Register<0x581, type::PointCoordReplace> pointCoordReplace;
            Register<0x582, Address> setProgramRegion;

            Register<0x585, u32> vertexEndGl; //!< Method-only register with no real value, used after calling vertexBeginGl to invoke the draw
            Register<0x586, type::VertexBeginGl> vertexBeginGl; //!< Similar to glVertexBegin semantically, supplies a primitive topology for draws alongside instancing data

            Register<0x591, u32> primitiveRestartEnable;
            Register<0x592, u32> primitiveRestartIndex;

            Register<0x5A1, u32> provokingVertexIsLast;

            Register<0x5F2, type::IndexBuffer> indexBuffer;

            Register<0x5F7, u32> drawIndexFirst; //!< The first element in the index buffer to draw
            Register<0x5F8, u32> drawIndexCount; //!< The amount of elements to draw, calling this method triggers indexed drawing

            Register<0x61F, float> depthBiasClamp;

            Register<0x620, std::array<u32, type::VertexBufferCount>> isVertexInputRatePerInstance; //!< A per-VBO boolean denoting if the vertex input rate should be per vertex or per instance

            Register<0x646, u32> cullFaceEnable;
            Register<0x647, type::FrontFace> frontFace;
            Register<0x648, type::CullFace> cullFace;

            Register<0x649, u32> pixelCentreImage;
            Register<0x64B, u32> viewportTransformEnable;
            Register<0x64F, type::ViewVolumeClipControl> viewVolumeClipControl;

            Register<0x66F, u32> depthBoundsEnable;

            struct ColorLogicOp {
                u32 enable;
                type::ColorLogicOp type;
            };
            Register<0x671, ColorLogicOp> colorLogicOp;

            Register<0x674, type::ClearBuffers> clearBuffers;
            Register<0x680, std::array<type::ColorWriteMask, type::RenderTargetCount>> colorWriteMask;

            struct Semaphore {
                Address address; // 0x6C0
                u32 payload; // 0x6C2
                type::SemaphoreInfo info; // 0x6C3
            };
            Register<0x6C0, Semaphore> semaphore;

            struct VertexBuffer {
                union {
                    u32 raw;
                    struct {
                        u32 stride : 12;
                        u32 enable : 1;
                    };
                } config;
                Address iova;
                u32 divisor;
            };
            static_assert(sizeof(VertexBuffer) == sizeof(u32) * 4);
            Register<0x700, std::array<VertexBuffer, type::VertexBufferCount>> vertexBuffers;

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

            Register<0x7C0, std::array<Address, type::VertexBufferCount>> vertexBufferLimits; //!< A per-VBO IOVA denoting the end of the vertex buffer

            Register<0x800, std::array<type::SetProgramInfo, type::ShaderStageCount>> setProgram;

            Register<0x8C0, u32[0x20]> firmwareCall;

            struct ConstantBufferSelector {
                u32 size;
                Address address;
            };
            Register<0x8E0, ConstantBufferSelector> constantBufferSelector;

            /**
             * @brief Allows updating the currently selected constant buffer inline with an offset and up to 16 words of data
             */
            struct ConstantBufferUpdate {
                u32 offset;
                std::array<u32, 16> data;
            };
            Register<0x8E3, ConstantBufferUpdate> constantBufferUpdate;

            Register<0x900, std::array<type::Bind, type::PipelineStageCount>> bind; //!< Binds constant buffers to pipeline stages

            Register<0x982, u32> bindlessTextureConstantBufferIndex; //!< The index of the constant buffer containing bindless texture descriptors

            Register<0xA00, std::array<u32, (type::TransformFeedbackVaryingCount / sizeof(u32))  * type::TransformFeedbackBufferCount>> transformFeedbackVaryings;
        };
        static_assert(sizeof(Registers) == (EngineMethodsEnd * sizeof(u32)));
        #pragma pack(pop)

        Registers registers{};
        Registers shadowRegisters{}; //!< A shadow-copy of the registers, their function is controlled by the 'shadowRamControl' register

        ChannelContext &channelCtx;

        Maxwell3D(const DeviceState &state, ChannelContext &channelCtx, MacroState &macroState, gpu::interconnect::CommandExecutor &executor);

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
    };
}
