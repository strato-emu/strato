// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <list>
#include <kernel/memory.h>
#include "KThread.h"
#include "KTransferMemory.h"
#include "KSession.h"
#include "KEvent.h"

namespace skyline {
    namespace constant {
        constexpr u16 TlsSlotSize{0x200}; //!< The size of a single TLS slot
        constexpr u8 TlsSlots{PAGE_SIZE / TlsSlotSize}; //!< The amount of TLS slots in a single page
        constexpr KHandle BaseHandleIndex{0xD000}; //!< The index of the base handle
        constexpr u32 MtxOwnerMask{0xBFFFFFFF}; //!< The mask of values which contain the owner of a mutex
    }

    namespace kernel::type {
        /**
         * @brief KProcess manages process-global state such as memory, kernel handles allocated to the process and synchronization primitives
         */
        class KProcess : public KSyncObject {
          public:
            MemoryManager memory; // This is here to ensure it is present during the destruction of dependent objects

          private:
            std::vector<std::shared_ptr<KObject>> handles;
            std::shared_mutex handleMutex;

            struct WaitStatus {
                std::atomic_bool flag{false};
                i8 priority;
                KHandle handle;
                u32* mutex{};

                WaitStatus(i8 priority, KHandle handle);

                WaitStatus(i8 priority, KHandle handle, u32* mutex);
            };

            std::unordered_map<u64, std::vector<std::shared_ptr<WaitStatus>>> mutexes; //!< A map from a mutex's address to a vector of Mutex objects for threads waiting on it
            std::unordered_map<u64, std::list<std::shared_ptr<WaitStatus>>> conditionals; //!< A map from a conditional variable's address to a vector of threads waiting on it
            Mutex mutexLock;
            Mutex conditionalLock;

            /**
            * @brief The status of a single TLS page (A page is 4096 bytes on ARMv8)
            * Each TLS page has 8 slots, each 0x200 (512) bytes in size
            * The first slot of the first page is reserved for user-mode exception handling
            * @url https://switchbrew.org/wiki/Thread_Local_Storage
            */
            struct TlsPage {
                u8 index{}; //!< The slots are assigned sequentially, this holds the index of the last TLS slot reserved
                std::shared_ptr<KPrivateMemory> memory;

                TlsPage(const std::shared_ptr<KPrivateMemory>& memory);

                u8* ReserveSlot();

                u8* Get(u8 index);

                bool Full();
            };

          public:
            std::shared_ptr<KPrivateMemory> mainThreadStack;
            std::shared_ptr<KPrivateMemory> heap;
            std::vector<std::shared_ptr<KThread>> threads;
            std::vector<std::shared_ptr<TlsPage>> tlsPages;

            KProcess(const DeviceState &state);

            /**
             * @note This requires VMM regions to be initialized, it will map heap at an arbitrary location otherwise
             */
            void InitializeHeap();

            /**
             * @return A 0x200 TLS slot allocated inside the TLS/IO region
             */
            u8* AllocateTlsSlot();

            std::shared_ptr<KThread> CreateThread(void *entry, u64 argument = 0, void *stackTop = nullptr, i8 priority = constant::DefaultPriority);

            /**
            * @brief The output for functions that return created kernel objects
            * @tparam objectClass The class of the kernel object
            */
            template<typename objectClass>
            struct HandleOut {
                std::shared_ptr<objectClass> item; //!< A shared pointer to the object
                KHandle handle; //!< The handle of the object in the process
            };

            /**
            * @brief Creates a new handle to a KObject and adds it to the process handle_table
            * @tparam objectClass The class of the kernel object to create
            * @param args The arguments for the kernel object except handle, pid and state
            */
            template<typename objectClass, typename ...objectArgs>
            HandleOut<objectClass> NewHandle(objectArgs... args) {
                std::unique_lock lock(handleMutex);

                std::shared_ptr<objectClass> item;
                if constexpr (std::is_same<objectClass, KThread>())
                    item = std::make_shared<objectClass>(state, constant::BaseHandleIndex + handles.size(), args...);
                else
                    item = std::make_shared<objectClass>(state, args...);
                handles.push_back(std::static_pointer_cast<KObject>(item));
                return {item, static_cast<KHandle>((constant::BaseHandleIndex + handles.size()) - 1)};
            }

            /**
            * @brief Inserts an item into the process handle table
            * @return The handle of the corresponding item in the handle table
            */
            template<typename objectClass>
            KHandle InsertItem(std::shared_ptr<objectClass> &item) {
                std::unique_lock lock(handleMutex);

                handles.push_back(std::static_pointer_cast<KObject>(item));
                return static_cast<KHandle>((constant::BaseHandleIndex + handles.size()) - 1);
            }

            template<typename objectClass = KObject>
            std::shared_ptr<objectClass> GetHandle(KHandle handle) {
                std::shared_lock lock(handleMutex);

                KType objectType;
                if constexpr(std::is_same<objectClass, KThread>())
                    objectType = KType::KThread;
                else if constexpr(std::is_same<objectClass, KProcess>())
                    objectType = KType::KProcess;
                else if constexpr(std::is_same<objectClass, KSharedMemory>())
                    objectType = KType::KSharedMemory;
                else if constexpr(std::is_same<objectClass, KTransferMemory>())
                    objectType = KType::KTransferMemory;
                else if constexpr(std::is_same<objectClass, KPrivateMemory>())
                    objectType = KType::KPrivateMemory;
                else if constexpr(std::is_same<objectClass, KSession>())
                    objectType = KType::KSession;
                else if constexpr(std::is_same<objectClass, KEvent>())
                    objectType = KType::KEvent;
                else
                    throw exception("KProcess::GetHandle couldn't determine object type");
                try {
                    auto& item{handles.at(handle - constant::BaseHandleIndex)};
                    if (item != nullptr && item->objectType == objectType)
                        return std::static_pointer_cast<objectClass>(item);
                    else if (item == nullptr)
                        throw exception("GetHandle was called with a deleted handle: 0x{:X}", handle);
                    else
                        throw exception("Tried to get kernel object (0x{:X}) with different type: {} when object is {}", handle, objectType, item->objectType);
                } catch (std::out_of_range) {
                    throw exception("GetHandle was called with an invalid handle: 0x{:X}", handle);
                }
            }

            template<>
            std::shared_ptr<KObject> GetHandle<KObject>(KHandle handle) {
                return handles.at(handle - constant::BaseHandleIndex);
            }

            /**
            * @brief Retrieves a kernel memory object that owns the specified address
            * @param address The address to look for
            * @return A shared pointer to the corresponding KMemory object
            */
            std::optional<HandleOut<KMemory>> GetMemoryObject(u8* ptr);

            /**
             * @brief Closes a handle in the handle table
             */
            inline void CloseHandle(KHandle handle) {
                handles.at(handle - constant::BaseHandleIndex) = nullptr;
            }

            /**
            * @brief Locks the Mutex at the specified address
            * @param owner The handle of the current mutex owner
            * @return If the mutex was successfully locked
            */
            bool MutexLock(u32* mutex, KHandle owner);

            /**
            * @brief Unlocks the Mutex at the specified address
            * @return If the mutex was successfully unlocked
            */
            bool MutexUnlock(u32* mutex);

            /**
            * @param timeout The amount of time to wait for the conditional variable
            * @return If the conditional variable was successfully waited for or timed out
            */
            bool ConditionalVariableWait(void* conditional, u32* mutex, u64 timeout);

            /**
            * @brief Signals a number of conditional variable waiters
            * @param amount The amount of waiters to signal
            */
            void ConditionalVariableSignal(void* conditional, u64 amount);

            /**
             * @brief Resets the object to an unsignalled state
             */
            inline void ResetSignal() {
                signalled = false;
            }
        };
    }
}
