// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Copyright © 2022 Ryujinx Team and Contributors (https://github.com/Ryujinx/)

#include <soc.h>
#include "fermi_2d.h"

namespace skyline::soc::gm20b::engine::fermi2d {
    Fermi2D::Fermi2D(const DeviceState &state, ChannelContext &channelCtx, MacroState &macroState)
        : MacroEngineBase{macroState},
          syncpoints{state.soc->host1x.syncpoints},
          interconnect{*state.gpu, channelCtx},
          channelCtx{channelCtx} {}

    void Fermi2D::HandleMethod(u32 method, u32 argument) {
        registers.raw[method] = argument;

        if (method == ENGINE_STRUCT_OFFSET(pixelsFromMemory, trigger)) {
            // Example user code for this method: https://github.com/devkitPro/deko3d/blob/8ee30005cf6d24d081800ee3820810290fffbb09/source/dk_image.cpp#L513

            auto &src{*registers.src};
            auto &dst{*registers.dst};
            auto &pixelsFromMemory{*registers.pixelsFromMemory};

            if (src.layer != 0 || dst.layer != 0)
                Logger::Warn("Blits between layers are unimplemented!");

            if (pixelsFromMemory.safeOverlap)
                Logger::Warn("Safe overlap is unimplemented!");

            auto fixedToFloating{[](i64 value) {
                constexpr u32 FractionalComponentSize{32};

                return static_cast<float>(value) / (1ULL << FractionalComponentSize);
            }};

            // The 2D engine supports subpixel blit precision in the lower 32 bits of the src{X,Y}0 registers for filtering, we can safely ignore this in most cases though since the host driver will handle this in its own way
            float srcX{fixedToFloating(pixelsFromMemory.srcX)};
            float srcY{fixedToFloating(pixelsFromMemory.srcY)};

            float duDx{fixedToFloating(pixelsFromMemory.duDx)};
            float dvDy{fixedToFloating(pixelsFromMemory.dvDy)};

            if (registers.pixelsFromMemory->sampleMode.origin == type::SampleModeOrigin::Center) {
                // This is an MSAA resolve operation, sampling from the center of each pixel in order to resolve the final image from the MSAA samples
                // MSAA images are stored in memory like regular images but each pixels dimensions are scaled: e.g for 2x2 MSAA
                /* 112233
                   112233
                   445566
                   445566 */
                // These would be sampled with both duDx and duDy as 2 (integer part), resolving to the following:
                /* 123
                   456 */

                // Since we don't implement MSAA, we can avoid any scaling at all by setting using a scale factor of 1
                duDx = dvDy = 1.0f;
            }

            interconnect.Blit(src, dst,
                              srcX, srcY,
                              pixelsFromMemory.dstWidth, pixelsFromMemory.dstHeight,
                              pixelsFromMemory.dstX0, pixelsFromMemory.dstY0,
                              duDx, dvDy,
                              registers.pixelsFromMemory->sampleMode.origin,
                              registers.pixelsFromMemory->sampleMode.origin == type::SampleModeOrigin::Center,
                              pixelsFromMemory.sampleMode.filter);
        }
    }

    void Fermi2D::CallMethodFromMacro(u32 method, u32 argument) {
        HandleMethod(method, argument);
    }

    u32 Fermi2D::ReadMethodFromMacro(u32 method) {
        return registers.raw[method];
    }

    __attribute__((always_inline)) void Fermi2D::CallMethod(u32 method, u32 argument) {
        Logger::Verbose("Called method in Fermi 2D: 0x{:X} args: 0x{:X}", method, argument);

        HandleMethod(method, argument);
    }
}
