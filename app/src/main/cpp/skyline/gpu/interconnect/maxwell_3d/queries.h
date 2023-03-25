// SPDX-License-Identifier: MPL-2.0
// Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <limits>
#include <unordered_set>
#include <soc/gm20b/gmmu.h>
#include "common.h"
#include "gpu/buffer.h"
#include "gpu/interconnect/common/common.h"

namespace skyline::gpu::interconnect::maxwell3d {
    /**
     * @brief Handles using host Vulkan queries
     */
    class Queries {
      public:
        enum class CounterType : u32 {
            Occulusion = 0,
            MaxValue
        };

      private:
        /**
         * @brief Represents a single query counter type
         */
        class Counter {
          private:
            static constexpr size_t QueryPoolSize{0x1000}; //!< Size of the underlying VK query pool to use

            /**
             * @brief Information required to report a single query with an optional timestamp
             */
            struct Query {
                BufferView view; //!< View to write the query result to
                BufferBinding timestampBinding; //!< Binding to buffer containing timestamp to write out (optional)
            };

            vk::raii::QueryPool pool;

            ContextTag lastTag{}; //!< Execution tag at the last time a query was began
            u32 lastRenderPassIndex{}; //!< Renderpass index at the last time a query was began
            bool recordOnNextEnd{}; //!< If to record the query copying code upon ending the next query

            // A note on the below variables: In Vulkan you can begin/end queries in an RP but you can't copy the results. Since some games perform hundreds of queries in a row it's not ideal to have constantly end the RP. To work around this, queries are performed on a per-RP basis, with a reset of query 0->queryCount before the RP begins, and all the copies after the RP ends. Since per-RP storage is needed for this the below variables are linearly allocated and replaced upon new queries happening in a new RP.
            span<Query> queries{}; //!< A list of queries reports to perform at the end of the current RP, linearly allocated
            u32 *usedQueryCount{}; //!< Number of queries used from the pool in the current RP, linearly allocated
            bool *queryActive{}; //!< If a query is active in the current RP, this is used so that the RP end code knows whether it needs to end the final query

            std::function<void(vk::raii::CommandBuffer &, const std::shared_ptr<FenceCycle> &, GPU &)> Prepare(InterconnectContext &ctx);

          public:
            Counter(vk::raii::Device &device, vk::QueryType type);

            /**
             * @brief Begins a query in the command stream
             * @param atExecutionStart Whether to insert the query begin at the start of the current executor or at the current position
             */
            void Begin(InterconnectContext &ctx, bool atExecutionStart = false);

            /**
             * @brief Records a query end, and a copy into the target buffer in the command stream
             * @param view View to copy the query result into
             * @param timestamp Optional timestamp to report along with the query
             */
            void Report(InterconnectContext &ctx, BufferView view, std::optional<u64> timestamp);

            /**
             * @brief Records a query end
             */
            void End(InterconnectContext &ctx);

        };

        std::array<Counter, static_cast<u32>(CounterType::MaxValue)> counters;

        CachedMappedBufferView view{}; //!< Cached view for looking up query buffers from IOVAs

        std::unordered_set<u64> usedQueryAddresses;

      public:
        Queries(GPU &gpu);

        /**
         * @brief Records a query of the counter corresponding to `type` and writes the result to the supplied address
         */
        void Query(InterconnectContext &ctx, soc::gm20b::IOVA address, CounterType type, std::optional<u64> timestamp);

        /**
         * @brief Resets the counter value for `type` to the default
         */
        void ResetCounter(InterconnectContext &ctx, CounterType type);

        void PurgeCaches(InterconnectContext &ctx);

        /**
         * @return If a query has ever been reported to `address`
         */
        bool QueryPresentAtAddress(soc::gm20b::IOVA address);
    };
}
