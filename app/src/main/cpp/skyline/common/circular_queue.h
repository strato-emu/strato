// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common/trace.h>
#include <common.h>

namespace skyline {
    /**
     * @brief An efficient consumer-producer oriented queue with internal synchronization
     */
    template<typename Type>
    class CircularQueue {
      private:
        std::vector<u8> vector; //!< The internal vector holding the circular queue's data, we use a byte vector due to the default item construction/destruction semantics not being appropriate for a circular buffer
        Type *start{reinterpret_cast<Type *>(vector.begin().base())}; //!< The start/oldest element of the queue
        Type *end{reinterpret_cast<Type *>(vector.begin().base())}; //!< The end/newest element of the queue
        std::mutex consumptionMutex;
        std::condition_variable consumeCondition;
        std::mutex productionMutex;
        std::condition_variable produceCondition;

      public:
        /**
         * @note The internal allocation is an item larger as we require a sentinel value
         */
        CircularQueue(size_t size) : vector((size + 1) * sizeof(Type)) {}

        CircularQueue(const CircularQueue &) = delete;

        CircularQueue &operator=(const CircularQueue &) = delete;

        CircularQueue(CircularQueue &&other) : vector(std::move(other.vector)), consumptionMutex(std::move(other.consumptionMutex)), consumeCondition(std::move(other.consumeCondition)), productionMutex(std::move(other.productionMutex)), produceCondition(std::move(other.produceCondition)) {
            start = other.start;
            end = other.end;
            other.start = other.end = nullptr;
        }

        ~CircularQueue() {
            while (start != end) {
                auto next{start + 1};
                next = (next == reinterpret_cast<Type *>(vector.end().base())) ? reinterpret_cast<Type *>(vector.begin().base()) : next;
                std::destroy_at(next);
                start = next;
            }
        }

        /**
         * @brief A blocking for-each that runs on every item and waits till new items to run on them as well
         * @param function A function that is called for each item (with the only parameter as a reference to that item)
         * @param preWait An optional function that's called prior to waiting on more items to be queued
         */
        template<typename F1, typename F2>
        [[noreturn]] void Process(F1 function, F2 preWait) {
            TRACE_EVENT_BEGIN("containers", "CircularQueue::Process");

            while (true) {
                if (start == end) {
                    std::unique_lock lock{consumptionMutex};

                    TRACE_EVENT_END("containers");
                    preWait();
                    produceCondition.wait(lock, [this]() { return start != end; });
                    TRACE_EVENT_BEGIN("containers", "CircularQueue::Process");
                }

                while (start != end) {
                    auto next{start + 1};
                    next = (next == reinterpret_cast<Type *>(vector.end().base())) ? reinterpret_cast<Type *>(vector.begin().base()) : next;
                    function(*next);
                    start = next;
                }

                consumeCondition.notify_one();
            }
        }

        Type Pop() {
            std::unique_lock lock(productionMutex);
            produceCondition.wait(lock, [this]() { return start != end; });

            auto next{start + 1};
            next = (next == reinterpret_cast<Type *>(vector.end().base())) ? reinterpret_cast<Type *>(vector.begin().base()) : next;
            Type item{*next};
            start = next;

            if (start == end)
                consumeCondition.notify_one();

            return item;
        }

        void Push(const Type &item) {
            std::unique_lock lock(productionMutex);
            auto next{end + 1};
            next = (next == reinterpret_cast<Type *>(vector.end().base())) ? reinterpret_cast<Type *>(vector.begin().base()) : next;
            if (next == start) {
                std::unique_lock consumeLock(consumptionMutex);
                consumeCondition.wait(consumeLock, [=]() { return next != start; });
            }
            *next = item;
            end = next;
            produceCondition.notify_one();
        }

        void Append(span <Type> buffer) {
            std::unique_lock lock(productionMutex);
            for (const auto &item : buffer) {
                auto next{end + 1};
                next = (next == reinterpret_cast<Type *>(vector.end().base())) ? reinterpret_cast<Type *>(vector.begin().base()) : next;
                if (next == start) {
                    std::unique_lock consumeLock(consumptionMutex);
                    consumeCondition.wait(consumeLock, [=]() { return next != start; });
                }
                *next = item;
                end = next++;
            }
            produceCondition.notify_one();
        }

        /**
         * @brief Appends a buffer with an alternative input type while supplied transformation function
         * @param tranformation A function that takes in an item of TransformedType as input and returns an item of Type
         */
        template<typename TransformedType, typename Transformation>
        void AppendTranform(TransformedType &container, Transformation transformation) {
            std::unique_lock lock(productionMutex);
            for (auto &item : container) {
                auto next{end + 1};
                next = (next == reinterpret_cast<Type *>(vector.end().base())) ? reinterpret_cast<Type *>(vector.begin().base()) : next;
                if (next == start) {
                    std::unique_lock consumeLock(consumptionMutex);
                    consumeCondition.wait(consumeLock, [=]() { return next != start; });
                }
                *next = transformation(item);
                end = next;
            }
            produceCondition.notify_one();
        }
    };
}
