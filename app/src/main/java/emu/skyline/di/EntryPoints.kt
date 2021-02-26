package emu.skyline.di

import android.content.Context
import dagger.hilt.EntryPoint
import dagger.hilt.InstallIn
import dagger.hilt.android.EntryPointAccessors
import dagger.hilt.components.SingletonComponent
import emu.skyline.input.InputManager
import emu.skyline.utils.Settings

@EntryPoint
@InstallIn(SingletonComponent::class)
interface InputManagerProviderEntryPoint {
    fun inputManager() : InputManager
}

fun Context.getInputManager() = EntryPointAccessors.fromApplication(this, InputManagerProviderEntryPoint::class.java).inputManager()

@EntryPoint
@InstallIn(SingletonComponent::class)
interface SettingsProviderEntryPoint {
    fun settings() : Settings
}

fun Context.getSettings() = EntryPointAccessors.fromApplication(this, SettingsProviderEntryPoint::class.java).settings()
