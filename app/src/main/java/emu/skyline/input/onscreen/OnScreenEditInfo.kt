/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.input.onscreen

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

    val isEditing get() = editMode != EditMode.None
}
