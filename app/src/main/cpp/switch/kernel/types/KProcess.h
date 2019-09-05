#pragma once

#include "KThread.h"

namespace lightSwitch::kernel::type {
    /**
     * The KProcess class is responsible for holding the state of a process
     */
    class KProcess : public KObject {
    private:
        /**
         * tls_page_t holds the status of a single TLS page (A page is 4096 bytes on ARMv8).
         * Each TLS page has 8 slots, each 0x200 (512) bytes in size.
         * The first slot of the first page is reserved for user-mode exception handling
         * Read more about TLS here: https://switchbrew.org/wiki/Thread_Local_Storage
         */
        struct tls_page_t {
            uint64_t address; //!< The address of the page allocated for TLS
            uint8_t index = 0; //!< The slots are assigned sequentially, this holds the index of the last TLS slot reserved
            bool slot[constant::tls_slots]{0}; //!< An array of booleans denoting which TLS slots are reserved

            /**
             * @param address The address of the allocated page
             */
            tls_page_t(uint64_t address);
            /**
             * Reserves a single 0x200 byte TLS slot
             * @return The address of the reserved slot
             */
            uint64_t ReserveSlot();
            /**
             * Returns the address of a particular slot
             * @param slot_no The number of the slot to be returned
             * @return The address of the specified slot
             */
            uint64_t Get(uint8_t slot_no);
            /**
             * @return If the whole page is full or not
             */
            bool Full();
        };
        /**
         * @param init If this initializes the first page (As the first TLS slot is reserved)
         * @return The address of a free TLS slot
         */
        uint64_t GetTLSSlot(bool init);

        int mem_fd; //!< The file descriptor to the memory of the process
        const device_state& state; //!< The state of the device

    public:
        enum class process_state_t { Created, CreatedAttached, Started, Crashed, StartedAttached, Exiting, Exited, DebugSuspended } process_state; //!< The state of the process
        handle_t handle; //!< The handle of the current process in it's parent process's handle table (Will be 0 if this is the main process)
        handle_t handle_index = constant::base_handle_index; //!< This is used to keep track of what to map as an handle
        pid_t main_thread; //!< The PID of the main thread
        size_t main_thread_stack_sz; //!< The size of the main thread's stack (All other threads map stack themselves so we don't know the size per-se)
        std::map<memory::Region, memory::RegionData> memory_map; //!< A mapping from every memory::Region to it's corresponding memory::RegionData which holds it's address and size
        std::map<handle_t, std::shared_ptr<KObject>> handle_table; //!< A mapping from a handle_t to it's corresponding KObject which is the actual underlying object
        std::map<pid_t, std::shared_ptr<KThread>> thread_map; //!< A mapping from a PID to it's corresponding KThread object
        std::vector<std::shared_ptr<tls_page_t>> tls_pages; //!< A vector of all allocated TLS pages

        /**
         * Creates a KThread object for the main thread and opens the process's memory file
         * @param pid The PID of the main thread
         * @param entry_point The address to start execution at
         * @param stack_base The base of the stack
         * @param stack_size The size of the stack
         * @param state The state of the device
         * @param handle A handle to the process, this isn't used if the kernel creates the process
         */
        KProcess(pid_t pid, uint64_t entry_point, uint64_t stack_base, uint64_t stack_size, const device_state& state, handle_t handle=0);

        /**
         * Close the file descriptor to the process's memory
         */
        ~KProcess();

        /**
         * Create a thread in this process
         * @param entry_point The address of the initial function
         * @param entry_arg An argument to the function
         * @param stack_top The top of the stack
         * @param priority The priority of the thread
         * @return An instance of KThread class for the corresponding thread
         */
        std::shared_ptr<KThread> CreateThread(uint64_t entry_point, uint64_t entry_arg, uint64_t stack_top, uint8_t priority);

        /**
         * Returns an object of type T from process memory
         * @tparam T The type of the object to be read
         * @param address The address of the object
         * @return An object of type T with read data
         */
        template <typename T>
        T ReadMemory(uint64_t address) const;

        /**
         * Writes an object of type T to process memory
         * @tparam T The type of the object to be written
         * @param item The object to write
         * @param address The address of the object
         */
        template<typename T>
        void WriteMemory(T &item, uint64_t address) const;

        /**
         * Read a piece of process memory
         * @param destination The address to the location where the process memory is written
         * @param offset The address to read from in process memory
         * @param size The amount of memory to be read
         */
        void ReadMemory(void *destination, uint64_t offset, size_t size) const;

        /**
         * Write a piece of process memory
         * @param source The address of where the data to be written is present
         * @param offset The address to write to in process memory
         * @param size The amount of memory to be written
         */
        void WriteMemory(void *source, uint64_t offset, size_t size) const;

        /**
         * Map a chunk of process local memory (private memory)
         * @param address The address to map to (Can be 0 if address doesn't matter)
         * @param size The size of the chunk of memory
         * @param perms The permissions of the memory
         * @return The address of the mapped chunk (Use when address is 0)
         */
        uint64_t MapPrivate(uint64_t address, size_t size, const memory::Permission perms);

        /**
         * Map a chunk of process local memory (private memory)
         * @param address The address to map to (Can be 0 if address doesn't matter)
         * @param size The size of the chunk of memory
         * @param perms The permissions of the memory
         * @param region The specific region this memory is mapped for
         * @return The address of the mapped chunk (Use when address is 0)
         */
        uint64_t MapPrivate(uint64_t address, size_t size, const memory::Permission perms, const memory::Region region);

        /**
         * Remap a chunk of memory as to change the size occupied by it
         * @param address The address of the mapped memory
         * @param old_size The current size of the memory
         * @param size The new size of the memory
         */
        void RemapPrivate(uint64_t address, size_t old_size, size_t size);

        /**
         * Remap a chunk of memory as to change the size occupied by it
         * @param region The region of memory that was mapped
         * @param size The new size of the memory
         */
        void RemapPrivate(const memory::Region region, size_t size);

        /**
         * Updates the permissions of a chunk of mapped memory
         * @param address The address of the mapped memory
         * @param size The size of the mapped memory
         * @param perms The new permissions to be set for the memory
         */
        void UpdatePermissionPrivate(uint64_t address, size_t size, const memory::Permission perms);

        /**
         * Updates the permissions of a chunk of mapped memory
         * @param region The region of memory that was mapped
         * @param perms The new permissions to be set for the memory
         */
        void UpdatePermissionPrivate(const memory::Region region, const memory::Permission perms);

        /**
         * Unmap a particular chunk of mapped memory
         * @param address The address of the mapped memory
         * @param size The size of the mapped memory
         */
        void UnmapPrivate(uint64_t address, size_t size);

        /**
         * Unmap a particular chunk of mapped memory
         * @param region The region of mapped memory
         */
        void UnmapPrivate(const memory::Region region);

        /**
         * Creates a new handle to a KObject and adds it to the process handle_table
         * @param obj A shared pointer to the KObject to be added to the table
         * @return The handle of the corresponding object
         */
        handle_t NewHandle(std::shared_ptr<KObject> obj);
    };
}
