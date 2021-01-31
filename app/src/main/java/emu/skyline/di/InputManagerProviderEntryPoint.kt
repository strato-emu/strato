package emu.skyline.di

import android.content.Context
import dagger.hilt.EntryPoint
import dagger.hilt.InstallIn
import dagger.hilt.android.EntryPointAccessors
import dagger.hilt.components.SingletonComponent
import emu.skyline.input.InputManager

@EntryPoint
@InstallIn(SingletonComponent::class)
interface InputManagerProviderEntryPoint {
    fun inputManager() : InputManager

    companion object {
        fun getInputManager(context : Context) = EntryPointAccessors.fromApplication(context, InputManagerProviderEntryPoint::class.java).inputManager()
    }
}
