/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package org.stratoemu.strato.di

import android.content.Context
import dagger.hilt.EntryPoint
import dagger.hilt.InstallIn
import dagger.hilt.android.EntryPointAccessors
import dagger.hilt.components.SingletonComponent
import org.stratoemu.strato.input.InputManager
import org.stratoemu.strato.settings.AppSettings

@EntryPoint
@InstallIn(SingletonComponent::class)
interface InputManagerProviderEntryPoint {
    fun inputManager() : InputManager
}

fun Context.getInputManager() = EntryPointAccessors.fromApplication(this, InputManagerProviderEntryPoint::class.java).inputManager()

@EntryPoint
@InstallIn(SingletonComponent::class)
interface SettingsProviderEntryPoint {
    fun appSettings() : AppSettings
}

fun Context.getSettings() = EntryPointAccessors.fromApplication(this, SettingsProviderEntryPoint::class.java).appSettings()
