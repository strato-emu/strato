/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.utils

import android.content.Context
import androidx.preference.PreferenceManager
import kotlin.properties.ReadWriteProperty
import kotlin.reflect.KProperty

inline fun <reified T> sharedPreferences(context : Context, default : T, prefix : String = "", prefName : String? = null) = SharedPreferencesDelegate(context, T::class.java, default, prefix, prefName)

@Suppress("UNCHECKED_CAST")
class SharedPreferencesDelegate<T>(context : Context, private val clazz : Class<T>, private val default : T, private val prefix : String, prefName : String?) : ReadWriteProperty<Any, T> {
    private val prefs = prefName?.let { context.getSharedPreferences(prefName, Context.MODE_PRIVATE) } ?: PreferenceManager.getDefaultSharedPreferences(context)

    override fun setValue(thisRef : Any, property : KProperty<*>, value : T) = (prefix + camelToSnakeCase(property.name)).let { keyName ->
        prefs.edit().apply {
            when (clazz) {
                Float::class.java, java.lang.Float::class.java -> putFloat(keyName, value as Float)
                Boolean::class.java, java.lang.Boolean::class.java -> putBoolean(keyName, value as Boolean)
                String::class.java, java.lang.String::class.java -> putString(keyName, value as String)
                Int::class.java, java.lang.Integer::class.java -> putInt(keyName, value as Int)
                else -> error("Unsupported type $clazz")
            }
        }.apply()
    }

    override fun getValue(thisRef : Any, property : KProperty<*>) : T = (prefix + camelToSnakeCase(property.name)).let { keyName ->
        prefs.let {
            @Suppress("IMPLICIT_CAST_TO_ANY")
            when (clazz) {
                Float::class.java, java.lang.Float::class.java -> it.getFloat(keyName, default as Float)
                Boolean::class.java, java.lang.Boolean::class.java -> it.getBoolean(keyName, default as Boolean)
                String::class.java, java.lang.String::class.java -> it.getString(keyName, default as String)
                Int::class.java, java.lang.Integer::class.java -> it.getInt(keyName, default as Int)
                else -> error("Unsupported type $clazz")
            }
        } as T
    }

    private fun camelToSnakeCase(text : String) = StringBuilder().apply {
        text.forEachIndexed { index, c ->
            if (index != 0 && c.isUpperCase()) append('_')
            append(c.toLowerCase())
        }.toString()
    }
}
