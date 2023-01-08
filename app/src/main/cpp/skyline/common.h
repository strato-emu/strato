// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <bit>
#include <map>
#include <unordered_map>
#include <list>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <functional>
#include <thread>
#include <string>
#include <memory>
#include <compare>
#include <condition_variable>
#include <boost/container/small_vector.hpp>
#include <common/exception.h>
#include <common/span.h>
#include <common/result.h>
#include <common/logger.h>

namespace skyline {
    class Settings;
    namespace nce {
        class NCE;
        struct ThreadContext;
    }
    class JvmManager;
    namespace gpu {
        class GPU;
    }
    namespace soc {
        class SOC;
    }
    namespace kernel {
        namespace type {
            class KProcess;
            class KThread;
        }
        class Scheduler;
        class OS;
    }
    namespace audio {
        class Audio;
    }
    namespace input {
        class Input;
    }
    namespace loader {
        class Loader;
    }

    /**
     * @brief The state of the entire emulator is contained within this class, all objects related to emulation are tied into it
     */
    struct DeviceState {
        DeviceState(kernel::OS *os, std::shared_ptr<JvmManager> jvmManager, std::shared_ptr<Settings> settings);

        ~DeviceState();

        kernel::OS *os;
        std::shared_ptr<JvmManager> jvm;
        std::shared_ptr<Settings> settings;
        std::shared_ptr<loader::Loader> loader;
        std::shared_ptr<nce::NCE> nce;
        std::shared_ptr<kernel::type::KProcess> process{};
        static thread_local inline std::shared_ptr<kernel::type::KThread> thread{}; //!< The KThread of the thread which accesses this object
        static thread_local inline nce::ThreadContext *ctx{}; //!< The context of the guest thread for the corresponding host thread
        std::shared_ptr<gpu::GPU> gpu;
        std::shared_ptr<soc::SOC> soc;
        std::shared_ptr<audio::Audio> audio;
        std::shared_ptr<kernel::Scheduler> scheduler;
        std::shared_ptr<input::Input> input;
    };
}
