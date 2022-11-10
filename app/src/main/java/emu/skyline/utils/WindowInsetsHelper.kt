/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.utils

import android.view.View
import android.view.ViewGroup
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updateLayoutParams

/**
 * An interface to easily add window insets handling to any layout
 */
interface WindowInsetsHelper {
    companion object {
        /**
         * Convenience method to setup insets for most Activities with a single call
         * @param rootView The root view of the layout
         * @param listView The list view of the layout
         */
        fun applyToActivity(rootView : View, listView : View? = null) {
            // Apply margin to the root view to avoid overlapping system bar in landscape mode
            // Don't consume insets in the root view so that child views can apply them
            setMargin(rootView, consume = false, left = true, right = true)

            // Apply padding to the list view to avoid navigation bar overlap at the bottom
            listView?.let { addPadding(it, bottom = true) }
        }

        fun setMargin(view : View, consume : Boolean = true, left : Boolean = false, top : Boolean = false, right : Boolean = false, bottom : Boolean = false) {
            ViewCompat.setOnApplyWindowInsetsListener(view) { v, windowInsets ->
                val insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
                v.updateLayoutParams<ViewGroup.MarginLayoutParams> {
                    if (left) leftMargin = insets.left
                    if (top) topMargin = insets.top
                    if (right) rightMargin = insets.right
                    if (bottom) bottomMargin = insets.bottom
                }
                if (consume) WindowInsetsCompat.CONSUMED else windowInsets
            }
        }

        fun addMargin(view : View, consume : Boolean = true, left : Boolean = false, top : Boolean = false, right : Boolean = false, bottom : Boolean = false) {
            ViewCompat.setOnApplyWindowInsetsListener(view) { v, windowInsets ->
                val insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
                v.updateLayoutParams<ViewGroup.MarginLayoutParams> {
                    if (left) leftMargin += insets.left
                    if (top) topMargin += insets.top
                    if (right) rightMargin += insets.right
                    if (bottom) bottomMargin += insets.bottom
                }
                if (consume) WindowInsetsCompat.CONSUMED else windowInsets
            }
        }

        fun setPadding(view : View, consume : Boolean = true, left : Boolean = false, top : Boolean = false, right : Boolean = false, bottom : Boolean = false) {
            ViewCompat.setOnApplyWindowInsetsListener(view) { v, windowInsets ->
                val insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
                v.setPadding(
                    if (left) insets.left else v.paddingLeft,
                    if (top) insets.top else v.paddingTop,
                    if (right) insets.right else v.paddingRight,
                    if (bottom) insets.bottom else v.paddingBottom
                )
                if (consume) WindowInsetsCompat.CONSUMED else windowInsets
            }
        }

        fun addPadding(view : View, consume : Boolean = true, left : Boolean = false, top : Boolean = false, right : Boolean = false, bottom : Boolean = false) {
            ViewCompat.setOnApplyWindowInsetsListener(view) { v, windowInsets ->
                val insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
                v.setPadding(
                    if (left) insets.left + v.paddingLeft else v.paddingLeft,
                    if (top) insets.top + v.paddingTop else v.paddingTop,
                    if (right) insets.right + v.paddingRight else v.paddingRight,
                    if (bottom) insets.bottom + v.paddingBottom else v.paddingBottom
                )
                if (consume) WindowInsetsCompat.CONSUMED else windowInsets
            }
        }
    }
}
