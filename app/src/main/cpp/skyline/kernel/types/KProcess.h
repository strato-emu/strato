// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <vfs/npdm.h>
#include "KThread.h"
#include "KTransferMemory.h"
#include "KSession.h"
#include "KEvent.h"

namespace skyline {
    namespace constant {
        constexpr u16 TlsSlotSize{0x200}; //!< The size of a single TLS slot
        constexpr u8 TlsSlots{constant::PageSize / TlsSlotSize}; //!< The amount of TLS slots in a single page
        constexpr KHandle BaseHandleIndex{0xD000}; //!< The index of the base handle
    }

    namespace kernel::type {
        /**
         * @brief KProcess manages process-global state such as memory, kernel handles allocated to the process and synchronization primitives
         */
        class KProcess : public KSyncObject {
          public: // We have intermittent public/private members to ensure proper construction/destruction order
            MemoryManager memory;

          private:
            std::mutex threadMutex; //!< Synchronizes thread creation to prevent a race between thread creation and thread killing
            bool disableThreadCreation{}; //!< Whether to disable thread creation, we use this to prevent thread creation after all threads have been killed
            std::atomic_bool alreadyKilled{}; //!< If the process has already been killed prior so there's no need to redundantly kill it again
            std::vector<std::shared_ptr<KThread>> threads;

            using SyncWaiters = std::multimap<void *, std::shared_ptr<KThread>>;
            std::mutex syncWaiterMutex; //!< Synchronizes all mutations to the map to prevent races
            SyncWaiters syncWaiters; //!< All threads waiting on process-wide synchronization primitives (Atomic keys + Address Arbiter)

            /**
            * @brief The status of a single TLS page (A page is 4096 bytes on ARMv8)
            * Each TLS page has 8 slots, each 0x200 (512) bytes in size
            * The first slot of the first page is reserved for user-mode exception handling
            * @url https://switchbrew.org/wiki/Thread_Local_Storage
            */
            struct TlsPage {
                u8 index{}; //!< The slots are assigned sequentially, this holds the index of the last TLS slot reserved
                std::shared_ptr<KPrivateMemory> memory; //!< A single page sized memory allocation for this TLS page

                TlsPage(std::shared_ptr<KPrivateMemory> memory);

                /**
                 * @return A non-null pointer to a TLS page slot on success, a nullptr will be returned if this page is full
                 * @note This function is not thread-safe and should be called exclusively by one thread at a time
                 */
                u8 *ReserveSlot();
            };

          public:
            u8 *tlsExceptionContext{}; //!< A pointer to the TLS exception handling context slot
            std::mutex tlsMutex; //!< A mutex to synchronize allocation of TLS pages to prevent extra pages from being created
            std::vector<std::shared_ptr<TlsPage>> tlsPages; //!< All TLS pages allocated by this process
            std::shared_ptr<KPrivateMemory> mainThreadStack; //!< The stack memory of the main thread stack is owned by the KProcess itself
            std::shared_ptr<KPrivateMemory> heap;
            vfs::NPDM npdm;

          private:
            std::shared_mutex handleMutex;
            std::vector<std::shared_ptr<KObject>> handles;

          public:
            KProcess(const DeviceState &state);

            ~KProcess();

            /**
             * @brief Kill the main thread/all running threads in the process in a graceful manner
             * @param join Return after the main thread has joined rather than instantly
             * @param all Whether to kill all running threads or just the main thread
             * @param disableCreation Whether to disable further thread creation
             * @note If there are no threads then this will silently return
             * @note The main thread should eventually kill the rest of the threads itself
             */
            void Kill(bool join, bool all = false, bool disableCreation = false);

            /**
             * @brief This initializes the process heap and TLS Error Context slot pointer, it should be called prior to creating the first thread
             * @note This requires VMM regions to be initialized, it will map heap at an arbitrary location otherwise
             */
            void InitializeHeapTls();

            /**
             * @return A 0x200 TLS slot allocated inside the TLS/IO region
             */
            u8 *AllocateTlsSlot();

            /**
             * @return A shared pointer to a KThread initialized with the specified values or nullptr, if thread creation has been disabled
             * @note The default values are for the main thread and will use values from the NPDM
             */
            std::shared_ptr<KThread> CreateThread(void *entry, u64 argument = 0, void *stackTop = nullptr, std::optional<i8> priority = std::nullopt, std::optional<u8> idealCore = std::nullopt);

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
                if constexpr (std::is_same<objectClass, KThread>() || std::is_same<objectClass, KPrivateMemory>())
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
                if constexpr(std::is_same<objectClass, KThread>()) {
                    constexpr KHandle threadSelf{0xFFFF8000}; // The handle used by threads to refer to themselves
                    if (handle == threadSelf)
                        return state.thread;
                    objectType = KType::KThread;
                } else if constexpr(std::is_same<objectClass, KProcess>()) {
                    constexpr KHandle processSelf{0xFFFF8001}; // The handle used by threads in a process to refer to the process
                    if (handle == processSelf)
                        return state.process;
                    objectType = KType::KProcess;
                } else if constexpr(std::is_same<objectClass, KSharedMemory>()) {
                    objectType = KType::KSharedMemory;
                } else if constexpr(std::is_same<objectClass, KTransferMemory>()) {
                    objectType = KType::KTransferMemory;
                } else if constexpr(std::is_same<objectClass, KPrivateMemory>()) {
                    objectType = KType::KPrivateMemory;
                } else if constexpr(std::is_same<objectClass, KSession>()) {
                    objectType = KType::KSession;
                } else if constexpr(std::is_same<objectClass, KEvent>()) {
                    objectType = KType::KEvent;
                } else {
                    throw exception("KProcess::GetHandle couldn't determine object type");
                }
                try {
                    auto &item{handles.at(handle - constant::BaseHandleIndex)};
                    if (item != nullptr && item->objectType == objectType)
                        return std::static_pointer_cast<objectClass>(item);
                    else if (item == nullptr)
                        throw exception("GetHandle was called with a deleted handle: 0x{:X}", handle);
                    else
                        throw exception("Tried to get kernel object (0x{:X}) with different type: {} when object is {}", handle, objectType, item->objectType);
                } catch (const std::out_of_range &) {
                    throw std::out_of_range(fmt::format("GetHandle was called with an invalid handle: 0x{:X}", handle));
                }
            }

            template<>
            std::shared_ptr<KObject> GetHandle<KObject>(KHandle handle) {
                std::shared_lock lock(handleMutex);
                auto &item{handles.at(handle - constant::BaseHandleIndex)};
                if (item != nullptr)
                    return item;
                else
                    throw std::out_of_range(fmt::format("GetHandle was called with a deleted handle: 0x{:X}", handle));
            }

            /**
             * @brief Retrieves a kernel memory object that owns the specified address
             * @param address The address to look for
             * @return A shared pointer to the corresponding KMemory object
             */
            std::optional<HandleOut<KMemory>> GetMemoryObject(u8 *ptr);

            /**
             * @brief Closes a handle in the handle table
             */
            void CloseHandle(KHandle handle) {
                handles.at(handle - constant::BaseHandleIndex) = nullptr;
            }

            /**
             * @brief Clear the process handle table
             * @note A handle created prior to clearing must not be retrieved after this is run
             */
            void ClearHandleTable();

            /**
             * @brief Locks the mutex at the specified address
             * @param thread The thread that is locking the mutex
             * @param ownerHandle The psuedo-handle of the current mutex owner
             * @param tag The handle of the thread which is requesting this lock
             * @param failOnOutdated If true, the function will return InvalidCurrentMemory if the supplied ownerHandle is outdated
             */
            Result MutexLock(const std::shared_ptr<KThread> &thread, u32 *mutex, KHandle ownerHandle, KHandle tag, bool failOnOutdated = false);

            /**
             * @brief Unlocks the mutex at the specified address
             */
            void MutexUnlock(u32 *mutex);

            /**
             * @brief Waits on the conditional variable at the specified address
             */
            Result ConditionVariableWait(u32 *key, u32 *mutex, KHandle tag, i64 timeout);

            /**
             * @brief Signals the conditional variable at the specified address
             */
            void ConditionVariableSignal(u32 *key, i32 amount);

            enum class ArbitrationType : u32 {
                WaitIfLessThan = 0,
                DecrementAndWaitIfLessThan = 1,
                WaitIfEqual = 2,
            };

            /**
             * @brief Waits on the supplied address with the specified arbitration type
             */
            Result WaitForAddress(u32 *address, u32 value, i64 timeout, ArbitrationType type);

            enum class SignalType : u32 {
                Signal = 0,
                SignalAndIncrementIfEqual = 1,
                SignalAndModifyBasedOnWaitingThreadCountIfEqual = 2,
            };

            /**
             * @brief Signals a variable for amount of waiters at the supplied address with the specified signal type
             */
            Result SignalToAddress(u32 *address, u32 value, i32 amount, SignalType type);
        };
    }
}
