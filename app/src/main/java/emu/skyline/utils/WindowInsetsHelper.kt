/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.utils

import android.view.View
import android.view.ViewGroup
import androidx.core.view.*

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
            // Save initial margin values to avoid adding to the initial margin multiple times
            val baseMargin = object {
                val left = view.marginLeft
                val top = view.marginTop
                val right = view.marginRight
                val bottom = view.marginBottom
            }

            ViewCompat.setOnApplyWindowInsetsListener(view) { v, windowInsets ->
                val insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
                v.updateLayoutParams<ViewGroup.MarginLayoutParams> {
                    if (left) leftMargin = baseMargin.left + insets.left
                    if (top) topMargin = baseMargin.top + insets.top
                    if (right) rightMargin = baseMargin.right + insets.right
                    if (bottom) bottomMargin = baseMargin.bottom + insets.bottom
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

        private fun addPadding(view : View, consume : Boolean = true, left : Boolean = false, top : Boolean = false, right : Boolean = false, bottom : Boolean = false) {
            // Save initial padding values to avoid adding to the initial padding multiple times
            val basePadding = object {
                val left = view.paddingLeft
                val top = view.paddingTop
                val right = view.paddingRight
                val bottom = view.paddingBottom
            }

            ViewCompat.setOnApplyWindowInsetsListener(view) { v, windowInsets ->
                val insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
                v.setPadding(
                    if (left) basePadding.left + insets.left else basePadding.left,
                    if (top) basePadding.top + insets.top else basePadding.top,
                    if (right) basePadding.right + insets.right else basePadding.right,
                    if (bottom) basePadding.bottom + insets.bottom else basePadding.bottom
                )
                if (consume) WindowInsetsCompat.CONSUMED else windowInsets
            }
        }
    }
}
