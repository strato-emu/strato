#pragma once

#include <map>
#include <unordered_map>
#include <vector>
#include <fstream>
#include <syslog.h>
#include <string>
#include <sstream>
#include <memory>
#include <fmt/format.h>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace skyline {
    // Global typedefs
    typedef __uint128_t u128;
    typedef __uint64_t u64;
    typedef __uint32_t u32;
    typedef __uint16_t u16;
    typedef __uint8_t u8;
    typedef __int128_t i128;
    typedef __int64_t i64;
    typedef __int32_t i32;
    typedef __int16_t i16;
    typedef __int8_t i8;
    typedef std::runtime_error exception; //!< This is used as the default exception
    typedef u32 handle_t; //!< The type of an handle

    namespace constant {
        // Memory
        constexpr u64 BaseAddr = 0x8000000; //!< The address space base
        constexpr u64 MapAddr = BaseAddr + 0x80000000; //!< The address of the map region
        constexpr u64 BaseSize = 0x7FF8000000; //!< The size of the address space
        constexpr u64 MapSize = 0x1000000000; //!< The size of the map region
        constexpr u64 TotalPhyMem = 0xF8000000; // ~4 GB of RAM
        constexpr size_t DefStackSize = 0x1E8480; //!< The default amount of stack: 2 MB
        constexpr size_t DefHeapSize = PAGE_SIZE; //!< The default amount of heap
        constexpr size_t TlsSlotSize = 0x200; //!< The size of a single TLS slot
        constexpr u8 TlsSlots = PAGE_SIZE / TlsSlotSize; //!< The amount of TLS slots in a single page
        // Loader
        constexpr u32 NroMagic = 0x304F524E; //!< "NRO0" in reverse, this is written at the start of every NRO file
        // NCE
        constexpr u8 NumRegs = 31; //!< The amount of registers that ARMv8 has
        constexpr u16 SvcLast = 0x7F; //!< The index of the last SVC
        constexpr u16 BrkRdy = 0xFF; //!< This is reserved for our kernel's to know when a process/thread is ready
        constexpr u32 TpidrroEl0 = 0x5E83; //!< ID of TPIDRRO_EL0 in MRS
        // Kernel
        constexpr u64 MaxSyncHandles = 0x40; //!< The total amount of handles that can be passed to WaitSynchronization
        constexpr handle_t BaseHandleIndex = 0xD000; // The index of the base handle
        constexpr u8 DefaultPriority = 31; //!< The default priority of a process
        constexpr std::pair<int8_t, int8_t> PriorityAn = {19, -8}; //!< The range of priority for Android, taken from https://medium.com/mindorks/exploring-android-thread-priority-5d0542eebbd1
        constexpr std::pair<u8, u8> PriorityNin = {0, 63}; //!< The range of priority for the Nintendo Switch
        // IPC
        constexpr size_t TlsIpcSize = 0x100; //!< The size of the IPC command buffer in a TLS slot
        constexpr u8 PortSize = 0x8; //!< The size of a port name string
        constexpr u32 SfcoMagic = 0x4F434653; //!< SFCO in reverse, written to IPC messages
        constexpr u32 SfciMagic = 0x49434653; //!< SFCI in reverse, present in received IPC messages
        constexpr u64 PaddingSum = 0x10; //!< The sum of the padding surrounding DataPayload
        constexpr handle_t BaseVirtualHandleIndex = 0x1; // The index of the base virtual handle
        // Status codes
        namespace status {
            constexpr u32 Success = 0x0; //!< "Success"
            constexpr u32 ServiceInvName = 0xC15; //!< "Invalid name"
            constexpr u32 ServiceNotReg = 0xE15; //!< "Service not registered"
            constexpr u32 InvAddress = 0xCC01; //!< "Invalid address"
            constexpr u32 InvHandle = 0xE401; //!< "Invalid handle"
            constexpr u32 MaxHandles = 0xEE01; //!< "Too many handles"
            constexpr u32 Timeout = 0xEA01; //!< "Timeout while svcWaitSynchronization"
            constexpr u32 Unimpl = 0x177202; //!< "Unimplemented behaviour"
        }
    };

    namespace instr {
        /**
         * @brief A bit-field struct that encapsulates a BRK instruction. It can be used to generate as well as parse the instruction's opcode. See https://developer.arm.com/docs/ddi0596/latest/base-instructions-alphabetic-order/brk-breakpoint-instruction.
         */
        struct Brk {
            /**
             * Creates a BRK instruction with a specific immediate value, used for generating BRK opcodes
             * @param val The immediate value of the instruction
             */
            Brk(u16 val) {
                start = 0x0; // First 5 bits of an BRK instruction are 0
                value = val;
                end = 0x6A1; // Last 11 bits of an BRK instruction stored as u16
            }

            /**
             * @brief Returns if the opcode is valid or not
             * @return If the opcode represents a valid BRK instruction
             */
            bool Verify() {
                return (start == 0x0 && end == 0x6A1);
            }

            u8 start : 5;
            u32 value : 16;
            u16 end : 11;
        };

        static_assert(sizeof(Brk) == sizeof(u32));

        /**
         * @brief A bit-field struct that encapsulates a SVC instruction. See https://developer.arm.com/docs/ddi0596/latest/base-instructions-alphabetic-order/svc-supervisor-call.
         */
        struct Svc {
            /**
             * @brief Returns if the opcode is valid or not
             * @return If the opcode represents a valid SVC instruction
             */
            bool Verify() {
                return (start == 0x1 && end == 0x6A0);
            }

            u8 start : 5;
            u32 value : 16;
            u16 end : 11;
        };

        static_assert(sizeof(Svc) == sizeof(u32));

        /**
         * @brief A bit-field struct that encapsulates a MRS instruction. See https://developer.arm.com/docs/ddi0596/latest/base-instructions-alphabetic-order/mrs-move-system-register.
         */
        struct Mrs {
            /**
             * @brief Returns if the opcode is valid or not
             * @return If the opcode represents a valid MRS instruction
             */
            bool Verify() {
                return (end == 0xD53);
            }

            u8 dstReg : 5;
            u32 srcReg : 15;
            u16 end : 12;
        };

        static_assert(sizeof(Mrs) == sizeof(u32));
    };

    /**
     * Read about ARMv8 registers here: https://developer.arm.com/docs/100878/latest/registers
     */
    enum class Xreg { X0, X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17, X18, X19, X20, X21, X22, X23, X24, X25, X26, X27, X28, X29, X30 };
    enum class Wreg { W0, W1, W2, W3, W4, W5, W6, W7, W8, W9, W10, W11, W12, W13, W14, W15, W16, W17, W18, W19, W20, W21, W22, W23, W24, W25, W26, W27, W28, W29, W30 };
    enum class Sreg { Sp, Pc, PState };

    /**
     * @brief The Logger class is to generate a log of the program
     */
    class Logger {
      private:
        std::ofstream logFile; //!< An output stream to the log file
        const char *levelStr[4] = {"0", "1", "2", "3"}; //!< This is used to denote the LogLevel when written out to a file
        static constexpr int levelSyslog[4] = {LOG_ERR, LOG_WARNING, LOG_INFO, LOG_DEBUG}; //!< This corresponds to LogLevel and provides it's equivalent for syslog

      public:
        enum LogLevel { Error, Warn, Info, Debug }; //!< The level of a particular log

        /**
         * @param logPath The path to the log file
         */
        Logger(const std::string &logPath);

        /**
         * Writes "Logging ended" as a header
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
        void Write(const LogLevel level, const std::string &str);

        /**
         * @brief Write a log to the log file with libfmt formatting
         * @param level The level of the log
         * @param formatStr The value to be written, with libfmt formatting
         * @param args The arguments based on format_str
         */
        template<typename S, typename... Args>
        void Write(Logger::LogLevel level, const S &formatStr, Args &&... args) {
            #ifdef NDEBUG
            if (level == Debug) return;
            #endif
            Write(level, fmt::format(formatStr, args...));
        }
    };

    /**
     * @brief The Settings class is used to access the parameters set in the Java component of the application
     */
    class Settings {
      private:
        std::map<std::string, std::string> stringMap; //!< A mapping from all keys to their corresponding string value
        std::map<std::string, bool> boolMap; //!< A mapping from all keys to their corresponding boolean value

      public:
        /**
         * @param prefXml The path to the preference XML file
         */
        Settings(const std::string &prefXml);

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
         * @brief Writes all settings keys and values to syslog. This function is for development purposes.
         */
        void List(std::shared_ptr<Logger> &logger);
    };

    // Predeclare some classes here as we use them in DeviceState
    class NCE;
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
        DeviceState(kernel::OS *os, std::shared_ptr<kernel::type::KProcess> &thisProcess, std::shared_ptr<kernel::type::KThread> &thisThread, std::shared_ptr<NCE> nce, std::shared_ptr<Settings> settings, std::shared_ptr<Logger> logger) : os(os), nce(nce), settings(settings), logger(logger), thisProcess(thisProcess), thisThread(thisThread) {}

        kernel::OS *os; //!< This holds a reference to the OS class
        std::shared_ptr<kernel::type::KProcess> &thisProcess; //!< This holds a reference to the current process object
        std::shared_ptr<kernel::type::KThread> &thisThread; //!< This holds a reference to the current thread object
        std::shared_ptr<NCE> nce; //!< This holds a reference to the NCE class
        std::shared_ptr<Settings> settings; //!< This holds a reference to the Settings class
        std::shared_ptr<Logger> logger; //!< This holds a reference to the Logger class
    };
}
