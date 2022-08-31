// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <common/signal.h>
#include <loader/loader.h>
#include <kernel/types/KProcess.h>
#include "input.h"

namespace skyline::input {
    Input::Input(const DeviceState &state)
        : state{state},
          kHid{std::make_shared<kernel::type::KSharedMemory>(state, sizeof(HidSharedMemory))},
          hid{reinterpret_cast<HidSharedMemory *>(kHid->host.data())},
          npad{state, hid},
          touch{state, hid},
          updateThread{&Input::UpdateThread, this} {}

    void Input::UpdateThread() {
        if (int result{pthread_setname_np(pthread_self(), "Sky-Input")})
            Logger::Warn("Failed to set the thread name: {}", strerror(result));

        try {
            signal::SetSignalHandler({SIGINT, SIGILL, SIGTRAP, SIGBUS, SIGFPE, SIGSEGV}, signal::ExceptionalSignalHandler);

            struct UpdateCallback {
                std::chrono::milliseconds period;
                std::chrono::steady_clock::time_point next;
                std::function<void(UpdateCallback &)> callback;

                UpdateCallback(std::chrono::milliseconds period, std::function<void(UpdateCallback &)> callback)
                    : period{period}, next{std::chrono::steady_clock::now() + period}, callback{std::move(callback)} {}

                void operator()() {
                    callback(*this);
                    next += period;
                }
            };

            constexpr std::chrono::milliseconds NPadUpdatePeriod{4}; //!< The period at which a Joy-Con is updated (250Hz)
            constexpr std::chrono::milliseconds TouchUpdatePeriod{4}; //!< The period at which the touch screen is updated (250Hz)

            std::array<UpdateCallback, 2> updateCallbacks{
                UpdateCallback{NPadUpdatePeriod, [&](UpdateCallback &callback) {
                    for (auto &pad : npad.npads)
                        pad.UpdateSharedMemory();
                }},
                UpdateCallback{TouchUpdatePeriod, [&](UpdateCallback &callback) {
                    touch.UpdateSharedMemory();
                }},
            };

            while (true) {
                auto now{std::chrono::steady_clock::now()}, next{updateCallbacks[0].next};
                for (auto &callback : updateCallbacks) {
                    if (now >= callback.next)
                        callback();

                    if (callback.next < next)
                        next = callback.next;
                }
                std::this_thread::sleep_until(next);
            }
        } catch (const signal::SignalException &e) {
            Logger::Error("{}\nStack Trace:{}", e.what(), state.loader->GetStackTrace(e.frames));
            if (state.process)
                state.process->Kill(false);
            else
                std::rethrow_exception(std::current_exception());
        } catch (const std::exception &e) {
            Logger::Error(e.what());
            if (state.process)
                state.process->Kill(false);
            else
                std::rethrow_exception(std::current_exception());
        }
    }
}
