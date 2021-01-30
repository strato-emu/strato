/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.data

import android.content.Context
import emu.skyline.R
import emu.skyline.loader.AppEntry
import emu.skyline.loader.LoaderResult
import java.io.Serializable

sealed class DataItem : Serializable

class HeaderItem(val title : String) : DataItem()

/**
 * This class is a wrapper around [AppEntry], it is used for passing around game metadata
 */
data class AppItem(private val meta : AppEntry) : DataItem() {
    /**
     * The icon of the application
     */
    val icon get() = meta.icon

    /**
     * The title of the application
     */
    val title get() = meta.name

    /**
     * The string used as the sub-title, we currently use the author
     */
    val subTitle get() = meta.author

    /**
     * The URI of the application's image file
     */
    val uri get() = meta.uri

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
    fun key() = meta.name + if (meta.author != null) " ${meta.author}" else ""
}
