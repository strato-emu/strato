/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.data

import android.content.Context
import android.graphics.Bitmap
import android.net.Uri
import emu.skyline.R
import emu.skyline.loader.AppEntry
import emu.skyline.loader.LoaderResult

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

    val loaderResult get() = meta.loaderResult

    fun loaderResultString(context : Context) = context.getString(when (meta.loaderResult) {
        LoaderResult.Success -> R.string.metadata_missing

        LoaderResult.ParsingError -> R.string.invalid_file

        LoaderResult.MissingTitleKey -> R.string.missing_title_key

        LoaderResult.MissingHeaderKey,
        LoaderResult.MissingTitleKek,
        LoaderResult.MissingKeyArea -> R.string.incomplete_prod_keys
    })

    /**
     * The name and author is used as the key
     */
    override fun key() : String? {
        return if (meta.author != null) meta.name + " " + meta.author else meta.name
    }
}
