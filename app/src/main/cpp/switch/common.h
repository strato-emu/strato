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
    typedef std::runtime_error exception; //!< This is used as the default exception
    typedef uint32_t handle_t; //!< The type of an handle

    namespace constant {
        // Memory
        constexpr uint64_t base_addr = 0x8000000; //!< The address space base
        constexpr uint64_t base_size = 0x7FF8000000; //!< The size of the address space
        constexpr uint64_t total_phy_mem = 0xF8000000; // ~4 GB of RAM
        constexpr size_t def_stack_size = 0x1E8480; //!< The default amount of stack: 2 MB
        constexpr size_t tls_slot_size = 0x200; //!< The size of a single TLS slot
        constexpr uint8_t tls_slots = PAGE_SIZE / constant::tls_slot_size; //!< The amount of TLS slots in a single page
        // Loader
        constexpr uint32_t nro_magic = 0x304F524E; //!< "NRO0" in reverse, this is written at the start of every NRO file
        // NCE
        constexpr uint8_t num_regs = 31; //!< The amount of registers that ARMv8 has
        constexpr uint16_t svc_last = 0x7F; //!< The index of the last SVC
        constexpr uint16_t brk_rdy = 0xFF; //!< This is reserved for our kernel's to know when a process/thread is ready
        constexpr uint32_t tpidrro_el0 = 0x5E83; //!< ID of tpidrro_el0 in MRS
        // IPC
        constexpr size_t tls_ipc_size = 0x100; //!< The size of the IPC command buffer in a TLS slot
        constexpr uint64_t sm_handle = 0xd000; //!< sm:'s handle
        constexpr uint8_t port_size = 0x8; //!< The size of a port name string
        constexpr uint32_t ipc_sfco = 0x4F434653; //!< SFCO in reverse
        // Process
        constexpr uint32_t base_handle_index = 0xD001; // The index of the base handle
        constexpr uint8_t default_priority = 31; //!< The default priority of a process
        constexpr std::pair<int8_t, int8_t> priority_an = {19, -8}; //!< The range of priority for Android, taken from https://medium.com/mindorks/exploring-android-thread-priority-5d0542eebbd1
        constexpr std::pair<uint8_t, uint8_t> priority_nin = {0, 63}; //!< The range of priority for the Nintendo Switch
    };

    namespace instr {
        /**
         * A bitfield struct that encapsulates a BRK instruction. It can be used to generate as well as parse the instruction's opcode. See https://developer.arm.com/docs/ddi0596/latest/base-instructions-alphabetic-order/brk-breakpoint-instruction.
         */
        struct brk {
            /**
             * Creates a BRK instruction with a specific immediate value, used for generating BRK opcodes
             * @param val The immediate value of the instruction
             */
            brk(uint16_t val) {
                start = 0x0; // First 5 bits of an BRK instruction are 0
                value = val;
                end = 0x6A1; // Last 11 bits of an BRK instruction stored as uint16_t
            }

            /**
             * @return If the opcode represents a valid BRK instruction
             */
            bool verify() {
                return (start == 0x0 && end == 0x6A1);
            }

            uint8_t start : 5;
            uint32_t value : 16;
            uint16_t end : 11;
        };

        /**
         * A bitfield struct that encapsulates a SVC instruction. See https://developer.arm.com/docs/ddi0596/latest/base-instructions-alphabetic-order/svc-supervisor-call.
         */
        struct svc {
            /**
             * @return If the opcode represents a valid SVC instruction
             */
            bool verify() {
                return (start == 0x1 && end == 0x6A0);
            }

            uint8_t start : 5;
            uint32_t value : 16;
            uint16_t end : 11;
        };

        /**
         * A bitfield struct that encapsulates a MRS instruction. See https://developer.arm.com/docs/ddi0596/latest/base-instructions-alphabetic-order/mrs-move-system-register.
         */
        struct mrs {
            /**
             * @return If the opcode represents a valid MRS instruction
             */
            bool verify() {
                return (end == 0xD53);
            }

            uint8_t dst_reg : 5;
            uint32_t src_reg : 15;
            uint16_t end : 12;
        };
    };

    /**
     * Read about ARMv8 registers here: https://developer.arm.com/docs/100878/latest/registers
     */
    namespace regs {
        enum xreg { x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30 };
        enum wreg { w0, w1, w2, w3, w4, w5, w6, w7, w8, w9, w10, w11, w12, w13, w14, w15, w16, w17, w18, w19, w20, w21, w22, w23, w24, w25, w26, w27, w28, w29, w30 };
        enum sreg { sp, pc, pstate };
    }

    namespace memory {
        /**
         * The Permission struct holds the permission of a particular chunk of memory
         */
        struct Permission {
            /**
             * Initializes all values to false
             */
            Permission();

            /**
             * @param read If memory has read permission
             * @param write If memory has write permission
             * @param execute If memory has execute permission
             */
            Permission(bool read, bool write, bool execute);

            /**
             * Equality operator between two Permission objects
             */
            bool operator==(const Permission &rhs) const;

            /**
             * Inequality operator between two Permission objects
             */
            bool operator!=(const Permission &rhs) const;

            /**
             * @return The value of the permission struct in mmap(2) format
             */
            int get() const;

            bool r, w, x;
        };

        /**
         * Memory Regions that are mapped by the kernel
         */
        enum class Region {
            heap, tls, text, rodata, data, bss
        };

        /**
         * The RegionData struct holds information about a corresponding Region of memory such as address and size
         */
        struct RegionData {
            uint64_t address;
            size_t size;
            Permission perms;
            int fd;
        };
    }

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
        Settings(std::string pref_xml);

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
        enum LogLevel {ERROR, WARN, INFO, DEBUG}; //!< The level of a particular log

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
        std::shared_ptr<kernel::type::KProcess>& this_process;
        std::shared_ptr<kernel::type::KThread>& this_thread;
        std::shared_ptr<NCE> nce;
        std::shared_ptr<Settings> settings;
        std::shared_ptr<Logger> logger;
    };
}
