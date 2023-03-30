/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.input.onscreen

import android.content.res.Resources
import android.util.TypedValue

typealias OnEditButtonChangedListener = ((ConfigurableButton) -> Unit)

/**
 * A small class that holds information about the current edit session
 * This is used to share information between the [OnScreenControllerView] and the individual [OnScreenButton]s
 */
class OnScreenEditInfo {
    /**
     * Whether the buttons are currently in edit mode
     */
    var isEditing : Boolean = false

    /**
     * The button that is currently being edited
     */
    private lateinit var _editButton : ConfigurableButton
    var editButton : ConfigurableButton
        get() = _editButton
        set(value) {
            _editButton = value
            onEditButtonChangedListener?.invoke(value)
        }

    var onEditButtonChangedListener : OnEditButtonChangedListener? = null

    /**
     * Whether the buttons should snap to a grid when in edit mode
     */
    var snapToGrid : Boolean = false

    var gridSize : Int = GridSize

    companion object {
        /**
         * The size of the grid, calculated from the value of 8dp
         */
        var GridSize = TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, 8f, Resources.getSystem().displayMetrics).toInt()

        /**
         * The amount the button will be moved when using the arrow keys
         */
        val ArrowKeyMoveAmount = TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, 2f, Resources.getSystem().displayMetrics).toInt()
    }
}
