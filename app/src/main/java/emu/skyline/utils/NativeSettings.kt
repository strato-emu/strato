/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.utils

/**
 * The settings that will be passed to libskyline when running and executable
 */
class NativeSettings(pref: PreferenceSettings) {
    var isDocked : Boolean = pref.isDocked
    var usernameValue : String = pref.usernameValue
    var systemLanguage : Int = pref.systemLanguage
    var forceTripleBuffering : Boolean = pref.forceTripleBuffering
    var disableFrameThrottling : Boolean = pref.disableFrameThrottling

    /**
     * Updates settings in libskyline during emulation
     */
    external fun updateNative()

    companion object {
        /**
         * Sets libskyline logger level to the given one
         */
        external fun setLogLevel(logLevel : Int)
    }
}
