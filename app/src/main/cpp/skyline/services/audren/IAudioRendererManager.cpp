#include "IAudioRenderer.h"
#include "IAudioRendererManager.h"
#include <kernel/types/KProcess.h>

namespace skyline::service::audren {
    IAudioRendererManager::IAudioRendererManager(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, Service::IAudioRendererManager, "IAudioRendererManager", {
        {0x0, SFUNC(IAudioRendererManager::OpenAudioRenderer)},
        {0x1, SFUNC(IAudioRendererManager::GetAudioRendererWorkBufferSize)}
    }) {}

    void IAudioRendererManager::OpenAudioRenderer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        AudioRendererParams params = request.Pop<AudioRendererParams>();

        state.logger->Debug("IAudioRendererManager: Opening a rev {} IAudioRenderer with sample rate: {}, voice count: {}, effect count: {}",
            ExtractVersionFromRevision(params.revision), params.sampleRate, params.voiceCount, params.effectCount);

        manager.RegisterService(std::make_shared<IAudioRenderer>(state, manager, params), session, response);

    }

    void IAudioRendererManager::GetAudioRendererWorkBufferSize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        AudioRendererParams params = request.Pop<AudioRendererParams>();

        RevisionInfo revisionInfo;
        revisionInfo.SetUserRevision(params.revision);

        u32 totalMixCount = params.subMixCount + 1;

        i64 size = utils::AlignUp(params.mixBufferCount * 4, constant::BufferAlignment) +
            params.subMixCount * 0x400 +
            totalMixCount * 0x940 +
            params.voiceCount * 0x3F0 +
            utils::AlignUp(totalMixCount * 8, 16) +
            utils::AlignUp(params.voiceCount * 8, 16) +
            utils::AlignUp(((params.sinkCount + params.subMixCount) * 0x3C0 + params.sampleCount * 4) * (params.mixBufferCount + 6), constant::BufferAlignment) +
            (params.sinkCount + params.subMixCount) * 0x2C0 +
            (params.effectCount + params.voiceCount * 4) * 0x30 +
            0x50;

        if (revisionInfo.SplitterSupported()) {
            i32 nodeStateWorkSize = utils::AlignUp(totalMixCount, constant::BufferAlignment);

            if (nodeStateWorkSize < 0)
                nodeStateWorkSize |= 7;

            nodeStateWorkSize = 4 * (totalMixCount * totalMixCount) + 12 * totalMixCount + 2 * (nodeStateWorkSize / 8);

            i32 edgeMatrixWorkSize = utils::AlignUp(totalMixCount * totalMixCount, constant::BufferAlignment);

            if (edgeMatrixWorkSize < 0)
                edgeMatrixWorkSize |= 7;

            edgeMatrixWorkSize /= 8;
            size += utils::AlignUp(edgeMatrixWorkSize + nodeStateWorkSize, 16);
        }

        i64 splitterWorkSize = 0;

        if (revisionInfo.SplitterSupported()) {
            splitterWorkSize += params.splitterDestinationDataCount * 0xE0 +
                params.splitterCount * 0x20;

            if (revisionInfo.SplitterBugFixed())
                splitterWorkSize += utils::AlignUp(4 * params.splitterDestinationDataCount, 16);
        }
        size = params.sinkCount * 0x170 +
            (params.sinkCount + params.subMixCount) * 0x280 +
            params.effectCount * 0x4C0 +
            ((size + splitterWorkSize + 0x30 * params.effectCount + (4 * params.voiceCount) + 0x8F) & ~0x3Fl) +
            ((params.voiceCount << 8) | 0x40);

        if (params.performanceManagerCount > 0) {
            i64 performanceMetricsBufferSize;

            if (revisionInfo.UsesPerformanceMetricDataFormatV2()) {
                performanceMetricsBufferSize = (params.voiceCount +
                    params.effectCount +
                    totalMixCount +
                    params.sinkCount) + 0x990;
            } else {
                performanceMetricsBufferSize = ((static_cast<i64>((params.voiceCount +
                    params.effectCount +
                    totalMixCount +
                    params.sinkCount)) << 32) >> 0x1C) + 0x658;
            }

            size += (performanceMetricsBufferSize * (params.performanceManagerCount + 1) + 0xFF) & ~0x3Fl;
        }

        if (revisionInfo.VaradicCommandBufferSizeSupported()) {
            size += params.effectCount * 0x840 +
                params.subMixCount * 0x5A38 +
                params.sinkCount * 0x148 +
                params.splitterDestinationDataCount * 0x540 +
                (params.splitterCount * 0x68 + 0x2E0) * params.voiceCount +
                ((params.voiceCount + params.subMixCount + params.effectCount + params.sinkCount + 0x65) << 6) +
                0x3F8 +
                0x7E;
        } else {
            size += 0x1807E;
        }

        size = utils::AlignUp(size, 0x1000);

        state.logger->Debug("IAudioRendererManager: Work buffer size: 0x{:X}", size);
        response.Push<i64>(size);
    }
}
