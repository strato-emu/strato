// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include "ILogger.h"

namespace skyline::service::lm {
    ILogger::ILogger(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, Service::lm_ILogger, "lm:ILogger", {
        {0x0, SFUNC(ILogger::Log)},
        {0x1, SFUNC(ILogger::SetDestination)}
    }) {}

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

    void ILogger::Log(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        struct Data {
            u64 pid;
            u64 threadContext;
            u16 flags;
            LogLevel level;
            u8 verbosity;
            u32 payloadLength;
        } data = state.process->GetReference<Data>(request.inputBuf.at(0).address);

        std::ostringstream logMessage;
        logMessage << "Guest log: ";

        u64 offset = sizeof(Data);
        while (offset < request.inputBuf.at(0).size) {
            auto fieldType = state.process->GetObject<LogFieldType>(request.inputBuf.at(0).address + offset++);
            auto length = state.process->GetObject<u8>(request.inputBuf.at(0).address + offset++);
            auto address = request.inputBuf.at(0).address + offset;

            switch  (fieldType) {
                case LogFieldType::Start:
                    offset += length;
                    continue;
                case LogFieldType::Line:
                    logMessage << GetFieldName(fieldType) << ": " << state.process->GetObject<u32>(address);
                    offset += sizeof(u32);
                    continue;
                case LogFieldType::DropCount:
                    logMessage << GetFieldName(fieldType) << ": " << state.process->GetObject<u64>(address);
                    offset += sizeof(u64);
                    continue;
                case LogFieldType::Time:
                    logMessage << GetFieldName(fieldType) << ": " << state.process->GetObject<u64>(address) << "s";
                    offset += sizeof(u64);
                    continue;
                case LogFieldType::Stop:
                    break;
                default:
                    logMessage << GetFieldName(fieldType) << ": " << state.process->GetString(address, length);
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

    }

    void ILogger::SetDestination(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {}
}
