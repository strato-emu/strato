// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IApplet.h"

namespace skyline::service::am {

    IApplet::IApplet(const DeviceState &state, ServiceManager &manager, std::shared_ptr<kernel::type::KEvent> onAppletStateChanged, std::shared_ptr<kernel::type::KEvent> onNormalDataPushFromApplet, std::shared_ptr<kernel::type::KEvent> onInteractiveDataPushFromApplet, applet::LibraryAppletMode appletMode)
        : BaseService(state, manager), onAppletStateChanged(std::move(onAppletStateChanged)),
          onNormalDataPushFromApplet(std::move(onNormalDataPushFromApplet)),
          onInteractiveDataPushFromApplet(std::move(onInteractiveDataPushFromApplet)) {}

    IApplet::~IApplet() = default;

    void IApplet::PushNormalDataAndSignal(std::shared_ptr<IStorage> data) {
        std::scoped_lock lock{outputDataMutex};
        normalOutputData.emplace(std::move(data));
        onNormalDataPushFromApplet->Signal();
    }

    void IApplet::PushInteractiveDataAndSignal(std::shared_ptr<IStorage> data) {
        std::scoped_lock lock{interactiveOutputDataMutex};
        interactiveOutputData.emplace(std::move(data));
        onInteractiveDataPushFromApplet->Signal();
    }

    std::shared_ptr<IStorage> IApplet::PopNormalAndClear() {
        std::scoped_lock lock{outputDataMutex};
        if (normalOutputData.empty())
            return {};
        std::shared_ptr<IStorage> data(normalOutputData.front());
        normalOutputData.pop();
        onNormalDataPushFromApplet->ResetSignal();
        return data;
    }

    std::shared_ptr<IStorage> IApplet::PopInteractiveAndClear() {
        std::scoped_lock lock{interactiveOutputDataMutex};
        if (interactiveOutputData.empty())
            return {};
        std::shared_ptr<IStorage> data(interactiveOutputData.front());
        interactiveOutputData.pop();
        onInteractiveDataPushFromApplet->ResetSignal();
        return data;
    }

}
