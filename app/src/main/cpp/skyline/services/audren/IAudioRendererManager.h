#pragma once

#include <audio.h>
#include <services/base_service.h>
#include <services/serviceman.h>
#include <kernel/types/KEvent.h>

namespace skyline::service::audren {
    /**
     * @brief audren:u or IAudioRendererManager is used to manage audio renderer outputs (https://switchbrew.org/wiki/Audio_services#audren:u)
     */
    class IAudioRendererManager : public BaseService {
      public:
        IAudioRendererManager(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief Creates a new audrenU::IAudioRenderer object and returns a handle to it
         */
        void OpenAudioRenderer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Calculates the size of the buffer the guest needs to allocate for audren
         */
        void GetAudioRendererWorkBufferSize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
