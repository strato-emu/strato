#pragma once

#include "KThread.h"
#include "KPrivateMemory.h"
#include "KSharedMemory.h"
#include "KSession.h"

namespace skyline::kernel::type {
    /**
     * @brief The KProcess class is responsible for holding the state of a process
     */
    class KProcess : public KSyncObject {
      private:
        /**
         * @brief This class holds a single TLS page's status
         * @details tls_page_t holds the status of a single TLS page (A page is 4096 bytes on ARMv8).
         * Each TLS page has 8 slots, each 0x200 (512) bytes in size.
         * The first slot of the first page is reserved for user-mode exception handling.
         * Read more about TLS here: https://switchbrew.org/wiki/Thread_Local_Storage
         */
        struct TlsPage {
            u64 address; //!< The address of the page allocated for TLS
            u8 index = 0; //!< The slots are assigned sequentially, this holds the index of the last TLS slot reserved
            bool slot[constant::TlsSlots]{0}; //!< An array of booleans denoting which TLS slots are reserved

            /**
             * @param address The address of the allocated page
             */
            TlsPage(u64 address);

            /**
             * @brief Reserves a single 0x200 byte TLS slot
             * @return The address of the reserved slot
             */
            u64 ReserveSlot();

            /**
             * @brief Returns the address of a particular slot
             * @param slotNo The number of the slot to be returned
             * @return The address of the specified slot
             */
            u64 Get(u8 slotNo);

            /**
             * @brief Returns boolean on if the TLS page has free slots or not
             * @return If the whole page is full or not
             */
            bool Full();
        };

        /**
         * @brief Returns a TLS slot from an arbitrary TLS page
         * @return The address of a free TLS slot
         */
        u64 GetTlsSlot();

        int memFd; //!< The file descriptor to the memory of the process

      public:
        enum class ProcessStatus { Created, CreatedAttached, Started, Crashed, StartedAttached, Exiting, Exited, DebugSuspended } status = ProcessStatus::Created; //!< The state of the process
        handle_t handleIndex = constant::BaseHandleIndex; //!< This is used to keep track of what to map as an handle
        pid_t mainThread; //!< The PID of the main thread
        size_t mainThreadStackSz; //!< The size of the main thread's stack (All other threads map stack themselves so we don't know the size per-se)
        std::map<u64, std::shared_ptr<KPrivateMemory>> memoryMap; //!< A mapping from every address to a shared pointer of it's corresponding KPrivateMemory, used to keep track of KPrivateMemory instances
        std::map<memory::Region, std::shared_ptr<KPrivateMemory>> memoryRegionMap; //!< A mapping from every MemoryRegion to a shared pointer of it's corresponding KPrivateMemory
        std::map<handle_t, std::shared_ptr<KObject>> handleTable; //!< A mapping from a handle_t to it's corresponding KObject which is the actual underlying object
        std::map<pid_t, std::shared_ptr<KThread>> threadMap; //!< A mapping from a PID to it's corresponding KThread object
        std::vector<std::shared_ptr<TlsPage>> tlsPages; //!< A vector of all allocated TLS pages

        /**
         * This is used as the output for functions that return created kernel objects
         * @tparam objectClass The class of the kernel object
         */
        template<typename objectClass>
        struct HandleOut {
            std::shared_ptr<objectClass> item;
            handle_t handle;
        };

        /**
         * @brief Creates a KThread object for the main thread and opens the process's memory file
         * @param state The state of the device
         * @param pid The PID of the main thread
         * @param entryPoint The address to start execution at
         * @param stackBase The base of the stack
         * @param stackSize The size of the stack
         */
        KProcess(const DeviceState &state, pid_t pid, u64 entryPoint, u64 stackBase, u64 stackSize);

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
        std::shared_ptr<KThread> CreateThread(u64 entryPoint, u64 entryArg, u64 stackTop, u8 priority);

        /**
         * @brief Returns an object from process memory
         * @tparam Type The type of the object to be read
         * @param address The address of the object
         * @return An object of type T with read data
         */
        template<typename Type>
        Type ReadMemory(u64 address) const {
            Type item{};
            ReadMemory(&item, address, sizeof(Type));
            return item;
        }

        /**
         * @brief Writes an object to process memory
         * @tparam Type The type of the object to be written
         * @param item The object to write
         * @param address The address of the object
         */
        template<typename Type>
        void WriteMemory(Type &item, u64 address) const {
            WriteMemory(&item, address, sizeof(Type));
        }

        /**
         * @brief Read data from the process's memory
         * @param destination The address to the location where the process memory is written
         * @param offset The address to read from in process memory
         * @param size The amount of memory to be read
         */
        void ReadMemory(void *destination, u64 offset, size_t size) const;

        /**
         * @brief Write to the process's memory
         * @param source The address of where the data to be written is present
         * @param offset The address to write to in process memory
         * @param size The amount of memory to be written
         */
        void WriteMemory(void *source, u64 offset, size_t size) const;

        /**
         * @brief Returns the FD of the memory for the process
         * @return The FD of the memory for the process
         */
        int GetMemoryFd() const;

        /**
         * @brief Map a chunk of process local memory (private memory)
         * @param address The address to map to (Can be 0 if address doesn't matter)
         * @param size The size of the chunk of memory
         * @param perms The permissions of the memory
         * @param type The type of the memory
         * @param region The specific region this memory is mapped for
         * @return The HandleOut of the created KPrivateMemory
         */
        HandleOut<KPrivateMemory> MapPrivateRegion(u64 address, size_t size, const memory::Permission perms, const memory::Type type, const memory::Region region);

        /**
         * @brief Returns the total memory occupied by regions mapped for the process
         */
        size_t GetProgramSize();

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
             handleTable[handleIndex] = std::static_pointer_cast<KObject>(item);
             return {item, handleIndex++};
        }

        /**
         * @brief This inserts an item into the process handle table
         * @param item The item to insert
         * @return The handle of the corresponding item in the handle table
         */
        template<typename objectClass>
        handle_t InsertItem(std::shared_ptr<objectClass> item) {
            handleTable[handleIndex] = std::static_pointer_cast<KObject>(item);
            return handleIndex++;
        }

        /**
         * @brief Returns the underlying kernel object for a handle
         * @tparam objectClass The class of the kernel object present in the handle
         * @param handle The handle of the object
         * @return A shared pointer to the object
         */
        template<typename objectClass>
        std::shared_ptr<objectClass> GetHandle(handle_t handle) {
            KType objectType;
            if constexpr(std::is_same<objectClass, KThread>())
                objectType = KType::KThread;
            else if constexpr(std::is_same<objectClass, KProcess>())
                 objectType = KType::KProcess;
            else if constexpr(std::is_same<objectClass, KSharedMemory>())
                 objectType = KType::KSharedMemory;
            else if constexpr(std::is_same<objectClass, KPrivateMemory>())
                 objectType = KType::KPrivateMemory;
            else if constexpr(std::is_same<objectClass, KSession>())
                 objectType = KType::KSession;
            else
                throw exception("KProcess::GetHandle couldn't determine object type");
            try {
                auto item = handleTable.at(handle);
                if (item->objectType == objectType)
                    return std::static_pointer_cast<objectClass>(item);
                else
                    throw exception(fmt::format("Tried to get kernel object (0x{:X}) with different type: {} when object is {}", handle, objectType, item->objectType));
            } catch (std::out_of_range) {
                throw exception(fmt::format("GetHandle was called with invalid handle: 0x{:X}", handle));
            }
        }
    };
}
