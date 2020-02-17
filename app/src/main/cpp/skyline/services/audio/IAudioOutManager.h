#pragma once

#include <kernel/types/KEvent.h>
#include <services/base_service.h>
#include <services/serviceman.h>

namespace skyline::service::audio {
    namespace constant {
        constexpr std::string_view DefaultAudioOutName = "DeviceOut"; //!< The default audio output device name
    };

    /**
     * @brief IAudioOutManager or audout:u is used to manage audio outputs (https://switchbrew.org/wiki/Audio_services#audout:u)
     */
    class IAudioOutManager : public BaseService {
      public:
        IAudioOutManager(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief Returns a list of all available audio outputs (https://switchbrew.org/wiki/Audio_services#ListAudioOuts)
         */
        void ListAudioOuts(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Creates a new audoutU::IAudioOut object and returns a handle to it (https://switchbrew.org/wiki/Audio_services#OpenAudioOut)
         */
        void OpenAudioOut(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
