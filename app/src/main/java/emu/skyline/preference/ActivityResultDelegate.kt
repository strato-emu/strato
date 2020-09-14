/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.preference

import android.content.Intent

/**
 * Some preferences need results from activities, this delegates the results to them
 */
interface ActivityResultDelegate {
    var requestCode : Int

    fun onActivityResult(requestCode : Int, resultCode : Int, data : Intent?)
}
