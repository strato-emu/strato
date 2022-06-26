// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Copyright © 2020 Ryujinx Team and Contributors (https://github.com/Ryujinx/)

#include <kernel/types/KProcess.h>
#include <services/account/IAccountServiceForApplication.h>
#include <services/am/storage/VectorIStorage.h>
#include <applet/applet_creator.h>
#include "ILibraryAppletAccessor.h"

namespace skyline::service::am {
    ILibraryAppletAccessor::ILibraryAppletAccessor(const DeviceState &state, ServiceManager &manager, skyline::applet::AppletId appletId, applet::LibraryAppletMode appletMode)
        : BaseService(state, manager),
          stateChangeEvent(std::make_shared<type::KEvent>(state, false)),
          popNormalOutDataEvent((std::make_shared<type::KEvent>(state, false))),
          popInteractiveOutDataEvent((std::make_shared<type::KEvent>(state, false))),
          applet(skyline::applet::CreateApplet(state, manager, appletId, stateChangeEvent, popNormalOutDataEvent, popInteractiveOutDataEvent, appletMode)) {
        stateChangeEventHandle = state.process->InsertItem(stateChangeEvent);
        popNormalOutDataEventHandle = state.process->InsertItem(popNormalOutDataEvent);
        popInteractiveOutDataEventHandle = state.process->InsertItem(popInteractiveOutDataEvent);
        Logger::Debug("Applet accessor for {} ID created with appletMode 0x{:X}", ToString(appletId), appletMode);
    }

    Result ILibraryAppletAccessor::GetAppletStateChangedEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        Logger::Debug("Applet State Change Event Handle: 0x{:X}", stateChangeEventHandle);
        response.copyHandles.push_back(stateChangeEventHandle);
        return {};
    }

    Result ILibraryAppletAccessor::Start(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return applet->Start();
    }

    Result ILibraryAppletAccessor::GetResult(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return applet->GetResult();
    }

    Result ILibraryAppletAccessor::PushInData(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        applet->PushNormalDataToApplet(request.PopService<IStorage>(0, session));
        return {};
    }

    Result ILibraryAppletAccessor::PushInteractiveInData(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        applet->PushInteractiveDataToApplet(request.PopService<IStorage>(0, session));
        return {};
    }

    Result ILibraryAppletAccessor::PopOutData(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        if (auto outIStorage{applet->PopNormalAndClear()}) {
            manager.RegisterService(outIStorage, session, response);
            return {};
        }
        return result::NotAvailable;
    }

    Result ILibraryAppletAccessor::PopInteractiveOutData(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        if (auto outIStorage = applet->PopInteractiveAndClear()) {
            manager.RegisterService(outIStorage, session, response);
            return {};
        }
        return result::NotAvailable;
    }

    Result ILibraryAppletAccessor::GetPopOutDataEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.copyHandles.push_back(popNormalOutDataEventHandle);
        return {};
    }

    Result ILibraryAppletAccessor::GetPopInteractiveOutDataEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.copyHandles.push_back(popInteractiveOutDataEventHandle);
        return {};
    }
}
