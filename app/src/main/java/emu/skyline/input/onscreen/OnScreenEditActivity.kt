/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.input.onscreen

import android.os.Build
import android.os.Bundle
import android.view.*
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.android.material.floatingactionbutton.FloatingActionButton
import dagger.hilt.android.AndroidEntryPoint
import emu.skyline.R
import emu.skyline.databinding.OnScreenEditActivityBinding
import emu.skyline.utils.PreferenceSettings
import javax.inject.Inject

@AndroidEntryPoint
class OnScreenEditActivity : AppCompatActivity() {
    private val binding by lazy { OnScreenEditActivityBinding.inflate(layoutInflater) }

    private var fullEditVisible = true
    private var editMode = false

    @Inject
    lateinit var preferenceSettings : PreferenceSettings

    private val closeAction : () -> Unit = {
        if (editMode) {
            toggleFabVisibility(true)
            binding.onScreenControllerView.setEditMode(false)
            editMode = false
        } else {
            fullEditVisible = !fullEditVisible
            toggleFabVisibility(fullEditVisible)
            fabMapping[R.drawable.ic_close]!!.animate().rotation(if (fullEditVisible) 0f else 45f)
        }
    }

    private fun toggleFabVisibility(visible : Boolean) {
        actions.forEach {
            if (it.first != R.drawable.ic_close)
                if (visible) fabMapping[it.first]!!.show()
                else fabMapping[it.first]!!.hide()
        }
    }

    private val editAction = {
        editMode = true
        binding.onScreenControllerView.setEditMode(true)
        toggleFabVisibility(false)
    }

    private val toggleAction : () -> Unit = {
        val buttonProps = binding.onScreenControllerView.getButtonProps()
        val checkArray = buttonProps.map { it.second }.toBooleanArray()

        MaterialAlertDialogBuilder(this)
                .setMultiChoiceItems(buttonProps.map {
                    val longText = getString(it.first.long!!)
                    if (it.first.short == longText) longText else "$longText: ${it.first.short}"
                }.toTypedArray(), checkArray) { _, which, isChecked ->
                    checkArray[which] = isChecked
                }.setPositiveButton(R.string.confirm) { _, _ ->
                    buttonProps.forEachIndexed { index, pair ->
                        if (checkArray[index] != pair.second)
                            binding.onScreenControllerView.setButtonEnabled(pair.first, checkArray[index])
                    }
                }.setNegativeButton(R.string.cancel, null)
                .setOnDismissListener { fullScreen() }
                .show()
    }

    private val actions : List<Pair<Int, () -> Unit>> = listOf(
            Pair(R.drawable.ic_restore, { binding.onScreenControllerView.resetControls() }),
            Pair(R.drawable.ic_toggle, toggleAction),
            Pair(R.drawable.ic_edit, editAction),
            Pair(R.drawable.ic_zoom_out, { binding.onScreenControllerView.decreaseScale() }),
            Pair(R.drawable.ic_zoom_in, { binding.onScreenControllerView.increaseScale() }),
            Pair(R.drawable.ic_close, closeAction)
    )

    private val fabMapping = mutableMapOf<Int, FloatingActionButton>()

    override fun onCreate(savedInstanceState : Bundle?) {
        super.onCreate(savedInstanceState)
        window.attributes.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
        setContentView(binding.root)

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            // Android might not allow child views to overlap the system bars
            // Override this behavior and force content to extend into the cutout area
            window.setDecorFitsSystemWindows(false)

            window.insetsController?.let {
                it.systemBarsBehavior = WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
                it.hide(WindowInsets.Type.systemBars())
            }
        }

        binding.onScreenControllerView.recenterSticks = preferenceSettings.onScreenControlRecenterSticks

        actions.forEach { pair ->
            binding.fabParent.addView(LayoutInflater.from(this).inflate(R.layout.on_screen_edit_mini_fab, binding.fabParent, false).apply {
                (this as FloatingActionButton).setImageDrawable(ContextCompat.getDrawable(context, pair.first))
                setOnClickListener { pair.second.invoke() }
                fabMapping[pair.first] = this
            })
        }
    }

    override fun onResume() {
        super.onResume()

        fullScreen()
    }

    private fun fullScreen() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            @Suppress("DEPRECATION")
            window.decorView.systemUiVisibility = (View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                    or View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                    or View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                    or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                    or View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                    or View.SYSTEM_UI_FLAG_FULLSCREEN)
        }
    }
}
