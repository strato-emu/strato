// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <common/signal.h>
#include "types/KThread.h"
#include "scheduler.h"

namespace skyline::kernel {
    Scheduler::CoreContext::CoreContext(u8 id) : id(id) {}

    Scheduler::Scheduler(const DeviceState &state) : state(state) {}

    Scheduler::CoreContext &Scheduler::LoadBalance() {
        auto &thread{state.thread};
        auto currentCore{&cores.at(thread->coreId)};

        if (!currentCore->queue.empty() && thread->affinityMask.count() != 1) {
            // Select core where the current thread will be scheduled the earliest based off average timeslice durations for resident threads
            // There's a preference for the current core as migration isn't free

            size_t minTimeslice{};
            CoreContext *optimalCore{};
            for (auto &candidateCore : cores) {
                if (thread->affinityMask.test(candidateCore.id)) {
                    u64 timeslice{};

                    if (!candidateCore.queue.empty()) {
                        std::shared_lock lock(candidateCore.mutex);

                        auto threadIterator{candidateCore.queue.cbegin()};
                        if (threadIterator != candidateCore.queue.cend()) {
                            const auto &runningThread{*threadIterator};
                            timeslice += runningThread->averageTimeslice ? std::min(runningThread->averageTimeslice - (util::GetTimeTicks() - runningThread->timesliceStart), 1UL) : runningThread->timesliceStart ? util::GetTimeTicks() - runningThread->timesliceStart : 1UL;

                            while (++threadIterator != candidateCore.queue.cend()) {
                                const auto &residentThread{*threadIterator};
                                if (residentThread->priority <= thread->priority)
                                    timeslice += residentThread->averageTimeslice ? residentThread->averageTimeslice : 1UL;
                            }
                        }
                    }

                    if (!optimalCore || timeslice < minTimeslice || (timeslice == minTimeslice && &candidateCore == currentCore)) {
                        optimalCore = &candidateCore;
                        minTimeslice = timeslice;
                    }
                }
            }

            if (optimalCore != currentCore) {
                std::unique_lock lock(currentCore->mutex);
                currentCore->queue.erase(std::remove(currentCore->queue.begin(), currentCore->queue.end(), thread), currentCore->queue.end());
                currentCore->mutateCondition.notify_all();

                thread->coreId = optimalCore->id;

                state.logger->Debug("Load Balancing for #{}: C{} -> C{}", thread->id, currentCore->id, optimalCore->id);
            } else {
                state.logger->Debug("Load Balancing for #{}: C{} (Late)", thread->id, currentCore->id);
            }

            return *optimalCore;
        }

        state.logger->Debug("Load Balancing for #{}: C{} (Early)", thread->id, currentCore->id);

        return *currentCore;
    }

    void Scheduler::InsertThread(bool loadBalance) {
        auto &thread{state.thread};
        auto &core{loadBalance ? LoadBalance() : cores.at(thread->coreId)};

        std::unique_lock lock(core.mutex);
        auto nextThread{std::find_if(core.queue.begin(), core.queue.end(), [&](const std::shared_ptr<type::KThread> &it) { return it->priority > thread->priority; })};
        if (nextThread == core.queue.begin() && nextThread != core.queue.end()) {
            throw exception("Migration Interrupt Required");
        } else {
            core.queue.insert(nextThread, thread);
        }

        core.mutateCondition.notify_all();
    }

    void Scheduler::WaitSchedule() {
        auto &thread{state.thread};
        auto *core{&cores.at(thread->coreId)};

        std::shared_lock lock(core->mutex);
        if (thread->affinityMask.count() > 1) {
            std::chrono::milliseconds loadBalanceThreshold{1}; //!< The amount of time that needs to pass unscheduled for a thread to attempt load balancing
            while (!core->mutateCondition.wait_for(lock, loadBalanceThreshold, [&]() { return core->queue.front() == thread; })) {
                lock.unlock();
                LoadBalance();
                if (thread->coreId == core->id) {
                    lock.lock();
                } else {
                    InsertThread(false);
                    core = &cores.at(thread->coreId);
                    lock = std::shared_lock(core->mutex);
                }

                loadBalanceThreshold *= 2; // We double the duration required for future load balancing for this invocation to minimize pointless load balancing
            }
        } else {
            core->mutateCondition.wait(lock, [&]() { return core->queue.front() == thread; });
        }

        thread->timesliceStart = util::GetTimeTicks();
    }

    void Scheduler::Rotate() {
        auto &thread{state.thread};
        auto &core{cores.at(thread->coreId)};

        std::unique_lock lock(core.mutex);
        if (core.queue.front() == thread) {
            thread->averageTimeslice = (thread->averageTimeslice / 4) + (3 * (util::GetTimeTicks() - thread->timesliceStart / 4)); // 0.25 * old timeslice duration + 0.75 * current timeslice duration

            core.queue.pop_front();
            core.queue.push_back(thread);

            core.mutateCondition.notify_all();
        }
    }

    void Scheduler::RemoveThread() {
        auto &thread{state.thread};
        auto &core{cores.at(thread->coreId)};

        std::unique_lock lock(core.mutex);
        core.queue.erase(std::remove(core.queue.begin(), core.queue.end(), thread), core.queue.end());
    }
}
