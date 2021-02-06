// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>

namespace skyline::service::audio::IAudioRenderer {
    enum class EffectState : u8 {
        None = 0, //!< The effect isn't being used
        New = 1,
    };

    /**
     * @brief Input containing information on what effects to use on an audio stream
     */
    struct EffectIn {
        u8 _unk0_;
        u8 isNew; //!< Whether the effect was used in the previous samples
        u8 _unk1_[0xBE];
    };
    static_assert(sizeof(EffectIn) == 0xC0);

    /**
     * @brief Returned to inform the guest of the state of an effect
     */
    struct EffectOut {
        EffectState state;
        u8 _pad0_[15];
    };
    static_assert(sizeof(EffectOut) == 0x10);

    /**
     * @brief The Effect class stores the state of audio post processing effects
     */
    class Effect {
      public:
        EffectOut output{};

        void ProcessInput(const EffectIn &input) {
            if (input.isNew)
                output.state = EffectState::New;
        }
    };
}
