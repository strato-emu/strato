/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.adapter

import android.graphics.Rect
import android.view.View
import androidx.recyclerview.widget.GridLayoutManager
import androidx.recyclerview.widget.RecyclerView

/**
 * A [RecyclerView.ItemDecoration] that adds a margin at the bottom of each item
 */
class SpacingItemDecoration(private val padding : Int) : RecyclerView.ItemDecoration() {
    override fun getItemOffsets(outRect : Rect, view : View, parent : RecyclerView, state : RecyclerView.State) {
        super.getItemOffsets(outRect, view, parent, state)
        outRect.set(0, 0, 0, padding)
    }
}

/**
 * A [RecyclerView.ItemDecoration] that adds a margin to the sides of each item in a grid
 */
class GridSpacingItemDecoration(private val padding : Int) : RecyclerView.ItemDecoration() {
    override fun getItemOffsets(outRect : Rect, view : View, parent : RecyclerView, state : RecyclerView.State) {
        super.getItemOffsets(outRect, view, parent, state)

        val gridLayoutManager = parent.layoutManager as GridLayoutManager
        val layoutParams = view.layoutParams as GridLayoutManager.LayoutParams
        when (layoutParams.spanIndex) {
            0 -> outRect.left = padding

            gridLayoutManager.spanCount - 1 -> outRect.right = padding

            else -> {
                outRect.left = padding / 2
                outRect.right = padding / 2
            }
        }

        if (layoutParams.spanSize == gridLayoutManager.spanCount) {
            outRect.left = 0
            outRect.right = 0
        }
    }
}
