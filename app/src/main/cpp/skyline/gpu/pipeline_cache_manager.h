// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <queue>
#include <common.h>
#include "interconnect/common/pipeline_state_bundle.h"

namespace skyline::gpu {
    /**
     * @brief Manages access and validation of the underlying pipeline cache files
     */
    class PipelineCacheManager {
      private:
        std::thread writerThread;
        std::queue<std::unique_ptr<interconnect::PipelineStateBundle>> writeQueue; //!< The queue of pipeline state bundles to be written to the cache
        std::mutex writeMutex; //!< Protects access to the write queue
        std::condition_variable writeCondition; //!< Notifies the writer thread when the write queue is not empty
        std::string stagingPath; //!< The path to the staging pipeline cache file, which will be actively written to at runtime
        std::string mainPath; //!< The path to the main pipeline cache file

        void Run();

        bool ValidateHeader(std::ifstream &stream);

        void MergeStaging();

      public:
        PipelineCacheManager(const DeviceState &state, const std::string &path);

        /**
         * @brief Queues a pipeline state bundle to be written to the cache
         */
        void QueueWrite(std::unique_ptr<interconnect::PipelineStateBundle> bundle);

        std::ifstream OpenReadStream();

        /**
         * @brief Shrinks the pipeline cache file to `offset` bytes, removing any (potentially invalid) data after that point
         */
        void InvalidateAllAfter(u64 offset);
    };
}
