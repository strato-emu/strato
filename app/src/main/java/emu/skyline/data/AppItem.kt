/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.data

import android.graphics.Bitmap
import android.net.Uri
import emu.skyline.loader.AppEntry

/**
 * This class is a wrapper around [AppEntry], it is used for passing around game metadata
 */
class AppItem(val meta : AppEntry) : BaseItem() {
    /**
     * The icon of the application
     */
    val icon : Bitmap?
        get() = meta.icon

    /**
     * The title of the application
     */
    val title : String
        get() = meta.name

    /**
     * The string used as the sub-title, we currently use the author
     */
    val subTitle : String?
        get() = meta.author

    /**
     * The URI of the application's image file
     */
    val uri : Uri
        get() = meta.uri

    /**
     * The format of the application ROM as a string
     */
    private val type : String
        get() = meta.format.name

    /**
     * The name and author is used as the key
     */
    override fun key() : String? {
        return if (meta.author != null) meta.name + " " + meta.author else meta.name
    }
}
