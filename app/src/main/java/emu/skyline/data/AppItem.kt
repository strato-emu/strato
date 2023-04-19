/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.data

import android.content.Context
import android.graphics.Bitmap
import androidx.core.content.ContextCompat
import androidx.core.graphics.drawable.toBitmap
import emu.skyline.BuildConfig
import emu.skyline.R
import emu.skyline.SkylineApplication
import emu.skyline.loader.AppEntry
import emu.skyline.loader.LoaderResult
import java.io.Serializable

/**
 * The tag used to pass [AppItem]s between activities and fragments
 */
const val AppItemTag = BuildConfig.APPLICATION_ID + ".APP_ITEM"

private val missingIcon by lazy { ContextCompat.getDrawable(SkylineApplication.instance, R.drawable.default_icon)!!.toBitmap(256, 256) }

/**
 * This class is a wrapper around [AppEntry], it is used for passing around game metadata
 */
@Suppress("SERIAL")
data class AppItem(private val meta : AppEntry) : Serializable {
    /**
     * The icon of the application
     */
    val icon get() = meta.icon

    val bitmapIcon : Bitmap get() = meta.icon ?: missingIcon

    /**
     * The title of the application
     */
    val title get() = meta.name

    /**
     * The title ID of the application
     */
    val titleId get() = meta.titleId

    /**
     * The application version
     */
    val version get() = meta.version

    /**
     * The application author
     */
    val author get() = meta.author

    /**
     * The URI of the application's image file
     */
    val uri get() = meta.uri

    /**
     * The format of the application
     */
    val format get() = meta.format

    val loaderResult get() = meta.loaderResult

    fun loaderResultString(context : Context) = context.getString(
        when (meta.loaderResult) {
            LoaderResult.Success -> R.string.metadata_missing

            LoaderResult.ParsingError -> R.string.invalid_file

            LoaderResult.MissingTitleKey -> R.string.missing_title_key

            LoaderResult.MissingHeaderKey,
            LoaderResult.MissingTitleKek,
            LoaderResult.MissingKeyArea -> R.string.incomplete_prod_keys
        }
    )

    /**
     * The name and author is used as the key
     */
    fun key() = "${meta.name}${meta.author.let { it ?: "" }}"
}
