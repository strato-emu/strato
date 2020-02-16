#include <kernel/types/KProcess.h>
#include <services/audren/IAudioRendererManager.h>
#include "sm/IUserInterface.h"
#include "settings/ISystemSettingsServer.h"
#include "apm/apm.h"
#include "am/applet.h"
#include "am/appletController.h"
#include "audout/audout.h"
#include "fatal/fatal.h"
#include "hid/IHidServer.h"
#include "timesrv/IStaticService.h"
#include "fs/fs.h"
#include "nvdrv/nvdrv.h"
#include "vi/vi_m.h"
#include "serviceman.h"

namespace skyline::service {
    ServiceManager::ServiceManager(const DeviceState &state) : state(state) {}

    std::shared_ptr<BaseService> ServiceManager::GetService(const Service serviceType) {
        if (serviceMap.count(serviceType))
            return serviceMap.at(serviceType);

        std::shared_ptr<BaseService> serviceObj;
        switch (serviceType) {
            case Service::sm_IUserInterface:
                serviceObj = std::make_shared<sm::IUserInterface>(state, *this);
                break;
            case Service::fatal_u:
                serviceObj = std::make_shared<fatal::fatalU>(state, *this);
                break;
            case Service::settings_ISystemSettingsServer:
                serviceObj = std::make_shared<settings::ISystemSettingsServer>(state, *this);
                break;
            case Service::apm:
                serviceObj = std::make_shared<apm::apm>(state, *this);
                break;
            case Service::am_appletOE:
                serviceObj = std::make_shared<am::appletOE>(state, *this);
                break;
            case Service::am_appletAE:
                serviceObj = std::make_shared<am::appletAE>(state, *this);
                break;
            case Service::audout_u:
                serviceObj = std::make_shared<audout::audoutU>(state, *this);
                break;
            case Service::IAudioRendererManager:
                serviceObj = std::make_shared<audren::IAudioRendererManager>(state, *this);
                break;
            case Service::hid_IHidServer:
                serviceObj = std::make_shared<hid::IHidServer>(state, *this);
                break;
            case Service::timesrv_IStaticService:
                serviceObj = std::make_shared<timesrv::IStaticService>(state, *this);
                break;
            case Service::fs_fsp:
                serviceObj = std::make_shared<fs::fsp>(state, *this);
                break;
            case Service::nvdrv:
                serviceObj = std::make_shared<nvdrv::nvdrv>(state, *this);
                break;
            case Service::vi_m:
                serviceObj = std::make_shared<vi::vi_m>(state, *this);
                break;
            default:
                throw exception("GetService called on missing object, type: {}", serviceType);
        }
        serviceMap[serviceType] = serviceObj;
        return serviceObj;
    }

    handle_t ServiceManager::NewSession(const Service serviceType) {
        std::lock_guard serviceGuard(mutex);
        return state.process->NewHandle<type::KSession>(GetService(serviceType)).handle;
    }

    std::shared_ptr<BaseService> ServiceManager::NewService(const std::string &serviceName, type::KSession &session, ipc::IpcResponse &response) {
        std::lock_guard serviceGuard(mutex);
        auto serviceObject = GetService(ServiceString.at(serviceName));
        handle_t handle{};
        if (response.isDomain) {
            session.domainTable[++session.handleIndex] = serviceObject;
            response.domainObjects.push_back(session.handleIndex);
            handle = session.handleIndex;
        } else {
            handle = state.process->NewHandle<type::KSession>(serviceObject).handle;
            response.moveHandles.push_back(handle);
        }
        state.logger->Debug("Service has been created: \"{}\" (0x{:X})", serviceName, handle);
        return serviceObject;
    }

    void ServiceManager::RegisterService(std::shared_ptr<BaseService> serviceObject, type::KSession &session, ipc::IpcResponse &response) { // NOLINT(performance-unnecessary-value-param)
        std::lock_guard serviceGuard(mutex);
        handle_t handle{};
        if (response.isDomain) {
            session.domainTable[session.handleIndex] = serviceObject;
            response.domainObjects.push_back(session.handleIndex);
            handle = session.handleIndex++;
        } else {
            handle = state.process->NewHandle<type::KSession>(serviceObject).handle;
            response.moveHandles.push_back(handle);
        }
        state.logger->Debug("Service has been registered: \"{}\" (0x{:X})", serviceObject->serviceName, handle);
    }

    void ServiceManager::CloseSession(const handle_t handle) {
        std::lock_guard serviceGuard(mutex);
        auto session = state.process->GetHandle<type::KSession>(handle);
        if (session->serviceStatus == type::KSession::ServiceStatus::Open) {
            if (session->isDomain) {
                for (const auto &[objectId, service] : session->domainTable)
                    serviceMap.erase(service->serviceType);
            } else {
                serviceMap.erase(session->serviceObject->serviceType);
            }
            session->serviceStatus = type::KSession::ServiceStatus::Closed;
        }
    };

    void ServiceManager::SyncRequestHandler(const handle_t handle) {
        auto session = state.process->GetHandle<type::KSession>(handle);
        state.logger->Debug("----Start----");
        state.logger->Debug("Handle is 0x{:X}", handle);

        if (session->serviceStatus == type::KSession::ServiceStatus::Open) {
            ipc::IpcRequest request(session->isDomain, state);
            ipc::IpcResponse response(session->isDomain, state);

            switch (request.header->type) {
                case ipc::CommandType::Request:
                case ipc::CommandType::RequestWithContext:
                    if (session->isDomain) {
                        try {
                            auto service = session->domainTable.at(request.domain->objectId);
                            switch (static_cast<ipc::DomainCommand>(request.domain->command)) {
                                case ipc::DomainCommand::SendMessage:
                                    service->HandleRequest(*session, request, response);
                                    break;
                                case ipc::DomainCommand::CloseVHandle:
                                    serviceMap.erase(service->serviceType);
                                    session->domainTable.erase(request.domain->objectId);
                                    break;
                            }
                        } catch (std::out_of_range &) {
                            throw exception("Invalid object ID was used with domain request");
                        }
                    } else {
                        session->serviceObject->HandleRequest(*session, request, response);
                    }
                    if (!response.nWrite)
                        response.WriteResponse();
                    break;
                case ipc::CommandType::Control:
                case ipc::CommandType::ControlWithContext:
                    state.logger->Debug("Control IPC Message: 0x{:X}", request.payload->value);
                    switch (static_cast<ipc::ControlCommand>(request.payload->value)) {
                        case ipc::ControlCommand::ConvertCurrentObjectToDomain:
                            response.Push(session->ConvertDomain());
                            break;
                        case ipc::ControlCommand::CloneCurrentObject:
                        case ipc::ControlCommand::CloneCurrentObjectEx:
                            response.Push(state.process->InsertItem(session));
                            break;
                        case ipc::ControlCommand::QueryPointerBufferSize:
                            response.Push<u32>(0x1000);
                            break;
                        default:
                            throw exception("Unknown Control Command: {}", request.payload->value);
                    }
                    response.WriteResponse();
                    break;
                case ipc::CommandType::Close:
                    state.logger->Debug("Closing Session");
                    CloseSession(handle);
                    break;
                default:
                    throw exception("Unimplemented IPC message type: {}", static_cast<u16>(request.header->type));
            }
        } else {
            state.logger->Warn("svcSendSyncRequest called on closed handle: 0x{:X}", handle);
        }
        state.logger->Debug("====End====");
    }
}
