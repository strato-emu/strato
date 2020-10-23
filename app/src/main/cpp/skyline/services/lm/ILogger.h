// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::lm {
    /**
     * @brief ILogger is used by applications to print messages to the system log
     * @url https://switchbrew.org/wiki/Log_services#ILogger
     */
    class ILogger : public BaseService {
      private:
        enum class LogFieldType : u8 {
            Start = 0, //!< The first log message in the stream
            Stop = 1, //!< The final log message in the stream
            Message = 2, //!< A log field with a general message
            Line = 3, //!< A log field with a line number
            Filename = 4, //!< A log field with a filename
            Function = 5, //!< A log field with a function name
            Module = 6, //!< A log field with a module name
            Thread = 7, //!< A log field with a thread name
            DropCount = 8, //!< A log field with the number of dropped messages
            Time = 9, //!< A log field with a timestamp
            ProgramName = 10, //!< A log field with the program's name
        };

        enum class LogLevel : u8 {
            Trace,
            Info,
            Warning,
            Error,
            Critical,
        };

      public:
        ILogger(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief Prints a message to the log
         * @url https://switchbrew.org/wiki/Log_services#Log
         */
        Result Log(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Sets the log destination
         * @url https://switchbrew.org/wiki/Log_services#SetDestination
         */
        Result SetDestination(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, ILogger, Log),
            SFUNC(0x1, ILogger, SetDestination)
        )
    };
}
