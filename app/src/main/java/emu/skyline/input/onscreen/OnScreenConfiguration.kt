/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.input.onscreen

import android.content.Context
import emu.skyline.input.ButtonId
import kotlin.properties.ReadWriteProperty
import kotlin.reflect.KProperty

interface ControllerConfiguration {
    var enabled : Boolean
    var globalScale : Float
    var relativeX : Float
    var relativeY : Float
}

class ControllerConfigurationDummy(
        defaultRelativeX : Float,
        defaultRelativeY : Float
) : ControllerConfiguration {
    override var enabled = true
    override var globalScale = 1f
    override var relativeX = defaultRelativeX
    override var relativeY = defaultRelativeY
}

class ControllerConfigurationImpl(
        private val context : Context,
        private val buttonId : ButtonId,
        defaultRelativeX : Float,
        defaultRelativeY : Float
) : ControllerConfiguration {
    private inline fun <reified T> config(default : T) = ControllerPrefs(context, "${buttonId.name}_", T::class.java, default)

    override var enabled by config(true)
    override var globalScale by ControllerPrefs(context, "on_screen_controller_", Float::class.java, 1f)
    override var relativeX by config(defaultRelativeX)
    override var relativeY by config(defaultRelativeY)
}

@Suppress("UNCHECKED_CAST")
private class ControllerPrefs<T>(context : Context, private val prefix : String, private val clazz : Class<T>, private val default : T) : ReadWriteProperty<Any, T> {
    companion object {
        const val CONTROLLER_CONFIG = "controller_config"
    }

    private val prefs = context.getSharedPreferences(CONTROLLER_CONFIG, Context.MODE_PRIVATE)

    override fun setValue(thisRef : Any, property : KProperty<*>, value : T) {
        prefs.edit().apply {
            when (clazz) {
                Float::class.java, java.lang.Float::class.java -> putFloat(prefix + property.name, value as Float)
                Boolean::class.java, java.lang.Boolean::class.java -> putBoolean(prefix + property.name, value as Boolean)
                else -> error("Unsupported type $clazz ${Float::class.java}")
            }
        }.apply()
    }

    override fun getValue(thisRef : Any, property : KProperty<*>) : T =
            prefs.let {
                @Suppress("IMPLICIT_CAST_TO_ANY")
                when (clazz) {
                    Float::class.java, java.lang.Float::class.java -> it.getFloat(prefix + property.name, default as Float)
                    Boolean::class.java, java.lang.Boolean::class.java -> it.getBoolean(prefix + property.name, default as Boolean)
                    else -> error("Unsupported type $clazz")
                }
            } as T
}
