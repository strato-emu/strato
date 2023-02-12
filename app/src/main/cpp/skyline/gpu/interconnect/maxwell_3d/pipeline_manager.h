// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

// #define PIPELINE_STATS //!< Enables recording and ranking of pipelines by the number of variants-per-shader set

#pragma once

#include <tsl/robin_map.h>
#include <shader_compiler/frontend/ir/program.h>
#include <gpu/graphics_pipeline_assembler.h>
#include <gpu/interconnect/common/samplers.h>
#include <gpu/interconnect/common/textures.h>
#include <gpu/interconnect/common/pipeline_state_accessor.h>
#include "common.h"
#include "packed_pipeline_state.h"
#include "constant_buffers.h"

namespace skyline::gpu {
    class TextureView;
}

namespace skyline::gpu::interconnect::maxwell3d {
    class Pipeline {
      public:
        /**
         * @brief A monolithic struct containing all the descriptor state of the pipeline
         */
        struct DescriptorInfo {
            std::vector<vk::DescriptorSetLayoutBinding> descriptorSetLayoutBindings;

            struct StageDescriptorInfo {
                vk::PipelineStageFlagBits stage;

                // Unwrapped counts (counting each array element as a separate descriptor) for the below desc structs
                u16 uniformBufferDescTotalCount;
                u16 storageBufferDescTotalCount;
                u16 uniformTexelBufferDescTotalCount;
                u16 storageTexelBufferDescTotalCount;
                u16 combinedImageSamplerDescTotalCount;
                u16 storageImageDescTotalCount;

                // Below are descriptor structs designed to be compatible with hades (hence the use of snake_case) but in a more compacted format to reduce memory usage
                struct UniformBufferDesc {
                    u8 index;
                    u8 count;

                    UniformBufferDesc(const Shader::ConstantBufferDescriptor &desc)
                        : index{static_cast<u8>(desc.index)},
                          count{static_cast<u8>(desc.count)} {}

                    auto operator<=>(const UniformBufferDesc &) const = default;
                };
                boost::container::static_vector<UniformBufferDesc, engine::ShaderStageConstantBufferCount> uniformBufferDescs;

                struct StorageBufferDesc {
                    u32 cbuf_offset;
                    u8 cbuf_index;
                    bool is_written;
                    u8 count;

                    StorageBufferDesc(const Shader::StorageBufferDescriptor &desc)
                        : cbuf_offset{desc.cbuf_offset},
                          cbuf_index{static_cast<u8>(desc.cbuf_index)},
                          is_written{desc.is_written},
                          count{static_cast<u8>(desc.count)} {}

                    auto operator<=>(const StorageBufferDesc &) const = default;
                };
                boost::container::small_vector<StorageBufferDesc, 8> storageBufferDescs;

                struct UniformTexelBufferDesc {
                    u32 cbuf_offset;
                    u32 secondary_cbuf_offset;
                    bool has_secondary;
                    u8 cbuf_index;
                    u8 shift_left;
                    u8 secondary_cbuf_index;
                    u8 secondary_shift_left;
                    u8 count;
                    u8 size_shift;

                    UniformTexelBufferDesc(const Shader::TextureBufferDescriptor &desc)
                        : cbuf_offset{desc.cbuf_offset},
                          secondary_cbuf_offset{desc.secondary_cbuf_offset},
                          has_secondary{desc.has_secondary},
                          cbuf_index{static_cast<u8>(desc.cbuf_index)},
                          shift_left{static_cast<u8>(desc.shift_left)},
                          secondary_cbuf_index{static_cast<u8>(desc.secondary_cbuf_index)},
                          secondary_shift_left{static_cast<u8>(desc.secondary_shift_left)},
                          count{static_cast<u8>(desc.count)},
                          size_shift{static_cast<u8>(desc.size_shift)} {}

                    auto operator<=>(const UniformTexelBufferDesc &) const = default;
                };
                std::vector<UniformTexelBufferDesc> uniformTexelBufferDescs;

                struct StorageTexelBufferDesc {
                    Shader::ImageFormat format;
                    u32 cbuf_offset;
                    bool is_read;
                    bool is_written;
                    u8 cbuf_index;
                    u8 count;
                    u8 size_shift;

                    StorageTexelBufferDesc(const Shader::ImageBufferDescriptor &desc)
                        : format{desc.format},
                          cbuf_offset{desc.cbuf_offset},
                          is_read{desc.is_read},
                          is_written{desc.is_written},
                          cbuf_index{static_cast<u8>(desc.cbuf_index)},
                          count{static_cast<u8>(desc.count)},
                          size_shift{static_cast<u8>(desc.size_shift)} {}

                    auto operator<=>(const StorageTexelBufferDesc &) const = default;
                };
                std::vector<StorageTexelBufferDesc> storageTexelBufferDescs;

                struct CombinedImageSamplerDesc {
                    Shader::TextureType type;
                    u32 cbuf_offset;
                    u32 secondary_cbuf_offset;
                    bool has_secondary;
                    u8 cbuf_index;
                    u8 shift_left;
                    u8 secondary_cbuf_index;
                    u8 secondary_shift_left;
                    u8 count;
                    u8 size_shift;

                    CombinedImageSamplerDesc(const Shader::TextureDescriptor &desc)
                        : type{desc.type},
                          cbuf_offset{desc.cbuf_offset},
                          secondary_cbuf_offset{desc.secondary_cbuf_offset},
                          has_secondary{desc.has_secondary},
                          cbuf_index{static_cast<u8>(desc.cbuf_index)},
                          shift_left{static_cast<u8>(desc.shift_left)},
                          secondary_cbuf_index{static_cast<u8>(desc.secondary_cbuf_index)},
                          secondary_shift_left{static_cast<u8>(desc.secondary_shift_left)},
                          count{static_cast<u8>(desc.count)},
                          size_shift{static_cast<u8>(desc.size_shift)} {}

                    auto operator<=>(const CombinedImageSamplerDesc &) const = default;
                };
                boost::container::small_vector<CombinedImageSamplerDesc, 10> combinedImageSamplerDescs;

                struct StorageImageDesc {
                    Shader::TextureType type;
                    Shader::ImageFormat format;
                    u32 cbuf_offset;
                    bool isRead;
                    bool is_written;
                    u8 cbuf_index;
                    u8 count;
                    u8 size_shift;

                    StorageImageDesc(const Shader::ImageDescriptor &desc)
                        : type{desc.type},
                          format{desc.format},
                          cbuf_offset{desc.cbuf_offset},
                          isRead{desc.is_read},
                          is_written{desc.is_written},
                          cbuf_index{static_cast<u8>(desc.cbuf_index)},
                          count{static_cast<u8>(desc.count)},
                          size_shift{static_cast<u8>(desc.size_shift)} {}

                    auto operator<=>(const StorageImageDesc &) const = default;
                };
                boost::container::small_vector<StorageImageDesc, 1> storageImageDescs;

                std::array<u32, Shader::Info::MAX_CBUFS> constantBufferUsedSizes;

                /**
                 * @brief Keeps track of all bindings that are dependent on a given constant buffer index to allow for quick binding
                 */
                struct ConstantBufferDescriptorUsages {
                    struct Usage {
                        u16 binding; //!< Vulkan binding index
                        u16 shaderDescIdx; //!< Index of the descriptor in the appropriate shader info member
                        u16 entirePipelineIdx; //!< Index of the image/storage buffer in the entire pipeline

                        bool operator==(const Usage&) const = default;
                    };

                    boost::container::small_vector<Usage, 2> uniformBuffers;
                    boost::container::small_vector<Usage, 2> storageBuffers;
                    boost::container::small_vector<Usage, 2> combinedImageSamplers;
                    u16 totalBufferDescCount;
                    u16 totalImageDescCount;
                    u16 writeDescCount;

                    bool operator==(const ConstantBufferDescriptorUsages&) const = default;
                };

                std::array<ConstantBufferDescriptorUsages, engine::ShaderStageConstantBufferCount> cbufUsages;

                bool operator==(const StageDescriptorInfo&) const = default;
            };

            std::vector<vk::CopyDescriptorSet> copyDescs; //!< Copy descriptors for all descs in the pipeline to allow for quick binding
            std::array<StageDescriptorInfo, engine::ShaderStageCount> stages;

            u16 totalStorageBufferCount;
            u16 totalCombinedImageSamplerCount;

            u16 totalWriteDescCount;
            u16 totalBufferDescCount;
            u16 totalTexelBufferDescCount;
            u16 totalImageDescCount;

            bool operator==(const DescriptorInfo &) const = default;
        };

        PackedPipelineState sourcePackedState;

      private:
        std::vector<CachedMappedBufferView> storageBufferViews;
        ContextTag lastExecutionTag{}; //!< The last execution tag this pipeline was used at
        DescriptorInfo descriptorInfo; //!< Info about all descriptors used in each stage of the pipeline
        u8 transitionCacheNextIdx{}; //!< The next index to insert into the transition cache
        u8 stageMask{}; //!< Bitmask of active shader stages
        u16 sampledImageCount{};

        std::array<Pipeline *, 6> transitionCache{};

        tsl::robin_map<Pipeline *, bool> bindingMatchCache; //!< Cache of which pipelines have bindings that match this pipeline

        void SyncCachedStorageBufferViews(ContextTag executionTag);

      public:
        GraphicsPipelineAssembler::CompiledPipeline compiledPipeline;

        Pipeline(GPU &gpu, PipelineStateAccessor &accessor, const PackedPipelineState &packedState);

        /**
         * @brief Returns the pipeline in the transition cache (if present) that matches the given state
         */
        Pipeline *LookupNext(const PackedPipelineState &packedState);

        /**
         * @brief Record a transition from this pipeline to the next pipeline in the transition cache
         */
        void AddTransition(Pipeline *next);

        bool CheckBindingMatch(Pipeline *other);

        u32 GetTotalSampledImageCount() const;

        /**
         * @brief Creates a descriptor set update from the current GPU state
         * @param sampledImages A span of size `GetTotalSampledImageCount()` in which texture view pointers for each sampled image will be written
         */
        DescriptorUpdateInfo *SyncDescriptors(InterconnectContext &ctx, ConstantBufferSet &constantBuffers, Samplers &samplers, Textures &textures, span<TextureView *> sampledImages, vk::PipelineStageFlags &srcStageMask, vk::PipelineStageFlags &dstStageMask);

        /**
         * @brief Creates a partial descriptor set update from the current GPU state for only the subset of descriptors changed by the quick bind constant buffer
         * @param sampledImages A span of size `GetTotalSampledImageCount()` in which texture view pointers for each sampled image will be written
         */
        DescriptorUpdateInfo *SyncDescriptorsQuickBind(InterconnectContext &ctx, ConstantBufferSet &constantBuffers, Samplers &samplers, Textures &textures, ConstantBuffers::QuickBind quickBind, span<TextureView *> sampledImages, vk::PipelineStageFlags &srcStageMask, vk::PipelineStageFlags &dstStageMask);
    };

    /**
     * @brief Manages the caching and creation of pipelines
     */
    class PipelineManager {
      private:
        tsl::robin_map<PackedPipelineState, std::unique_ptr<Pipeline>, PackedPipelineStateHash> map;

        #ifdef PIPELINE_STATS
        std::unordered_map<std::array<u64, engine::PipelineCount>, std::list<Pipeline*>, util::ObjectHash<std::array<u64, engine::PipelineCount>>> sharedPipelines; //!< Maps a shader set to all pipelines sharing that same set
        std::vector<std::list<Pipeline*>*> sortedSharedPipelines; //!< Sorted list of shared pipelines
        #endif

      public:
        PipelineManager(GPU &gpu);

        Pipeline *FindOrCreate(InterconnectContext &ctx, Textures &textures, ConstantBufferSet &constantBuffers, const PackedPipelineState &packedState, const std::array<ShaderBinary, engine::PipelineCount> &shaderBinaries);
    };
}
