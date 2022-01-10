/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.utils

import java.io.Serializable

class SettingsValues(pref: Settings) : Serializable {
    var isDocked : Boolean = pref.isDocked
    var usernameValue : String = pref.usernameValue
    var systemLanguage : Int = pref.systemLanguage
    var forceTripleBuffering : Boolean = pref.forceTripleBuffering
    var disableFrameThrottling : Boolean = pref.disableFrameThrottling

    /**
     * Updates settings in libskyline during emulation
     */
    external fun updateNative()
}
