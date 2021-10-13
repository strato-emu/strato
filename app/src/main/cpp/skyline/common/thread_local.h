// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <pthread.h>

namespace skyline {
    /**
     * @brief A thread-local RAII-bound wrapper class which unlike `thread_local` doesn't require the member to be static
     * @note Caller must ensure any arguments passed into the constructor remain valid throughout its lifetime
     * @note Caller must ensure the destructors of the object doesn't have any thread-local dependencies as they might be called from another thread
     * @note RAII-bound means that *all* thread-local instances of the object will be destroyed after this class is destroyed but can also be destroyed when a thread owning an instance dies
     */
    template<typename Type, bool TrivialDestructor = std::is_trivially_destructible_v<Type>>
    class ThreadLocal;

    template<typename Type>
    class ThreadLocal<Type, true> {
      private:
        pthread_key_t key;
        std::function<Type *()> constructor;

      public:
        template<typename... Args>
        ThreadLocal(Args &&... args) : constructor([args...]() { return new Type(args...); }) {
            int result;
            if ((result = pthread_key_create(&key, nullptr)))
                throw exception("Cannot create pthread_key: {}", strerror(result));
        }

        Type *operator->() {
            auto pointer{pthread_getspecific(key)};
            if (pointer)
                return static_cast<Type *>(pointer);

            int result;
            Type *object{constructor(*this)};
            if ((result = pthread_setspecific(key, object)))
                throw exception("Cannot set pthread_key to constructed type: {}", strerror(result));

            return object;
        }

        Type &operator*() {
            return *operator->();
        }

        ~ThreadLocal() {
            pthread_key_delete(key);
        }
    };

    template<typename Type>
    class ThreadLocal<Type, false> {
      private:
        struct IntrustiveTypeNode {
            Type object;
            ThreadLocal &threadLocal;
            IntrustiveTypeNode *next{};

            template<typename... Args>
            IntrustiveTypeNode(ThreadLocal &threadLocal, Args &&... args) : object(std::forward<Args>(args)...), threadLocal(threadLocal) {}

            ~IntrustiveTypeNode() {
                auto current{threadLocal.list.load(std::memory_order_acquire)};
                while (current == this)
                    if (threadLocal.list.compare_exchange_strong(current, next, std::memory_order_release, std::memory_order_consume))
                        return;

                while (current) {
                    if (current->next == this) {
                        current->next = next;
                        return;
                    }
                    current = current->next;
                }
            }
        };

        pthread_key_t key;
        std::function<IntrustiveTypeNode *(ThreadLocal &)> constructor;
        std::atomic<IntrustiveTypeNode *> list{}; //!< An atomic instrusive linked list of all instances of the object to call non-trivial destructors for the objects

      public:
        template<typename... Args>
        ThreadLocal(Args &&... args) : constructor([args...](ThreadLocal &threadLocal) { return new IntrustiveTypeNode(threadLocal, args...); }) {
            auto destructor{[](void *object) {
                static_cast<IntrustiveTypeNode *>(object)->~IntrustiveTypeNode();
            }};

            int result;
            if ((result = pthread_key_create(&key, destructor)))
                throw exception("Cannot create pthread_key: {}", strerror(result));
        }

        Type *operator->() {
            auto pointer{pthread_getspecific(key)};
            if (pointer)
                return &static_cast<IntrustiveTypeNode *>(pointer)->object;

            int result;
            IntrustiveTypeNode *node{constructor(*this)};
            if ((result = pthread_setspecific(key, node)))
                throw exception("Cannot set pthread_key to constructed type: {}", strerror(result));

            auto next{list.load(std::memory_order_acquire)};
            do {
                node->next = next;
            } while (!list.compare_exchange_strong(next, node, std::memory_order_release, std::memory_order_consume));

            return &node->object;
        }

        Type &operator*() {
            return *operator->();
        }

        ~ThreadLocal() {
            auto current{list.exchange(nullptr, std::memory_order_acquire)};
            while (current) {
                current->object.~Type();
                current = current->next;
            }

            pthread_key_delete(key);
        }
    };
}
