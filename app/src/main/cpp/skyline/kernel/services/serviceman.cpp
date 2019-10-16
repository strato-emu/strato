#include "serviceman.h"
#include <kernel/types/KProcess.h>
#include "sm/sm.h"
#include "set/sys.h"
#include "apm/apm.h"
#include "am/appletOE.h"
#include "fatal/fatal.h"
#include "hid/hid.h"

namespace skyline::kernel::service {
    ServiceManager::ServiceManager(const DeviceState &state) : state(state) {}

    std::shared_ptr<BaseService> ServiceManager::GetService(const Service serviceType) {
        std::shared_ptr<BaseService> serviceObj;
        switch (serviceType) {
                case Service::sm:
                    serviceObj = std::make_shared<sm::sm>(state, *this);
                    break;
                case Service::fatal_u:
                    serviceObj = std::make_shared<fatal::fatalU>(state, *this);
                    break;
                case Service::set_sys:
                    serviceObj = std::make_shared<set::sys>(state, *this);
                    break;
                case Service::apm:
                    serviceObj = std::make_shared<apm::apm>(state, *this);
                    break;
                case Service::apm_ISession:
                    serviceObj = std::make_shared<apm::ISession>(state, *this);
                    break;
                case Service::am_appletOE:
                    serviceObj = std::make_shared<am::appletOE>(state, *this);
                    break;
                case Service::am_IApplicationProxy:
                    serviceObj = std::make_shared<am::IApplicationProxy>(state, *this);
                    break;
                case Service::am_ICommonStateGetter:
                    serviceObj = std::make_shared<am::ICommonStateGetter>(state, *this);
                    break;
                case Service::am_IWindowController:
                    serviceObj = std::make_shared<am::IWindowController>(state, *this);
                    break;
                case Service::am_IAudioController:
                    serviceObj = std::make_shared<am::IAudioController>(state, *this);
                    break;
                case Service::am_IDisplayController:
                    serviceObj = std::make_shared<am::IDisplayController>(state, *this);
                    break;
                case Service::am_ISelfController:
                    serviceObj = std::make_shared<am::ISelfController>(state, *this);
                    break;
                case Service::am_ILibraryAppletCreator:
                    serviceObj = std::make_shared<am::ILibraryAppletCreator>(state, *this);
                    break;
                case Service::am_IApplicationFunctions:
                    serviceObj = std::make_shared<am::IApplicationFunctions>(state, *this);
                    break;
                case Service::am_IDebugFunctions:
                    serviceObj = std::make_shared<am::IDebugFunctions>(state, *this);
                    break;
                case Service::hid:
                    serviceObj = std::make_shared<hid::hid>(state, *this);
                    break;
                case Service::hid_IAppletResource:
                    serviceObj = std::make_shared<hid::IAppletResource>(state, *this);
                    break;
        }
        serviceVec.push_back(serviceObj);
        return serviceObj;
    }

    handle_t ServiceManager::NewSession(const Service serviceType) {
        return state.thisProcess->NewHandle<type::KSession>(GetService(serviceType), serviceType).handle;
    }

    std::shared_ptr<BaseService> ServiceManager::NewService(const Service serviceType, type::KSession &session, ipc::IpcResponse &response) {
        auto serviceObject = GetService(serviceType);
        if (response.isDomain) {
            session.domainTable[++session.handleIndex] = serviceObject;
            response.domainObjects.push_back(session.handleIndex);
        } else
            response.moveHandles.push_back(state.thisProcess->NewHandle<type::KSession>(serviceObject, serviceType).handle);
        state.logger->Write(Logger::Debug, "Service has been registered: \"{}\"", serviceObject->getName());
        return serviceObject;
    }

    void ServiceManager::CloseSession(const handle_t handle) {
        auto session = state.thisProcess->GetHandle<type::KSession>(handle);
        if (session->serviceStatus == type::KSession::ServiceStatus::Open) {
            if (session->isDomain) {
                for (const auto &[objectId, service] : session->domainTable)
                    serviceVec.erase(std::remove(serviceVec.begin(), serviceVec.end(), service), serviceVec.end());
            } else
                serviceVec.erase(std::remove(serviceVec.begin(), serviceVec.end(), session->serviceObject), serviceVec.end());
            session->serviceStatus = type::KSession::ServiceStatus::Closed;
        }
    };

    void ServiceManager::Loop() {
        for (auto &service : serviceVec)
            if (service->hasLoop)
                service->Loop();
    }

    void ServiceManager::SyncRequestHandler(const handle_t handle) {
        auto session = state.thisProcess->GetHandle<type::KSession>(handle);
        state.logger->Write(Logger::Debug, "----Start----");
        state.logger->Write(Logger::Debug, "Handle is 0x{:X}", handle);

        if (session->serviceStatus == type::KSession::ServiceStatus::Open) {
            ipc::IpcRequest request(session->isDomain, state);
            ipc::IpcResponse response(session->isDomain, state);

            switch (static_cast<ipc::CommandType>(request.header->type)) {
                case ipc::CommandType::Request:
                case ipc::CommandType::RequestWithContext:
                    if (session->isDomain) {
                        try {
                            auto service = session->domainTable.at(request.domain->object_id);
                            switch (static_cast<ipc::DomainCommand>(request.domain->command)) {
                                case ipc::DomainCommand::SendMessage:
                                    service->HandleRequest(*session, request, response);
                                    break;
                                case ipc::DomainCommand::CloseVHandle:
                                    serviceVec.erase(std::remove(serviceVec.begin(), serviceVec.end(), service), serviceVec.end());
                                    session->domainTable.erase(request.domain->object_id);
                                    break;
                            }
                        } catch (std::out_of_range&) {
                            throw exception("Invalid object ID was used with domain request");
                        }
                    } else
                        session->serviceObject->HandleRequest(*session, request, response);
                    response.WriteTls();
                    break;

                case ipc::CommandType::Control:
                case ipc::CommandType::ControlWithContext:
                    state.logger->Write(Logger::Debug, "Control IPC Message: {}", request.payload->value);
                    switch (static_cast<ipc::ControlCommand>(request.payload->value)) {
                        case ipc::ControlCommand::ConvertCurrentObjectToDomain:
                            response.WriteValue(session->ConvertDomain());
                            break;
                        case ipc::ControlCommand::CloneCurrentObject:
                        case ipc::ControlCommand::CloneCurrentObjectEx:
                            CloneSession(*session, request, response);
                            break;
                        default:
                            throw exception(fmt::format("Unknown Control Command: {}", request.payload->value));
                    }
                    response.WriteTls();
                    break;

                case ipc::CommandType::Close:
                    state.logger->Write(Logger::Debug, "Closing Session");
                    CloseSession(handle);
                    break;

                default:
                    throw exception(fmt::format("Unimplemented IPC message type: {}", u16(request.header->type)));
            }
        } else
            state.logger->Write(Logger::Warn, "svcSendSyncRequest called on closed handle: 0x{:X}", handle);
        state.logger->Write(Logger::Debug, "====End====");
    }

    void ServiceManager::CloneSession(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        NewService(session.serviceType, session, response);
    }
}
