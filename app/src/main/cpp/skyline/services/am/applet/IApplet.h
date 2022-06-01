// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <queue>
#include <services/am/storage/IStorage.h>
#include <kernel/types/KEvent.h>
#include <services/applet/common_arguments.h>


namespace skyline::service::am {
    /**
     * @brief The base class all Applets have to inherit from
     */
    class IApplet : public BaseService {
      private:
        std::shared_ptr<kernel::type::KEvent> onNormalDataPushFromApplet;
        std::shared_ptr<kernel::type::KEvent> onInteractiveDataPushFromApplet;
        std::mutex outputDataMutex;
        std::queue<std::shared_ptr<IStorage>> normalOutputData; //!< Stores data sent by the applet so the guest can read it when it needs to
        std::mutex interactiveOutputDataMutex;
        std::queue<std::shared_ptr<IStorage>> interactiveOutputData; //!< Stores interactive data sent by the applet so the guest can read it when it needs to

      protected:
        std::shared_ptr<kernel::type::KEvent> onAppletStateChanged;

        /**
         * @brief Utility to send data to the guest and trigger the onNormalDataPushFromApplet event
         */
        void PushNormalDataAndSignal(std::shared_ptr<IStorage> data);

        /**
         * @brief Utility to send data to the guest and trigger the onInteractiveDataPushFromApplet event
         */
        void PushInteractiveDataAndSignal(std::shared_ptr<IStorage> data);

      public:
        IApplet(const DeviceState &state, ServiceManager &manager, std::shared_ptr<kernel::type::KEvent> onAppletStateChanged, std::shared_ptr<kernel::type::KEvent> onNormalDataPushFromApplet, std::shared_ptr<kernel::type::KEvent> onInteractiveDataPushFromApplet, applet::LibraryAppletMode appletMode);

        virtual ~IApplet();

        /**
         * @brief Called when the applet is started
         */
        virtual Result Start() = 0;

        /**
         * @brief Called when the applet is stopped
         */
        virtual Result GetResult() = 0;

        /**
         * @brief Called when data is pushed to the applet by the guest through the normal queue
         */
        virtual void PushNormalDataToApplet(std::shared_ptr<IStorage> data) = 0;

        /**
         * @brief Called when data is pushed to the applet by the guest through the interactive queue
         */
        virtual void PushInteractiveDataToApplet(std::shared_ptr<IStorage> data) = 0;

        /**
         * @brief Used by ILibraryAppletAccessor to pop data from the normal queue and reset the corresponding event
         */
        std::shared_ptr<IStorage> PopNormalAndClear();

        /**
         * @brief Used by ILibraryAppletAccessor to pop data from the interactive queue and reset the corresponding event
         */
        std::shared_ptr<IStorage> PopInteractiveAndClear();
    };
}
