#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>

namespace skyline::service::audio {
    /**
     * @brief IAudioRendererManager or audren:u is used to manage audio renderer outputs (https://switchbrew.org/wiki/Audio_services#audren:u)
     */
    class IAudioRendererManager : public BaseService {
      public:
        IAudioRendererManager(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief Creates a new IAudioRenderer object and returns a handle to it
         */
        void OpenAudioRenderer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Calculates the size of the buffer the guest needs to allocate for IAudioRendererManager
         */
        void GetAudioRendererWorkBufferSize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
