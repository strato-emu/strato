// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>

namespace skyline::service::lm {
    /**
     * @brief ILogger is used by applications to print messages to the system log (https://switchbrew.org/wiki/Log_services#ILogger)
     */
    class ILogger : public BaseService {
      private:
        /**
         * @brief This enumerates the field types in a log message
         */
        enum class LogFieldType : u8 {
            Start = 0, //!< This is the first log message in the stream
            Stop = 1, //!< This is the final log message in the stream
            Message = 2, //!< This log field contains a general message
            Line = 3, //!< This log field contains a line number
            Filename = 4, //!< This log field contains a filename
            Function = 5, //!< This log field contains a function name
            Module = 6, //!< This log field contains a module name
            Thread = 7, //!< This log field contains a thread name
            DropCount = 8, //!< This log field contains the number of dropped messages
            Time = 9, //!< This log field contains a timestamp
            ProgramName = 10, //!< This log field contains the program's name
        };

        /**
          * @brief This enumerates the log levels for log messages
          */
        enum class LogLevel : u8 {
            Trace, //!< This is a trace log
            Info, //!< This is an info log
            Warning, //!< This is a warning log
            Error, //!< This is an error log
            Critical //!< This is a critical log
        };

        /**
         * @brief Obtains a string containing the name of the given field type
         * @param type The field type to return the name of
         * @return The name of the given field type
         */
        std::string GetFieldName(LogFieldType type);

      public:
        ILogger(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief This prints a message to the log (https://switchbrew.org/wiki/Log_services#Log)
         */
        Result Log(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This sets the log destination (https://switchbrew.org/wiki/Log_services#SetDestination)
         */
        Result SetDestination(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, ILogger, Log),
            SFUNC(0x1, ILogger, SetDestination)
        )
    };
}
