// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Copyright © 2022 Ryujinx Team and Contributors (https://github.com/Ryujinx/)

#include <soc.h>
#include "fermi_2d.h"

namespace skyline::soc::gm20b::engine::fermi2d {
    Fermi2D::Fermi2D(const DeviceState &state, ChannelContext &channelCtx, MacroState &macroState, gpu::interconnect::CommandExecutor &executor)
        : MacroEngineBase(macroState),
          syncpoints(state.soc->host1x.syncpoints),
          context(*state.gpu, channelCtx, executor),
          channelCtx(channelCtx) {}

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

            constexpr u32 FractionalComponentSize{32};

            // The 2D engine supports subpixel blit precision in the lower 32 bits of the src{X,Y}0 registers for filtering, we can safely ignore this in most cases though since the host driver will handle this in its own way
            i32 srcX{static_cast<i32>(pixelsFromMemory.srcX0 >> FractionalComponentSize)};
            i32 srcY{static_cast<i32>(pixelsFromMemory.srcY0 >> FractionalComponentSize)};

            i32 srcWidth{static_cast<i32>((pixelsFromMemory.duDx * pixelsFromMemory.dstWidth) >> FractionalComponentSize)};
            i32 srcHeight{static_cast<i32>((pixelsFromMemory.dvDy * pixelsFromMemory.dstHeight) >> FractionalComponentSize)};

            if (registers.pixelsFromMemory->sampleMode.origin == Registers::PixelsFromMemory::SampleModeOrigin::Center) {
                // This is an MSAA resolve operation, sampling from the center of each pixel in order to resolve the final image from the MSAA samples
                // MSAA images are stored in memory like regular images but each pixels dimensions are scaled: e.g for 2x2 MSAA
                /* 112233
                   112233
                   445566
                   445566 */
                // These would be sampled with both duDx and duDy as 2 (integer part), resolving to the following:
                /* 123
                   456 */

                // Since we don't implement MSAA, any image that is supposed to have MSAA applied when drawing is just stored in the corner without any pixel scaling, so adjust width/height appropriately
                srcWidth = pixelsFromMemory.dstWidth;
                srcHeight = pixelsFromMemory.dstHeight;
            } else {
                // This is a regular blit operation, scaling from one image to another
                // https://github.com/Ryujinx/Ryujinx/blob/c9c65af59edea05e7206a076cb818128c004384e/Ryujinx.Graphics.Gpu/Engine/Twod/TwodClass.cs#L253
                srcX -= (pixelsFromMemory.duDx >> FractionalComponentSize) >> 1;
                srcY -= (pixelsFromMemory.dvDy >> FractionalComponentSize) >> 1;
            }

            context.Blit(src, dst,
                         srcX, srcY,
                         srcWidth, srcHeight,
                         pixelsFromMemory.dstX0, pixelsFromMemory.dstY0,
                         pixelsFromMemory.dstWidth, pixelsFromMemory.dstHeight,
                         registers.pixelsFromMemory->sampleMode.origin == Registers::PixelsFromMemory::SampleModeOrigin::Center,
                         pixelsFromMemory.sampleMode.filter == Registers::PixelsFromMemory::SampleModeFilter::Bilinear);
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
