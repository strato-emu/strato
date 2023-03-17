/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.input.onscreen

import android.util.TypedValue
import emu.skyline.SkylineApplication

enum class EditMode {
    None,
    Move,
    Resize
}

/**
 * A small class that holds information about the current edit session
 * This is used to share information between the [OnScreenControllerView] and the individual [OnScreenButton]s
 */
class OnScreenEditInfo {
    /**
     * The current edit mode
     */
    var editMode : EditMode = EditMode.None

    /**
     * The button that is currently being edited
     */
    var editButton : OnScreenButton? = null

    /**
     * Whether the buttons should snap to a grid when in edit mode
     */
    var snapToGrid : Boolean = false

    var gridSize : Int = GridSize

    val isEditing get() = editMode != EditMode.None

    companion object {

        /**
         * The size of the grid, calculated from the value of 8dp
         */
        val GridSize = TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, 8f, SkylineApplication.context.resources.displayMetrics).toInt()
    }
}
