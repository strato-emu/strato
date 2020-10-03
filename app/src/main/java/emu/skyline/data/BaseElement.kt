/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.data

import java.io.Serializable

enum class ElementType {
    Header,
    Item
}

abstract class BaseElement(val elementType : ElementType) : Serializable
