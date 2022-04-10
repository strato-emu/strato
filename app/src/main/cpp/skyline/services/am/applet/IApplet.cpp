// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IApplet.h"

namespace skyline::service::am {

    IApplet::IApplet(const DeviceState &state, ServiceManager &manager, std::shared_ptr<kernel::type::KEvent> onAppletStateChanged, std::shared_ptr<kernel::type::KEvent> onNormalDataPushFromApplet, std::shared_ptr<kernel::type::KEvent> onInteractiveDataPushFromApplet, applet::LibraryAppletMode appletMode)
        : BaseService(state, manager), onAppletStateChanged(std::move(onAppletStateChanged)),
          onNormalDataPushFromApplet(std::move(onNormalDataPushFromApplet)),
          onInteractiveDataPushFromApplet(std::move(onInteractiveDataPushFromApplet)) {}

    IApplet::~IApplet() = default;

    void IApplet::PushNormalDataAndSignal(const std::shared_ptr<IStorage> &data) {
        normalOutputData.emplace(data);
        onNormalDataPushFromApplet->Signal();
    }

    void IApplet::PushInteractiveDataAndSignal(const std::shared_ptr<IStorage> &data) {
        interactiveOutputData.emplace(data);
        onInteractiveDataPushFromApplet->Signal();
    }

    std::shared_ptr<IStorage> IApplet::PopNormalAndClear() {
        if (normalOutputData.empty())
            return {};
        std::shared_ptr<IStorage> data(normalOutputData.front());
        normalOutputData.pop();
        onNormalDataPushFromApplet->ResetSignal();
        return data;
    }

    std::shared_ptr<IStorage> IApplet::PopInteractiveAndClear() {
        if (interactiveOutputData.empty())
            return {};
        std::shared_ptr<IStorage> data(interactiveOutputData.front());
        interactiveOutputData.pop();
        onInteractiveDataPushFromApplet->ResetSignal();
        return data;
    }

}
