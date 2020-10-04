// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <list>
#include "KThread.h"
#include "KPrivateMemory.h"
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
         * @brief The KProcess class is responsible for holding the state of a process
         */
        class KProcess : public KSyncObject {
          private:
            KHandle handleIndex{constant::BaseHandleIndex}; //!< The index of the handle that will be allocated next

            /**
            * @brief The status of a single TLS page (A page is 4096 bytes on ARMv8)
            * Each TLS page has 8 slots, each 0x200 (512) bytes in size
            * The first slot of the first page is reserved for user-mode exception handling
            * @url https://switchbrew.org/wiki/Thread_Local_Storage
            */
            struct TlsPage {
                u8* ptr;
                u8 index{}; //!< The slots are assigned sequentially, this holds the index of the last TLS slot reserved
                bool slot[constant::TlsSlots]{}; //!< An array of booleans denoting which TLS slots are reserved

                TlsPage(u8* ptr);

                /**
                * @brief Reserves a single 0x200 byte TLS slot
                * @return The address of the reserved slot
                */
                u8* ReserveSlot();

                /**
                * @brief Returns the address of a particular slot
                * @param slotNo The number of the slot to be returned
                * @return The address of the specified slot
                */
                u8* Get(u8 slotNo);

                /**
                * @brief Returns boolean on if the TLS page has free slots or not
                * @return If the whole page is full or not
                */
                bool Full();
            };

            /**
             * @return The address of a free TLS slot
             */
            u8* GetTlsSlot();

            /**
             * @brief Initializes heap and the initial TLS page
             */
            void InitializeMemory();

          public:
            friend OS;

            /**
            * @brief The output for functions that return created kernel objects
            * @tparam objectClass The class of the kernel object
            */
            template<typename objectClass>
            struct HandleOut {
                std::shared_ptr<objectClass> item; //!< A shared pointer to the object
                KHandle handle; //!< The handle of the object in the process
            };

            enum class Status {
                Created, //!< The process was created but the main thread has not started yet
                Started, //!< The process has been started
                Exiting, //!< The process is exiting
            } status = Status::Created;

            /**
             * @brief Metadata on a thread waiting for mutexes or conditional variables
             */
            struct WaitStatus {
                std::atomic_bool flag{false}; //!< The underlying atomic flag of the thread
                u8 priority; //!< The priority of the thread
                KHandle handle; //!< The handle of the thread
                u64 mutexAddress{}; //!< The address of the mutex

                WaitStatus(u8 priority, KHandle handle) : priority(priority), handle(handle) {}

                WaitStatus(u8 priority, KHandle handle, u64 mutexAddress) : priority(priority), handle(handle), mutexAddress(mutexAddress) {}
            };

            pid_t pid; //!< The PID of the process or TGID of the threads
            int memFd; //!< The file descriptor to the memory of the process
            std::vector<std::shared_ptr<KObject>> handles; //!< A vector of KObject which corresponds to the handle
            std::unordered_map<pid_t, std::shared_ptr<KThread>> threads; //!< A mapping from a PID to it's corresponding KThread object
            std::unordered_map<u64, std::vector<std::shared_ptr<WaitStatus>>> mutexes; //!< A map from a mutex's address to a vector of Mutex objects for threads waiting on it
            std::unordered_map<u64, std::list<std::shared_ptr<WaitStatus>>> conditionals; //!< A map from a conditional variable's address to a vector of threads waiting on it
            std::vector<std::shared_ptr<TlsPage>> tlsPages; //!< A vector of all allocated TLS pages
            std::shared_ptr<type::KSharedMemory> stack; //!< The shared memory used to hold the stack of the main thread
            std::shared_ptr<KPrivateMemory> heap; //!< The kernel memory object backing the allocated heap
            Mutex mutexLock; //!< Synchronizes all concurrent guest mutex operations
            Mutex conditionalLock; //!< Synchronizes all concurrent guest conditional variable operations

            /**
             * @param pid The PID of the main thread
             * @param entryPoint The entry point of execution for the guest
             * @param tlsMemory The KSharedMemory object for TLS memory allocated by the guest process
             */
            KProcess(const DeviceState &state, pid_t pid, u64 entryPoint, std::shared_ptr<type::KSharedMemory> &stack, std::shared_ptr<type::KSharedMemory> &tlsMemory);

            /**
             * Close the file descriptor to the process's memory
             */
            ~KProcess();

            /**
            * @brief Create a thread in this process
            * @param entryPoint The address of the initial function
            * @param entryArg An argument to the function
            * @param stackTop The top of the stack
            * @param priority The priority of the thread
            * @return An instance of KThread class for the corresponding thread
            */
            std::shared_ptr<KThread> CreateThread(u64 entryPoint, u64 entryArg, u64 stackTop, i8 priority);

            /**
            * @tparam Type The type of the pointer to return
            * @param address The address on the guest
            * @return A pointer corresponding to a certain address on the guest
            * @note If the address is invalid then nullptr will be returned
            */
            template<typename Type>
            inline Type *GetPointer(u64 address) {
                return reinterpret_cast<Type *>(address);
            }

            /**
            * @brief Writes an object to guest memory
            * @tparam Type The type of the object to be written
            * @param item The object to write
            * @param address The address of the object
            */
            template<typename Type>
            inline void WriteMemory(Type &item, u64 address) {
                auto destination{GetPointer<Type>(address)};
                if (destination) {
                    *destination = item;
                } else {
                    WriteMemory(&item, address, sizeof(Type));
                }
            }

            /**
            * @brief Writes an object to guest memory
            * @tparam Type The type of the object to be written
            * @param item The object to write
            * @param address The address of the object
            */
            template<typename Type>
            inline void WriteMemory(const Type &item, u64 address) {
                auto destination{GetPointer<Type>(address)};
                if (destination) {
                    *destination = item;
                } else {
                    WriteMemory(&item, address, sizeof(Type));
                }
            }

            /**
            * @brief Read data from the guest's memory
            * @param destination The address to the location where the process memory is written
            * @param offset The address to read from in process memory
            * @param size The amount of memory to be read
            * @param forceGuest Forces the write to be performed in guest address space
            */
            void ReadMemory(void *destination, u64 offset, size_t size, bool forceGuest = false);

            /**
            * @brief Write to the guest's memory
            * @param source The address of where the data to be written is present
            * @param offset The address to write to in process memory
            * @param size The amount of memory to be written
            * @param forceGuest Forces the write to be performed in guest address space
            */
            void WriteMemory(const void *source, u64 offset, size_t size, bool forceGuest = false);

            /**
            * @brief Creates a new handle to a KObject and adds it to the process handle_table
            * @tparam objectClass The class of the kernel object to create
            * @param args The arguments for the kernel object except handle, pid and state
            * @return A shared pointer to the corresponding object
            */
            template<typename objectClass, typename ...objectArgs>
            HandleOut<objectClass> NewHandle(objectArgs... args) {
                std::shared_ptr<objectClass> item;
                if constexpr (std::is_same<objectClass, KThread>())
                    item = std::make_shared<objectClass>(state, handleIndex, args...);
                else
                    item = std::make_shared<objectClass>(state, args...);
                handles.push_back(std::static_pointer_cast<KObject>(item));
                return {item, handleIndex++};
            }

            /**
            * @brief Inserts an item into the process handle table
            * @param item The item to insert
            * @return The handle of the corresponding item in the handle table
            */
            template<typename objectClass>
            KHandle InsertItem(std::shared_ptr<objectClass> &item) {
                handles.push_back(std::static_pointer_cast<KObject>(item));
                return handleIndex++;
            }

            /**
            * @brief Returns the underlying kernel object for a handle
            * @tparam objectClass The class of the kernel object present in the handle
            * @param handle The handle of the object
            * @return A shared pointer to the object
            */
            template<typename objectClass>
            std::shared_ptr<objectClass> GetHandle(KHandle handle) {
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
            * @param address The address of the mutex
            * @param owner The handle of the current mutex owner
            * @return If the mutex was successfully locked
            */
            bool MutexLock(u64 address, KHandle owner);

            /**
            * @brief Unlocks the Mutex at the specified address
            * @param address The address of the mutex
            * @return If the mutex was successfully unlocked
            */
            bool MutexUnlock(u64 address);

            /**
            * @param conditionalAddress The address of the conditional variable
            * @param mutexAddress The address of the mutex
            * @param timeout The amount of time to wait for the conditional variable
            * @return If the conditional variable was successfully waited for or timed out
            */
            bool ConditionalVariableWait(u64 conditionalAddress, u64 mutexAddress, u64 timeout);

            /**
            * @brief Signals a number of conditional variable waiters
            * @param address The address of the conditional variable
            * @param amount The amount of waiters to signal
            */
            void ConditionalVariableSignal(u64 address, u64 amount);

            /**
             * @brief Resets the object to an unsignalled state
             */
            inline void ResetSignal() {
                signalled = false;
            }
        };
    }
}
