/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2023 Strato Team and Contributors (https://github.com/strato-emu/)
 */

package org.stratoemu.strato.preference

import android.content.Context
import android.util.AttributeSet
import androidx.preference.Preference
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import android.content.ClipData
import android.content.ClipboardManager
import org.stratoemu.strato.settings.EmulationSettings

/**
 * Shows a dialog to copy the current game's custom settings (or the global settings if customs are disabled) so they
 * can be easily shareable in Discord or other places
 */
class ExportCustomSettingsPreference @JvmOverloads constructor(context : Context, attrs : AttributeSet? = null, defStyleAttr : Int = androidx.preference.R.attr.preferenceStyle) : Preference(context, attrs, defStyleAttr) {

    init {
        setOnPreferenceClickListener {
            var emulationSettings = EmulationSettings.forPrefName(preferenceManager.sharedPreferencesName)

            if (!emulationSettings.useCustomSettings) {
                emulationSettings = EmulationSettings.global
            }

            val systemIsDocked = emulationSettings.isDocked;

            val gpuDriver = emulationSettings.gpuDriver;
            val gpuTripleBuffering = emulationSettings.forceTripleBuffering;
            val gpuExecSlotCount = emulationSettings.executorSlotCountScale
            val gpuExecFlushThreshold = emulationSettings.executorFlushThreshold;
            val gpuDMI = emulationSettings.useDirectMemoryImport;
            val gpuFreeGuestTextureMemory = emulationSettings.freeGuestTextureMemory;
            val gpuDisableShaderCache = emulationSettings.disableShaderCache;
            val gpuForceMaxGpuClocks = emulationSettings.forceMaxGpuClocks

            val hackFastGpuReadback = emulationSettings.enableFastGpuReadbackHack;
            val hackFastReadbackWrite = emulationSettings.enableFastReadbackWrites;
            val hackDisableSubgroupShuffle = emulationSettings.disableSubgroupShuffle;

            val settingsAsText = String.format(
                """
                SYSTEM
                - Docked: $systemIsDocked
                
                GPU
                - Driver: $gpuDriver
                - Executors: $gpuExecSlotCount slots (threshold: $gpuExecFlushThreshold)
                - Triple buffering: $gpuTripleBuffering, DMI: $gpuDMI
                - Max clocks: $gpuForceMaxGpuClocks, free guest texture memory: $gpuFreeGuestTextureMemory
                - Disable shader cache: $gpuDisableShaderCache
                
                HACKS
                - Fast GPU readback: $hackFastGpuReadback, fast readback writes $hackFastReadbackWrite
                - Disable GPU subgroup shuffle: $hackDisableSubgroupShuffle
                """.trimIndent().replace("true", "✔").replace("false", "✖")
            )

            MaterialAlertDialogBuilder(context)
                .setTitle(title)
                .setMessage(settingsAsText)
                .setPositiveButton(android.R.string.copy) { _, _ ->
                    // Copy the current settings as text to the system clipboard
                    val clipboard = context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
                    val clip = ClipData.newPlainText("label", settingsAsText)
                    clipboard.setPrimaryClip(clip)

                }
                .setNegativeButton(android.R.string.ok, null)
                .show()

            true
        }
    }
}
