// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include "ILogger.h"

namespace skyline::service::lm {
    ILogger::ILogger(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    std::string ILogger::GetFieldName(LogFieldType type) {
        switch (type) {
            case LogFieldType::Message:
                return "Message";
            case LogFieldType::Line:
                return "Line";
            case LogFieldType::Filename:
                return "Filename";
            case LogFieldType::Function:
                return "Function";
            case LogFieldType::Module:
                return "Module";
            case LogFieldType::Thread:
                return "Thread";
            case LogFieldType::DropCount:
                return "DropCount";
            case LogFieldType::Time:
                return "Time";
            case LogFieldType::ProgramName:
                return "ProgramName";
            default:
                return "";
        }
    }

    Result ILogger::Log(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        struct Data {
            u64 pid;
            u64 threadContext;
            u16 flags;
            LogLevel level;
            u8 verbosity;
            u32 payloadLength;
        } &data = request.inputBuf.at(0).as<Data>();

        std::ostringstream logMessage;
        logMessage << "Guest log:";

        u64 offset = sizeof(Data);
        while (offset < request.inputBuf[0].size()) {
            auto fieldType = request.inputBuf[0].subspan(offset++).as<LogFieldType>();
            auto length = request.inputBuf[0].subspan(offset++).as<u8>();
            auto object = request.inputBuf[0].subspan(offset, length);

            logMessage << " ";

            switch (fieldType) {
                case LogFieldType::Start:
                    offset += length;
                    continue;
                case LogFieldType::Line:
                    logMessage << GetFieldName(fieldType) << ": " << object.as<u32>();
                    offset += sizeof(u32);
                    continue;
                case LogFieldType::DropCount:
                    logMessage << GetFieldName(fieldType) << ": " << object.as<u64>();
                    offset += sizeof(u64);
                    continue;
                case LogFieldType::Time:
                    logMessage << GetFieldName(fieldType) << ": " << object.as<u64>() << "s";
                    offset += sizeof(u64);
                    continue;
                case LogFieldType::Stop:
                    break;
                default:
                    logMessage << GetFieldName(fieldType) << ": " << object.as_string();
                    offset += length;
                    continue;
            }

            break;
        }

        switch (data.level) {
            case LogLevel::Trace:
                state.logger->Debug("{}", logMessage.str());
                break;
            case LogLevel::Info:
                state.logger->Info("{}", logMessage.str());
                break;
            case LogLevel::Warning:
                state.logger->Warn("{}", logMessage.str());
                break;
            case LogLevel::Error:
            case LogLevel::Critical:
                state.logger->Error("{}", logMessage.str());
                break;
        }

        return {};
    }

    Result ILogger::SetDestination(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }
}
