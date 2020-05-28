/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.views

import android.content.Context
import android.graphics.Rect
import android.util.AttributeSet
import android.util.Log
import android.view.View
import android.widget.LinearLayout
import androidx.coordinatorlayout.widget.CoordinatorLayout
import com.google.android.material.snackbar.Snackbar.SnackbarLayout

/**
 * Custom linear layout with support for [CoordinatorLayout] to move children, when [com.google.android.material.snackbar.Snackbar] shows up.
 */
class CustomLinearLayout : LinearLayout, CoordinatorLayout.AttachedBehavior {
    constructor(context : Context) : this(context, null)
    constructor(context : Context, attrs : AttributeSet?) : this(context, attrs, 0)
    constructor(context : Context, attrs : AttributeSet?, defStyleAttr : Int) : super(context, attrs, defStyleAttr)

    override fun getBehavior() : CoordinatorLayout.Behavior<CustomLinearLayout> = MoveUpwardBehavior()

    override fun requestFocus(direction: Int, previouslyFocusedRect: Rect): Boolean = getChildAt(if (direction == View.FOCUS_UP) childCount - 1 else 0 )?.requestFocus() ?: false

    /*
    override fun onRequestFocusInDescendants(dir : Int, rect : Rect?) : Boolean {
        Log.i("DESC", "$dir and $rect")
        return getChildAt(0).requestFocus()
    }
     */

    /**
     * Defines behaviour when [com.google.android.material.snackbar.Snackbar] is shown.
     * Simply sets an offset to y translation to move children out of the way.
     */
    class MoveUpwardBehavior : CoordinatorLayout.Behavior<CustomLinearLayout>() {
        override fun layoutDependsOn(parent : CoordinatorLayout, child : CustomLinearLayout, dependency : View) : Boolean = dependency is SnackbarLayout

        override fun onDependentViewChanged(parent : CoordinatorLayout, child : CustomLinearLayout, dependency : View) : Boolean {
            child.translationY = (0f).coerceAtMost(dependency.translationY - dependency.height)
            return true
        }

        override fun onDependentViewRemoved(parent : CoordinatorLayout, child : CustomLinearLayout, dependency : View) {
            child.translationY = 0f
        }
    }
}
