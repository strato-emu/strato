// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <queue>
#include <common.h>
#include "syncpoint.h"
#include "classes/class.h"

namespace skyline::soc::host1x {
    /**
     * @brief The 'Tegra Host Interface' or THI sits inbetween the Host1x and the class falcons, implementing syncpoint queueing and a method interface
     */
    template<typename ClassType>
    class TegraHostInterface {
      private:
        SyncpointSet &syncpoints;
        ClassType deviceClass; //!< The device class behind the THI, such as NVDEC or VIC

        u32 storedMethod{}; //!< Method that will be used for deviceClass.CallMethod, set using Method0

        std::queue<u32> incrQueue; //!< Queue of syncpoint IDs to be incremented when a device operation is finished, the same syncpoint may be held multiple times within the queue
        std::mutex incrMutex;

        void AddIncr(u32 syncpointId) {
            std::scoped_lock lock(incrMutex);
            incrQueue.push(syncpointId);
        }

        void SubmitPendingIncrs() {
            std::scoped_lock lock(incrMutex);

            while (!incrQueue.empty()) {
                u32 syncpointId{incrQueue.front()};
                incrQueue.pop();

                Logger::Debug("Increment syncpoint: {}", syncpointId);
                syncpoints.at(syncpointId).Increment();
            }
        }

      public:
        TegraHostInterface(SyncpointSet &syncpoints)
            : deviceClass([&] { SubmitPendingIncrs(); }),
              syncpoints(syncpoints) {}

        void CallMethod(u32 method, u32 argument)  {
            constexpr u32 Method0MethodId{0x10}; //!< Sets the method to be called on the device class upon a call to Method1, see TRM '15.5.6 NV_PVIC_THI_METHOD0'
            constexpr u32 Method1MethodId{0x11}; //!< Calls the method set by Method1 with the supplied argument, see TRM '15.5.7 NV_PVIC_THI_METHOD1"

            switch (method) {
                case IncrementSyncpointMethodId: {
                    IncrementSyncpointMethod incrSyncpoint{.raw = argument};

                    switch (incrSyncpoint.condition) {
                        case IncrementSyncpointMethod::Condition::Immediate:
                            Logger::Debug("Increment syncpoint: {}", incrSyncpoint.index);
                            syncpoints.at(incrSyncpoint.index).Increment();
                            break;
                        case IncrementSyncpointMethod::Condition::OpDone:
                            Logger::Debug("Queue syncpoint for OpDone: {}", incrSyncpoint.index);
                            AddIncr(incrSyncpoint.index);
                            SubmitPendingIncrs(); // FIXME: immediately submit the incrs as classes are not yet implemented
                            break;
                        default:
                            Logger::Warn("Unimplemented syncpoint condition: {}", static_cast<u8>(incrSyncpoint.condition));
                            break;
                    }
                    break;
                }
                case Method0MethodId:
                    storedMethod = argument;
                    break;
                case Method1MethodId:
                    deviceClass.CallMethod(storedMethod, argument);
                    break;
                default:
                    Logger::Error("Unknown THI method called: 0x{:X}, argument: 0x{:X}", method, argument);
                    break;
            }
        }
    };
}
