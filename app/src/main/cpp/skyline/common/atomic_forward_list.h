// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <atomic>

namespace skyline {
    /**
     * @brief A singly-linked list with atomic access to allow for lock-free access semantics
     */
    template<typename Type>
    class AtomicForwardList {
      private:
        struct Node {
            Node *next;
            Type value;
        };

        std::atomic<Node *> head{}; //!< The head of the list

      public:
        AtomicForwardList() = default;

        AtomicForwardList(const AtomicForwardList &) = delete;

        AtomicForwardList(AtomicForwardList &&other) {
            head = other.head.load();
            while (!other.head.compare_exchange_strong(head, nullptr, std::memory_order_release, std::memory_order_consume));
        }

        ~AtomicForwardList() {
            Clear();
        }

        /**
         * @brief Clears all the items from the list while deallocating them
         */
        void Clear() {
            auto current{head.exchange(nullptr, std::memory_order_acquire)};
            while (current) {
                auto next{current->next};
                delete current;
                current = next;
            }
        }

        /**
         * @brief Appends an item to the start of the list
         */
        void Append(Type item) {
            auto node{new Node{nullptr, item}};
            auto next{head.load(std::memory_order_consume)};
            do {
                node->next = next;
            } while (!head.compare_exchange_strong(next, node, std::memory_order_release, std::memory_order_consume));
        }

        /**
         * @brief Appends multiple items to the start of the list
         */
        void Append(std::initializer_list<Type> items) {
            if (std::empty(items))
                return;

            Node* firstNode{new Node{nullptr, *items.begin()}};
            Node* lastNode{firstNode};
            for (auto item{items.begin() + 1}; item != items.end(); item++)
                lastNode = new Node{lastNode, *item};

            auto next{head.load(std::memory_order_consume)};
            do {
                firstNode->next = next;
            } while (!head.compare_exchange_strong(next, lastNode, std::memory_order_release, std::memory_order_consume));
        }

        template<typename... Items>
        void Append(Items &&... items) {
            Append(std::initializer_list<Type>{std::forward<Items>(items)...});
        }

        /**
         * @brief Iterates over every single list item and calls the given function
         * @note This function is **not** thread-safe when used with Clear() as the item may be deallocated while iterating
         */
        template <typename Function>
        void Iterate(Function function) {
            auto current{head.load(std::memory_order_consume)};
            while (current) {
                function(current->value);
                current = current->next;
            }
        }

        /**
         * @brief Iterates over every single list item and calls the given function till the given predicate returns false
         * @note This function is **not** thread-safe when used with Clear() as the item may be deallocated while iterating
         */
        template <typename Function>
        bool AllOf(Function function) {
            auto current{head.load(std::memory_order_consume)};
            while (current) {
                if (!function(current->value))
                    return false;
                current = current->next;
            }
            return true;
        }
    };
}
