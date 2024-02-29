// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "common.h"
#include "nce.h"
#include "soc.h"
#include "gpu.h"
#include "audio.h"
#include "input.h"
#include "kernel/types/KProcess.h"

namespace skyline {
    DeviceState::DeviceState(kernel::OS *os, std::shared_ptr<JvmManager> jvmManager, std::shared_ptr<Settings> settings)
        : os(os), jvm(std::move(jvmManager)), settings(std::move(settings)) {
        // We assign these later as they use the state in their constructor and we don't want null pointers
        gpu = std::make_shared<gpu::GPU>(*this);
        soc = std::make_shared<soc::SOC>(*this);
        audio = std::make_shared<audio::Audio>(*this);
        input = std::make_shared<input::Input>(*this);
    }

    DeviceState::~DeviceState() {
        if (process)
            process->ClearHandleTable();
    }
}
