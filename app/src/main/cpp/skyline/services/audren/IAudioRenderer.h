#pragma once

#include <audio.h>
#include <services/base_service.h>
#include <services/serviceman.h>
#include <kernel/types/KEvent.h>
#include "memoryPool.h"
#include "effect.h"
#include "voice.h"
#include "revisionInfo.h"

namespace skyline::service::audren {
    namespace constant {
        constexpr int BufferAlignment = 0x40; //!< The alignment for all audren buffers
    }
    /**
     * @brief The parameters used to configure an IAudioRenderer
     */
    struct AudioRendererParams {
        u32 sampleRate; //!< The sample rate to use for the renderer
        u32 sampleCount; //!< The buffer sample count
        u32 mixBufferCount; //!< The amount of mix buffers to use
        u32 subMixCount; //!< The amount of sub mixes to use
        u32 voiceCount; //!< The amount of voices to use
        u32 sinkCount; //!< The amount of sinks to use
        u32 effectCount; //!< The amount of effects to use
        u32 performanceManagerCount; //!< The amount of performance managers to use
        u32 voiceDropEnable; //!< Whether to enable voice drop
        u32 splitterCount; //!< The amount of splitters to use
        u32 splitterDestinationDataCount; //!< The amount of splitter destination outputs to use
        u32 _unk0_;
        u32 revision; //!< The revision of audren to use
    };
    static_assert(sizeof(AudioRendererParams) == 0x34);

    /**
     * @brief Header containing information about the software side audren implementation
     */
    struct UpdateDataHeader {
        u32 revision; //!< Revision of the software implementation
        u32 behaviorSize; //!< The total size of the behaviour info
        u32 memoryPoolSize; //!< The total size of all MemoryPoolIn structs
        u32 voiceSize; //!< The total size of all VoiceIn structs
        u32 voiceResourceSize; //!< The total size of the voice resources
        u32 effectSize; //!< The total size of all EffectIn structs
        u32 mixSize; //!< The total size of all mixer descriptors in the input
        u32 sinkSize; //!< The total size of all sink descriptors in the input
        u32 performanceManagerSize; //!< The total size of all performance manager descriptors in the input
        u32 _unk0_;
        u32 elapsedFrameCountInfoSize; //!< The total size of all the elapsed frame info
        u32 _unk1_[4];
        u32 totalSize; //!< The total size of the whole input
    };
    static_assert(sizeof(UpdateDataHeader) == 0x40);

    /**
    * @brief IAudioRenderer is used to control an audio renderer output (https://switchbrew.org/wiki/Audio_services#IAudioRenderer)
    */
    class IAudioRenderer : public BaseService {
      private:
        std::shared_ptr<audio::AudioTrack> track; //!< The audio track associated with the audio renderer
        AudioRendererParams rendererParams; //!< The parameters to use for the renderer
        std::shared_ptr<type::KEvent> releaseEvent; //!< The KEvent that is signalled when a buffer has been released
        std::vector<MemoryPool> memoryPools; //!< An vector of all memory pools that the guest may need
        std::vector<Effect> effects; //!< An vector of all effects that the guest may need
        std::vector<Voice> voices; //!< An vector of all voices that the guest may need
        std::vector<i16> sampleBuffer; //!< The final output data that is appended to the stream
        RevisionInfo revisionInfo{}; //!< Stores info about supported features for the audren revision used

        audio::AudioOutState playbackState{audio::AudioOutState::Stopped}; //!< The current state of playback
        size_t memoryPoolCount{}; //!< The amount of memory pools the guest may need
        int samplesPerBuffer{}; //!< The amount of samples each appended buffer should contain

        /**
         * @brief Obtains new sample data from voices and mixes it together into the sample buffer
         */
        void MixFinalBuffer();

        /**
         * @brief Appends all released buffers with new mixed sample data
         */
        void UpdateAudio();

      public:
        /**
         * @param params The parameters to use for rendering
         */
        IAudioRenderer(const DeviceState &state, ServiceManager &manager, AudioRendererParams &params);

        /**
         * @brief Closes the audio track
         */
        ~IAudioRenderer();

        /**
         * @brief Returns the sample rate (https://switchbrew.org/wiki/Audio_services#GetSampleRate)
         */
        void GetSampleRate(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns the sample count (https://switchbrew.org/wiki/Audio_services#GetSampleCount)
        */
        void GetSampleCount(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
        * @brief Returns the number of mix buffers (https://switchbrew.org/wiki/Audio_services#GetMixBufferCount)
        */
        void GetMixBufferCount(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
        * @brief Returns the state of the renderer (https://switchbrew.org/wiki/Audio_services#GetAudioRendererState) (stubbed)?
        */
        void GetState(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
        * @brief Updates the audio renderer state and appends new data to playback buffers
        */
        void RequestUpdate(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
        * @brief Start the audio stream from the renderer
        */
        void Start(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
        * @brief Stop the audio stream from the renderer
        */
        void Stop(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
        * @brief Returns a handle to the sample release KEvent
        */
        void QuerySystemEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
