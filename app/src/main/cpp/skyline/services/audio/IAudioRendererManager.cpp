// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IAudioRenderer/IAudioRenderer.h"
#include "IAudioDevice.h"
#include "IAudioRendererManager.h"

namespace skyline::service::audio {
    IAudioRendererManager::IAudioRendererManager(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result IAudioRendererManager::OpenAudioRenderer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        IAudioRenderer::AudioRendererParameters params{request.Pop<IAudioRenderer::AudioRendererParameters>()};

        state.logger->Debug("Opening a rev {} IAudioRenderer with sample rate: {}, voice count: {}, effect count: {}", IAudioRenderer::ExtractVersionFromRevision(params.revision), params.sampleRate, params.voiceCount, params.effectCount);

        manager.RegisterService(std::make_shared<IAudioRenderer::IAudioRenderer>(state, manager, params), session, response);

        return {};
    }

    Result IAudioRendererManager::GetAudioRendererWorkBufferSize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        IAudioRenderer::AudioRendererParameters params{request.Pop<IAudioRenderer::AudioRendererParameters>()};

        IAudioRenderer::RevisionInfo revisionInfo{};
        revisionInfo.SetUserRevision(params.revision);

        u32 totalMixCount{params.subMixCount + 1};

        i64 size{util::AlignUp(params.mixBufferCount * 4, constant::BufferAlignment) +
            params.subMixCount * 0x400 +
            totalMixCount * 0x940 +
            params.voiceCount * 0x3F0 +
            util::AlignUp(totalMixCount * 8, 16) +
            util::AlignUp(params.voiceCount * 8, 16) +
            util::AlignUp(((params.sinkCount + params.subMixCount) * 0x3C0 + params.sampleCount * 4) * (params.mixBufferCount + 6), constant::BufferAlignment) + (params.sinkCount + params.subMixCount) * 0x2C0 + (params.effectCount + params.voiceCount * 4) * 0x30 + 0x50};

        if (revisionInfo.SplitterSupported()) {
            i32 nodeStateWorkSize{util::AlignUp<i32>(totalMixCount, constant::BufferAlignment)};
            if (nodeStateWorkSize < 0)
                nodeStateWorkSize |= 7;

            nodeStateWorkSize = 4 * (totalMixCount * totalMixCount) + 12 * totalMixCount + 2 * (nodeStateWorkSize / 8);

            i32 edgeMatrixWorkSize{util::AlignUp<i32>(totalMixCount * totalMixCount, constant::BufferAlignment)};
            if (edgeMatrixWorkSize < 0)
                edgeMatrixWorkSize |= 7;

            edgeMatrixWorkSize /= 8;
            size += util::AlignUp(edgeMatrixWorkSize + nodeStateWorkSize, 16);
        }

        i64 splitterWorkSize{};
        if (revisionInfo.SplitterSupported()) {
            splitterWorkSize += params.splitterDestinationDataCount * 0xE0 + params.splitterCount * 0x20;

            if (revisionInfo.SplitterBugFixed())
                splitterWorkSize += util::AlignUp(4 * params.splitterDestinationDataCount, 16);
        }

        size = params.sinkCount * 0x170 + (params.sinkCount + params.subMixCount) * 0x280 + params.effectCount * 0x4C0 + ((size + splitterWorkSize + 0x30 * params.effectCount + (4 * params.voiceCount) + 0x8F) & ~0x3FL) + ((params.voiceCount << 8) | 0x40);

        if (params.performanceManagerCount > 0) {
            i64 performanceMetricsBufferSize{};
            if (revisionInfo.UsesPerformanceMetricDataFormatV2()) {
                performanceMetricsBufferSize = (params.voiceCount + params.effectCount + totalMixCount + params.sinkCount) + 0x990;
            } else {
                performanceMetricsBufferSize = ((static_cast<i64>((params.voiceCount + params.effectCount + totalMixCount + params.sinkCount)) << 32) >> 0x1C) + 0x658;
            }

            size += (performanceMetricsBufferSize * (params.performanceManagerCount + 1) + 0xFF) & ~0x3FL;
        }

        if (revisionInfo.VaradicCommandBufferSizeSupported()) {
            size += params.effectCount * 0x840 + params.subMixCount * 0x5A38 + params.sinkCount * 0x148 + params.splitterDestinationDataCount * 0x540 + (params.splitterCount * 0x68 + 0x2E0) * params.voiceCount + ((params.voiceCount + params.subMixCount + params.effectCount + params.sinkCount + 0x65) << 6) + 0x3F8 + 0x7E;
        } else {
            size += 0x1807E;
        }

        size = util::AlignUp(size, 0x1000);

        state.logger->Debug("Work buffer size: 0x{:X}", size);
        response.Push<i64>(size);
        return {};
    }

    Result IAudioRendererManager::GetAudioDeviceService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IAudioDevice), session, response);
        return {};
    }
}
