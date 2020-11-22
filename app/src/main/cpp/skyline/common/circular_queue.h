// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

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
        inline CircularQueue(size_t size) : vector((size + 1) * sizeof(Type)) {}

        inline CircularQueue(const CircularQueue &) = delete;

        inline CircularQueue &operator=(const CircularQueue &) = delete;

        inline CircularQueue(CircularQueue &&other) : vector(std::move(other.vector)), consumptionMutex(std::move(other.consumptionMutex)), consumeCondition(std::move(other.consumeCondition)), productionMutex(std::move(other.productionMutex)), produceCondition(std::move(other.produceCondition)) {
            this->start = other.start;
            this->end = other.end;
            other.start = other.end = nullptr;
        }

        inline ~CircularQueue() {
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
         */
        template<typename F>
        [[noreturn]] inline void Process(F function) {
            while (true) {
                if (start == end) {
                    std::unique_lock lock(productionMutex);
                    produceCondition.wait(lock, [this]() { return start != end; });
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

        inline void Push(const Type &item) {
            std::unique_lock lock(productionMutex);
            end = (end == reinterpret_cast<Type *>(vector.end().base()) - 1) ? reinterpret_cast<Type *>(vector.begin().base()) : end;
            if (start == end + 1) {
                std::unique_lock consumeLock(consumptionMutex);
                consumeCondition.wait(consumeLock, [=]() { return start != end + 1; });
            }
            *end = item;
            produceCondition.notify_one();
        }

        inline void Append(span <Type> buffer) {
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
        inline void AppendTranform(span <TransformedType> buffer, Transformation transformation) {
            std::unique_lock lock(productionMutex);
            for (const auto &item : buffer) {
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
