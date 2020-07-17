/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.data

import emu.skyline.adapter.BaseElement
import emu.skyline.adapter.ElementType

/**
 * This is an abstract class that all adapter item classes inherit from
 */
abstract class BaseItem : BaseElement(ElementType.Item) {
    /**
     * This function returns a string used for searching
     */
    abstract fun key() : String?
}
