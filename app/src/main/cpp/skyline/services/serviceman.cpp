// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include "sm/IUserInterface.h"
#include "settings/ISettingsServer.h"
#include "settings/ISystemSettingsServer.h"
#include "apm/IManager.h"
#include "am/IApplicationProxyService.h"
#include "am/IAllSystemAppletProxiesService.h"
#include "audio/IAudioOutManager.h"
#include "audio/IAudioRendererManager.h"
#include "fatalsrv/IService.h"
#include "hid/IHidServer.h"
#include "timesrv/IStaticService.h"
#include "fssrv/IFileSystemProxy.h"
#include "services/nvdrv/INvDrvServices.h"
#include "visrv/IManagerRootService.h"
#include "pl/IPlatformServiceManager.h"
#include "aocsrv/IAddOnContentManager.h"
#include "pctl/IParentalControlServiceFactory.h"
#include "lm/ILogService.h"
#include "account/IAccountServiceForApplication.h"
#include "friends/IServiceCreator.h"
#include "nfp/IUserManager.h"
#include "nifm/IStaticService.h"
#include "socket/bsd/IClient.h"
#include "ssl/ISslService.h"
#include "prepo/IPrepoService.h"
#include "serviceman.h"

#define SERVICE_CASE(class, name) \
    case util::MakeMagic<ServiceName>(name): { \
            std::shared_ptr<BaseService> serviceObject = std::make_shared<class>(state, *this); \
            serviceMap[util::MakeMagic<ServiceName>(name)] = serviceObject; \
            return serviceObject; \
        }

namespace skyline::service {
    ServiceManager::ServiceManager(const DeviceState &state) : state(state), smUserInterface(std::make_shared<sm::IUserInterface>(state, *this)) {}

    std::shared_ptr<BaseService> ServiceManager::CreateService(ServiceName name) {
        auto serviceIter = serviceMap.find(name);
        if (serviceIter != serviceMap.end())
            return (*serviceIter).second;

        switch (name) {
            SERVICE_CASE(fatalsrv::IService, "fatal:u")
            SERVICE_CASE(settings::ISettingsServer, "set")
            SERVICE_CASE(settings::ISystemSettingsServer, "set:sys")
            SERVICE_CASE(apm::IManager, "apm")
            SERVICE_CASE(am::IApplicationProxyService, "appletOE")
            SERVICE_CASE(am::IAllSystemAppletProxiesService, "appletAE")
            SERVICE_CASE(audio::IAudioOutManager, "audout:u")
            SERVICE_CASE(audio::IAudioRendererManager, "audren:u")
            SERVICE_CASE(hid::IHidServer, "hid")
            SERVICE_CASE(timesrv::IStaticService, "time:s")
            SERVICE_CASE(timesrv::IStaticService, "time:a")
            SERVICE_CASE(timesrv::IStaticService, "time:u")
            SERVICE_CASE(fssrv::IFileSystemProxy, "fsp-srv")
            SERVICE_CASE(nvdrv::INvDrvServices, "nvdrv")
            SERVICE_CASE(nvdrv::INvDrvServices, "nvdrv:a")
            SERVICE_CASE(nvdrv::INvDrvServices, "nvdrv:s")
            SERVICE_CASE(nvdrv::INvDrvServices, "nvdrv:t")
            SERVICE_CASE(visrv::IManagerRootService, "vi:m")
            SERVICE_CASE(visrv::IManagerRootService, "vi:u")
            SERVICE_CASE(visrv::IManagerRootService, "vi:s")
            SERVICE_CASE(pl::IPlatformServiceManager, "pl:u")
            SERVICE_CASE(aocsrv::IAddOnContentManager, "aoc:u")
            SERVICE_CASE(pctl::IParentalControlServiceFactory, "pctl")
            SERVICE_CASE(pctl::IParentalControlServiceFactory, "pctl:a")
            SERVICE_CASE(pctl::IParentalControlServiceFactory, "pctl:s")
            SERVICE_CASE(pctl::IParentalControlServiceFactory, "pctl:r")
            SERVICE_CASE(lm::ILogService, "lm")
            SERVICE_CASE(account::IAccountServiceForApplication, "acc:u0")
            SERVICE_CASE(friends::IServiceCreator, "friend")
            SERVICE_CASE(nfp::IUserManager, "nfp:user")
            SERVICE_CASE(nifm::IStaticService, "nifm:u")
            SERVICE_CASE(socket::IClient, "bsd:u")
            SERVICE_CASE(ssl::ISslService, "ssl")
            SERVICE_CASE(prepo::IPrepoService, "prepo:u")
            default:
                throw exception("CreateService called with an unknown service name: {}", name);
        }
    }

    std::shared_ptr<BaseService> ServiceManager::NewService(ServiceName name, type::KSession &session, ipc::IpcResponse &response) {
        std::lock_guard serviceGuard(mutex);
        auto serviceObject = CreateService(name);
        KHandle handle{};
        if (session.isDomain) {
            session.domainTable[++session.handleIndex] = serviceObject;
            response.domainObjects.push_back(session.handleIndex);
            handle = session.handleIndex;
        } else {
            handle = state.process->NewHandle<type::KSession>(serviceObject).handle;
            response.moveHandles.push_back(handle);
        }
        state.logger->Debug("Service has been created: \"{}\" (0x{:X})", serviceObject->GetName(), handle);
        return serviceObject;
    }

    void ServiceManager::RegisterService(std::shared_ptr<BaseService> serviceObject, type::KSession &session, ipc::IpcResponse &response) { // NOLINT(performance-unnecessary-value-param)
        std::lock_guard serviceGuard(mutex);
        KHandle handle{};

        if (session.isDomain) {
            session.domainTable[session.handleIndex] = serviceObject;
            response.domainObjects.push_back(session.handleIndex);
            handle = session.handleIndex++;
        } else {
            handle = state.process->NewHandle<type::KSession>(serviceObject).handle;
            response.moveHandles.push_back(handle);
        }

        state.logger->Debug("Service has been registered: \"{}\" (0x{:X})", serviceObject->GetName(), handle);
    }

    void ServiceManager::CloseSession(KHandle handle) {
        std::lock_guard serviceGuard(mutex);
        auto session = state.process->GetHandle<type::KSession>(handle);
        if (session->serviceStatus == type::KSession::ServiceStatus::Open) {
            if (session->isDomain) {
                for (const auto &domainEntry : session->domainTable)
                    std::erase_if(serviceMap, [domainEntry](const auto &entry) {
                        return entry.second == domainEntry.second;
                    });
            } else {
                std::erase_if(serviceMap, [session](const auto &entry) {
                    return entry.second == session->serviceObject;
                });
            }
            session->serviceStatus = type::KSession::ServiceStatus::Closed;
        }
    }

    void ServiceManager::SyncRequestHandler(KHandle handle) {
        auto session = state.process->GetHandle<type::KSession>(handle);
        state.logger->Debug("----Start----");
        state.logger->Debug("Handle is 0x{:X}", handle);

        if (session->serviceStatus == type::KSession::ServiceStatus::Open) {
            ipc::IpcRequest request(session->isDomain, state);
            ipc::IpcResponse response(state);

            switch (request.header->type) {
                case ipc::CommandType::Request:
                case ipc::CommandType::RequestWithContext:
                    if (session->isDomain) {
                        try {
                            auto service = session->domainTable.at(request.domain->objectId);
                            switch (static_cast<ipc::DomainCommand>(request.domain->command)) {
                                case ipc::DomainCommand::SendMessage:
                                    response.errorCode = service->HandleRequest(*session, request, response);
                                    break;
                                case ipc::DomainCommand::CloseVHandle:
                                    std::erase_if(serviceMap, [service](const auto &entry) {
                                        return entry.second == service;
                                    });
                                    session->domainTable.erase(request.domain->objectId);
                                    break;
                            }
                        } catch (std::out_of_range &) {
                            throw exception("Invalid object ID was used with domain request");
                        }
                    } else {
                        response.errorCode = session->serviceObject->HandleRequest(*session, request, response);
                    }
                    response.WriteResponse(session->isDomain);
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
                            response.moveHandles.push_back(state.process->InsertItem(session));
                            break;
                        case ipc::ControlCommand::QueryPointerBufferSize:
                            response.Push<u32>(0x1000);
                            break;
                        default:
                            throw exception("Unknown Control Command: {}", request.payload->value);
                    }
                    response.WriteResponse(false);
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
