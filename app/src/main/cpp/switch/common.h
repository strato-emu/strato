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


namespace lightSwitch {
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
        constexpr u64 base_addr = 0x8000000; //!< The address space base
        constexpr u64 map_addr = base_addr + 0x80000000; //!< The address of the map region
        constexpr u64 base_size = 0x7FF8000000; //!< The size of the address space
        constexpr u64 map_size = 0x1000000000; //!< The size of the map region
        constexpr u64 total_phy_mem = 0xF8000000; // ~4 GB of RAM
        constexpr size_t def_stack_size = 0x1E8480; //!< The default amount of stack: 2 MB
        constexpr size_t def_heap_size = PAGE_SIZE; //!< The default amount of heap
        constexpr size_t tls_slot_size = 0x200; //!< The size of a single TLS slot
        constexpr u8 tls_slots = PAGE_SIZE / tls_slot_size; //!< The amount of TLS slots in a single page
        // Loader
        constexpr u32 nro_magic = 0x304F524E; //!< "NRO0" in reverse, this is written at the start of every NRO file
        // NCE
        constexpr u8 num_regs = 31; //!< The amount of registers that ARMv8 has
        constexpr u16 svc_last = 0x7F; //!< The index of the last SVC
        constexpr u16 brk_rdy = 0xFF; //!< This is reserved for our kernel's to know when a process/thread is ready
        constexpr u32 tpidrro_el0 = 0x5E83; //!< ID of tpidrro_el0 in MRS
        // IPC
        constexpr size_t tls_ipc_size = 0x100; //!< The size of the IPC command buffer in a TLS slot
        constexpr handle_t sm_handle = 0xD000; //!< sm:'s handle
        constexpr u8 port_size = 0x8; //!< The size of a port name string
        constexpr u32 sfco_magic = 0x4F434653; //!< SFCO in reverse, written to IPC messages
        constexpr u32 sfci_magic = 0x49434653; //!< SFCI in reverse, present in received IPC messages
        constexpr u64 padding_sum = 0x10; //!< The sum of the padding surrounding DataPayload
        // Process
        constexpr handle_t base_handle_index = sm_handle + 1; // The index of the base handle
        constexpr u8 default_priority = 31; //!< The default priority of a process
        constexpr std::pair<int8_t, int8_t> priority_an = {19, -8}; //!< The range of priority for Android, taken from https://medium.com/mindorks/exploring-android-thread-priority-5d0542eebbd1
        constexpr std::pair<u8, u8> priority_nin = {0, 63}; //!< The range of priority for the Nintendo Switch
        // Status codes
        namespace status {
            constexpr u32 success = 0x0; //!< "Success"
            constexpr u32 inv_address = 0xCC01; //!< "Invalid address"
            constexpr u32 inv_handle = 0xE401; //!< "Invalid handle"
            constexpr u32 unimpl = 0x177202; //!< "Unimplemented behaviour"
        }
    };

    namespace instr {
        /**
         * A bit-field struct that encapsulates a BRK instruction. It can be used to generate as well as parse the instruction's opcode. See https://developer.arm.com/docs/ddi0596/latest/base-instructions-alphabetic-order/brk-breakpoint-instruction.
         */
        struct brk {
            /**
             * Creates a BRK instruction with a specific immediate value, used for generating BRK opcodes
             * @param val The immediate value of the instruction
             */
            brk(u16 val) {
                start = 0x0; // First 5 bits of an BRK instruction are 0
                value = val;
                end = 0x6A1; // Last 11 bits of an BRK instruction stored as u16
            }

            /**
             * @return If the opcode represents a valid BRK instruction
             */
            bool verify() {
                return (start == 0x0 && end == 0x6A1);
            }

            u8 start : 5;
            u32 value : 16;
            u16 end : 11;
        };

        static_assert(sizeof(brk) == sizeof(u32));

        /**
         * A bit-field struct that encapsulates a SVC instruction. See https://developer.arm.com/docs/ddi0596/latest/base-instructions-alphabetic-order/svc-supervisor-call.
         */
        struct svc {
            /**
             * @return If the opcode represents a valid SVC instruction
             */
            bool verify() {
                return (start == 0x1 && end == 0x6A0);
            }

            u8 start : 5;
            u32 value : 16;
            u16 end : 11;
        };

        static_assert(sizeof(svc) == sizeof(u32));

        /**
         * A bit-field struct that encapsulates a MRS instruction. See https://developer.arm.com/docs/ddi0596/latest/base-instructions-alphabetic-order/mrs-move-system-register.
         */
        struct mrs {
            /**
             * @return If the opcode represents a valid MRS instruction
             */
            bool verify() {
                return (end == 0xD53);
            }

            u8 dst_reg : 5;
            u32 src_reg : 15;
            u16 end : 12;
        };

        static_assert(sizeof(mrs) == sizeof(u32));
    };

    /**
     * Read about ARMv8 registers here: https://developer.arm.com/docs/100878/latest/registers
     */
    enum class xreg { x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30 };
    enum class wreg { w0, w1, w2, w3, w4, w5, w6, w7, w8, w9, w10, w11, w12, w13, w14, w15, w16, w17, w18, w19, w20, w21, w22, w23, w24, w25, w26, w27, w28, w29, w30 };
    enum class sreg { sp, pc, pstate };

    /**
     * The Settings class is used to access the parameters set in the Java component of the application
     */
    class Settings {
      private:
        struct KeyCompare {
            bool operator()(char const *a, char const *b) const {
                return std::strcmp(a, b) < 0;
            }
        }; //!< This is a comparision operator between strings, implemented to store strings in a std::map

        std::map<char *, char *, KeyCompare> string_map; //!< A mapping from all keys to their corresponding string value
        std::map<char *, bool, KeyCompare> bool_map; //!< A mapping from all keys to their corresponding boolean value

      public:
        /**
         * @param pref_xml The path to the preference XML file
         */
        Settings(std::string &pref_xml);

        /**
         * @param key The key of the setting
         * @return The string value of the setting
         */
        char *GetString(char *key);

        /**
         * @param key The key of the setting
         * @return The boolean value of the setting
         */
        bool GetBool(char *key);

        /**
         * Writes all settings keys and values to syslog. This function is for development purposes.
         */
        void List();
    };

    /**
     * The Logger class is to generate a log of the program
     */
    class Logger {
      private:
        std::ofstream log_file; //!< An output stream to the log file
        const char *level_str[4] = {"0", "1", "2", "3"}; //!< This is used to denote the LogLevel when written out to a file
        static constexpr int level_syslog[4] = {LOG_ERR, LOG_WARNING, LOG_INFO, LOG_DEBUG}; //!< This corresponds to LogLevel and provides it's equivalent for syslog

      public:
        enum LogLevel { ERROR, WARN, INFO, DEBUG }; //!< The level of a particular log

        /**
         * @param log_path The path to the log file
         */
        Logger(const std::string &log_path);

        /**
         * Writes "Logging ended" to as a header
         */
        ~Logger();

        /**
         * Writes a header, should only be used for emulation starting and ending
         * @param str The value to be written
         */
        void WriteHeader(const std::string &str);

        /**
         * Write a log to the log file
         * @param level The level of the log
         * @param str The value to be written
         */
        void Write(const LogLevel level, const std::string &str);

        /**
         * Write a log to the log file with libfmt formatting
         * @param level The level of the log
         * @param format_str The value to be written, with libfmt formatting
         * @param args The arguments based on format_str
         */
        template<typename S, typename... Args>
        void Write(Logger::LogLevel level, const S &format_str, Args &&... args) {
            #ifdef NDEBUG
            if (level == DEBUG) return;
            #endif
            Write(level, fmt::format(format_str, args...));
        }
    };

    // Predeclare some classes here as we use them in device_state
    class NCE;
    namespace kernel {
        namespace type {
            class KProcess;

            class KThread;
        }
        class OS;
    }

    /**
     * This struct is used to hold the state of a device
     */
    struct device_state {
        device_state(kernel::OS *os, std::shared_ptr<kernel::type::KProcess> &this_process, std::shared_ptr<kernel::type::KThread> &this_thread, std::shared_ptr<NCE> nce, std::shared_ptr<Settings> settings, std::shared_ptr<Logger> logger) : os(os), nce(nce), settings(settings), logger(logger), this_process(this_process), this_thread(this_thread) {}

        kernel::OS *os; // Because OS holds the device_state struct, it's destruction will accompany that of device_state
        std::shared_ptr<kernel::type::KProcess> &this_process;
        std::shared_ptr<kernel::type::KThread> &this_thread;
        std::shared_ptr<NCE> nce;
        std::shared_ptr<Settings> settings;
        std::shared_ptr<Logger> logger;
    };
}
