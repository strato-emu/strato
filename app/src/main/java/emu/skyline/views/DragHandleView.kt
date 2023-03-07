/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.views

import android.content.Context
import android.util.AttributeSet
import android.view.View
import android.view.ViewGroup
import androidx.transition.TransitionManager
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetDragHandleView

/**
 * A [BottomSheetDragHandleView] that hides itself when the bottom sheet is expanded full-screen
 */
class DragHandleView : BottomSheetDragHandleView {
    private val visibilityCallback = DragIndicatorCallback()
    private var callbackAttached = false

    init {
        isFocusable = false
        isClickable = false
    }

    constructor(context : Context) : super(context)
    constructor(context : Context, attrs : AttributeSet?) : super(context, attrs)
    constructor(context : Context, attrs : AttributeSet?, defStyleAttr : Int) : super(context, attrs, defStyleAttr)

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()
        if (callbackAttached)
            return

        // Check if this view is inside a BottomSheetDialog and attach the visibility callback in that case
        var parentView = parent
        while (parentView != null) {
            try {
                val behavior = BottomSheetBehavior.from(parentView as View)
                behavior.addBottomSheetCallback(visibilityCallback)
                callbackAttached = true
                break
            } catch (e : IllegalArgumentException) {
                parentView = parentView.parent
            }
        }
    }

    inner class DragIndicatorCallback : BottomSheetBehavior.BottomSheetCallback() {
        override fun onStateChanged(bottomSheet : View, newState : Int) {
            // Enables animation between visibility states
            TransitionManager.beginDelayedTransition(parent as ViewGroup)

            visibility = if (newState == BottomSheetBehavior.STATE_EXPANDED && bottomSheet.top == 0)
                View.INVISIBLE
            else
                View.VISIBLE
        }

        override fun onSlide(bottomSheet : View, slideOffset : Float) {}
    }
}
