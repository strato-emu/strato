#pragma once

#include <map>
#include <unordered_map>
#include <vector>
#include <fstream>
#include <syslog.h>
#include <mutex>
#import <thread>
#include <string>
#include <sstream>
#include <memory>
#include <fmt/format.h>
#include <sys/mman.h>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <jni.h>
#include "nce/guest_common.h"

namespace skyline {
    using handle_t = u32; //!< The type of a kernel handle

    namespace constant {
        // Memory
        constexpr u64 BaseAddr = 0x8000000; //!< The address space base
        constexpr u64 MapAddr = BaseAddr + 0x80000000; //!< The address of the map region
        constexpr u64 HeapAddr = MapAddr + 0x1000000000; //!< The address of the heap region
        constexpr u64 BaseEnd = 0x7FFFFFFFFF; //!< The end of the address space
        constexpr u64 MapSize = 0x1000000000; //!< The size of the map region
        constexpr u64 TotalPhyMem = 0xF8000000; // ~4 GB of RAM
        constexpr size_t DefStackSize = 0x1E8480; //!< The default amount of stack: 2 MB
        constexpr size_t HeapSizeDiv = 0x200000; //!< The amount heap size has to be divisible by
        constexpr size_t DefHeapSize = HeapSizeDiv; //!< The default amount of heap
        constexpr size_t TlsSlotSize = 0x200; //!< The size of a single TLS slot
        constexpr u8 TlsSlots = PAGE_SIZE / TlsSlotSize; //!< The amount of TLS slots in a single page
        // Loader
        constexpr u32 NroMagic = 0x304F524E; //!< "NRO0" in reverse, this is written at the start of every NRO file
        // NCE
        constexpr u8 NumRegs = 30; //!< The amount of registers that ARMv8 has
        constexpr u16 SvcLast = 0x7F; //!< The index of the last SVC
        constexpr u16 BrkRdy = 0xFF; //!< This is reserved for our kernel's to know when a process/thread is ready
        constexpr u32 TpidrroEl0 = 0x5E83; //!< ID of TPIDRRO_EL0 in MRS
        constexpr u32 CntfrqEl0 = 0x5F00; //!< ID of CNTFRQ_EL0 in MRS
        constexpr u32 TegraX1Freq = 0x124F800; //!< The clock frequency of the Tegra X1
        constexpr u32 CntpctEl0 = 0x5F01; //!< ID of CNTPCT_EL0 in MRS
        constexpr u32 CntvctEl0 = 0x5F02; //!< ID of CNTVCT_EL0 in MRS
        // Kernel
        constexpr u64 MaxSyncHandles = 0x40; //!< The total amount of handles that can be passed to WaitSynchronization
        constexpr handle_t BaseHandleIndex = 0xD000; // The index of the base handle
        constexpr handle_t ThreadSelf = 0xFFFF8000; //!< This is the handle used by threads to refer to themselves
        constexpr u8 DefaultPriority = 31; //!< The default priority of a process
        constexpr std::pair<int8_t, int8_t> PriorityAn = {19, -8}; //!< The range of priority for Android, taken from https://medium.com/mindorks/exploring-android-thread-priority-5d0542eebbd1
        constexpr std::pair<u8, u8> PriorityNin = {0, 63}; //!< The range of priority for the Nintendo Switch
        constexpr u32 MtxOwnerMask = 0xBFFFFFFF; //!< The mask of values which contain the owner of a mutex
        constexpr u32 CheckInterval = 10000000; //!< The amount of cycles to wait between checking if the guest thread is dead
        // IPC
        constexpr size_t TlsIpcSize = 0x100; //!< The size of the IPC command buffer in a TLS slot
        constexpr u8 PortSize = 0x8; //!< The size of a port name string
        constexpr u32 SfcoMagic = 0x4F434653; //!< SFCO in reverse, written to IPC messages
        constexpr u32 SfciMagic = 0x49434653; //!< SFCI in reverse, present in received IPC messages
        constexpr u64 IpcPaddingSum = 0x10; //!< The sum of the padding surrounding DataPayload
        constexpr handle_t BaseVirtualHandleIndex = 0x1; // The index of the base virtual handle
        // GPU
        constexpr u32 HandheldResolutionW = 1280; //!< The width component of the handheld resolution
        constexpr u32 HandheldResolutionH = 720; //!< The height component of the handheld resolution
        constexpr u32 DockedResolutionW = 1920; //!< The width component of the docked resolution
        constexpr u32 DockedResolutionH = 1080; //!< The height component of the docked resolution
        constexpr u32 TokenLength = 0x50; //!< The length of the token on BufferQueue parcels
        constexpr u32 GobHeight = 0x8; //!< The height of a blocklinear GOB
        constexpr u32 GobStride = 0x40; //!< The stride of a blocklinear GOB
        constexpr u32 GobSize = GobHeight * GobStride; //!< The size of a blocklinear GOB
        // Status codes
        namespace status {
            constexpr u32 Success = 0x0; //!< "Success"
            constexpr u32 NoMessages = 0x680; //!< "No message available"
            constexpr u32 ServiceInvName = 0xC15; //!< "Invalid name"
            constexpr u32 ServiceNotReg = 0xE15; //!< "Service not registered"
            constexpr u32 InvSize = 0xCA01; //!< "Invalid size"
            constexpr u32 InvAddress = 0xCC01; //!< "Invalid address"
            constexpr u32 InvPermission = 0xE001; //!< "Invalid Permission"
            constexpr u32 InvHandle = 0xE401; //!< "Invalid handle"
            constexpr u32 InvCombination = 0xE801; //!< "Invalid combination"
            constexpr u32 MaxHandles = 0xEE01; //!< "Too many handles"
            constexpr u32 Timeout = 0xEA01; //!< "Timeout"
            constexpr u32 NotFound = 0xF201; //!< "Not found"
            constexpr u32 Unimpl = 0x177202; //!< "Unimplemented behaviour"
        }
    };

    /**
     * @brief This enumerates the types of the ROM
     * @note This needs to be synchronized with emu.skyline.loader.BaseLoader.TitleFormat
     */
    enum class TitleFormat {
        NRO, //!< The NRO format: https://switchbrew.org/wiki/NRO
        XCI, //!< The XCI format: https://switchbrew.org/wiki/XCI
        NSP, //!< The NSP format from "nspwn" exploit: https://switchbrew.org/wiki/Switch_System_Flaws
    };

    /**
     * @brief The Mutex class is a wrapper around an atomic bool used for synchronization
     */
    class Mutex {
        std::atomic_flag flag = ATOMIC_FLAG_INIT; //!< An atomic flag to hold the state of the mutex

      public:
        /**
         * @brief Wait on and lock the mutex
         */
        void lock();

        /**
         * @brief Lock the mutex if it is unlocked else return
         * @return If the mutex was successfully locked or not
         */
        bool try_lock();

        /**
         * @brief Unlock the mutex if it is held by this thread
         */
        void unlock();
    };

    /**
     * @brief The Logger class is to write log output to file and logcat
     */
    class Logger {
      private:
        std::ofstream logFile; //!< An output stream to the log file
        const char *levelStr[4] = {"0", "1", "2", "3"}; //!< This is used to denote the LogLevel when written out to a file
        static constexpr int levelSyslog[4] = {LOG_ERR, LOG_WARNING, LOG_INFO, LOG_DEBUG}; //!< This corresponds to LogLevel and provides it's equivalent for syslog

      public:
        enum class LogLevel { Error, Warn, Info, Debug }; //!< The level of a particular log
        LogLevel configLevel; //!< The level of logs to write

        /**
         * @param logFd A FD to the log file
         * @param configLevel The minimum level of logs to write
         */
        Logger(const int logFd, LogLevel configLevel);

        /**
         * @brief Writes the termination message to the log file
         */
        ~Logger();

        /**
         * @brief Writes a header, should only be used for emulation starting and ending
         * @param str The value to be written
         */
        void WriteHeader(const std::string &str);

        /**
         * @brief Write a log to the log file
         * @param level The level of the log
         * @param str The value to be written
         */
        void Write(const LogLevel level, std::string str);

        /**
         * @brief Write an error log with libfmt formatting
         * @param formatStr The value to be written, with libfmt formatting
         * @param args The arguments based on format_str
         */
        template<typename S, typename... Args>
        inline void Error(const S &formatStr, Args &&... args) {
            if (LogLevel::Error <= configLevel) {
                Write(LogLevel::Error, fmt::format(formatStr, args...));
            }
        }

        /**
         * @brief Write a debug log with libfmt formatting
         * @param formatStr The value to be written, with libfmt formatting
         * @param args The arguments based on format_str
         */
        template<typename S, typename... Args>
        inline void Warn(const S &formatStr, Args &&... args) {
            if (LogLevel::Warn <= configLevel) {
                Write(LogLevel::Warn, fmt::format(formatStr, args...));
            }
        }

        /**
         * @brief Write a debug log with libfmt formatting
         * @param formatStr The value to be written, with libfmt formatting
         * @param args The arguments based on format_str
         */
        template<typename S, typename... Args>
        inline void Info(const S &formatStr, Args &&... args) {
            if (LogLevel::Info <= configLevel) {
                Write(LogLevel::Info, fmt::format(formatStr, args...));
            }
        }

        /**
         * @brief Write a debug log with libfmt formatting
         * @param formatStr The value to be written, with libfmt formatting
         * @param args The arguments based on format_str
         */
        template<typename S, typename... Args>
        inline void Debug(const S &formatStr, Args &&... args) {
            if (LogLevel::Debug <= configLevel) {
                Write(LogLevel::Debug, fmt::format(formatStr, args...));
            }
        }
    };

    /**
     * @brief The Settings class is used to access the parameters set in the Java component of the application
     */
    class Settings {
      private:
        std::map<std::string, std::string> stringMap; //!< A mapping from all keys to their corresponding string value
        std::map<std::string, bool> boolMap; //!< A mapping from all keys to their corresponding boolean value
        std::map<std::string, int> intMap; //!< A mapping from all keys to their corresponding integer value

      public:
        /**
         * @param preferenceFd An FD to the preference XML file
         */
        Settings(const int preferenceFd);

        /**
         * @brief Retrieves a particular setting as a string
         * @param key The key of the setting
         * @return The string value of the setting
         */
        std::string GetString(const std::string &key);

        /**
         * @brief Retrieves a particular setting as a boolean
         * @param key The key of the setting
         * @return The boolean value of the setting
         */
        bool GetBool(const std::string &key);

        /**
         * @brief Retrieves a particular setting as a integer
         * @param key The key of the setting
         * @return The integer value of the setting
         */
        int GetInt(const std::string &key);

        /**
         * @brief Writes all settings keys and values to syslog. This function is for development purposes.
         */
        void List(const std::shared_ptr<Logger> &logger);
    };

    /**
     * @brief This is a std::runtime_error with libfmt formatting
     */
    class exception : public std::runtime_error {
      public:
        /**
         * @param formatStr The exception string to be written, with libfmt formatting
         * @param args The arguments based on format_str
         */
        template<typename S, typename... Args>
        inline exception(const S &formatStr, Args &&... args) : runtime_error(fmt::format(formatStr, args...)) {}
    };

    /**
     * @brief Returns the current time in nanoseconds
     * @return The current time in nanoseconds
     */
    inline u64 GetCurrTimeNs() {
        return static_cast<u64>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count());
    }

    class NCE;
    class JvmManager;
    namespace gpu {
        class GPU;
    }
    namespace kernel {
        namespace type {
            class KProcess;
            class KThread;
        }
        class OS;
    }

    /**
     * @brief This struct is used to hold the state of a device
     */
    struct DeviceState {
        DeviceState(kernel::OS *os, std::shared_ptr<kernel::type::KProcess> &process, std::shared_ptr<JvmManager> jvmManager, std::shared_ptr<Settings> settings, std::shared_ptr<Logger> logger);

        kernel::OS *os; //!< This holds a reference to the OS class
        std::shared_ptr<kernel::type::KProcess> &process; //!< This holds a reference to the process object
        thread_local static std::shared_ptr<kernel::type::KThread> thread; //!< This holds a reference to the current thread object
        thread_local static ThreadContext* ctx; //!< This holds the context of the thread
        std::shared_ptr<NCE> nce; //!< This holds a reference to the NCE class
        std::shared_ptr<gpu::GPU> gpu; //!< This holds a reference to the GPU class
        std::shared_ptr<JvmManager> jvmManager; //!< This holds a reference to the JvmManager class
        std::shared_ptr<Settings> settings; //!< This holds a reference to the Settings class
        std::shared_ptr<Logger> logger; //!< This holds a reference to the Logger class
    };
}
