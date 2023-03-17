/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.input.onscreen

import android.os.Build
import android.os.Bundle
import android.os.Vibrator
import android.os.VibratorManager
import android.view.*
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.android.material.floatingactionbutton.FloatingActionButton
import dagger.hilt.android.AndroidEntryPoint
import emu.skyline.R
import emu.skyline.databinding.OnScreenEditActivityBinding
import emu.skyline.settings.AppSettings
import emu.skyline.utils.SwitchColors
import emu.skyline.utils.SwitchColors.*
import petrov.kristiyan.colorpicker.DoubleColorPicker
import petrov.kristiyan.colorpicker.DoubleColorPicker.OnChooseDoubleColorListener
import javax.inject.Inject

@AndroidEntryPoint
class OnScreenEditActivity : AppCompatActivity() {
    private val binding by lazy { OnScreenEditActivityBinding.inflate(layoutInflater) }

    private var fullEditVisible = true

    @Inject
    lateinit var appSettings : AppSettings

    private val closeAction : () -> Unit = {
        if (binding.onScreenControllerView.isEditing) {
            toggleFabVisibility(true)
            binding.onScreenControllerView.setEditMode(EditMode.None)
        } else {
            fullEditVisible = !fullEditVisible
            toggleFabVisibility(fullEditVisible)
            fabMapping[R.drawable.ic_close]!!.animate().rotation(if (fullEditVisible) 0f else 45f)
        }
    }

    private fun toggleFabVisibility(visible : Boolean) {
        fabMapping.forEach { (id, fab) ->
            if (id != R.drawable.ic_close) {
                if (visible) fab.show()
                else fab.hide()
            }
        }
    }

    private val moveAction = {
        binding.onScreenControllerView.setEditMode(EditMode.Move)
        toggleFabVisibility(false)
    }

    private val resizeAction = {
        binding.onScreenControllerView.setEditMode(EditMode.Resize)
        toggleFabVisibility(false)
    }

    private val toggleAction : () -> Unit = {
        val buttonProps = binding.onScreenControllerView.getButtonProps()
        val checkedButtonsArray = buttonProps.map { it.enabled }.toBooleanArray()

        MaterialAlertDialogBuilder(this)
            .setMultiChoiceItems(
                buttonProps.map { button ->
                    val longText = getString(button.buttonId.long!!)
                    if (button.buttonId.short == longText) longText else "$longText: ${button.buttonId.short}"
                }.toTypedArray(),
                checkedButtonsArray
            ) { _, which, isChecked ->
                checkedButtonsArray[which] = isChecked
            }
            .setPositiveButton(R.string.confirm) { _, _ ->
                buttonProps.forEachIndexed { index, button ->
                    if (checkedButtonsArray[index] != button.enabled)
                        binding.onScreenControllerView.setButtonEnabled(button.buttonId, checkedButtonsArray[index])
                }
            }
            .setNegativeButton(R.string.cancel, null)
            .setOnDismissListener { fullScreen() }
            .show()
    }

    private val paletteAction : () -> Unit = {
        DoubleColorPicker(this@OnScreenEditActivity).apply {
            setTitle(this@OnScreenEditActivity.getString(R.string.osc_background_color))
            setDefaultColorButton(binding.onScreenControllerView.getBackGroundColor())
            setRoundColorButton(true)
            setColors(*SwitchColors.colors.toIntArray())
            setDefaultDoubleColorButton(binding.onScreenControllerView.getTextColor())
            setSecondTitle(this@OnScreenEditActivity.getString(R.string.osc_text_color))
            setOnChooseDoubleColorListener(object : OnChooseDoubleColorListener {
                override fun onChooseColor(position : Int, color : Int, position2 : Int, color2 : Int) {
                    binding.onScreenControllerView.setBackGroundColor(SwitchColors.colors[position])
                    binding.onScreenControllerView.setTextColor(SwitchColors.colors[position2])
                }

                override fun onCancel() {}
            })
            show()
        }
    }

    private val actions : List<Pair<Int, () -> Unit>> = listOf(
        Pair(R.drawable.ic_palette, paletteAction),
        Pair(R.drawable.ic_restore) { binding.onScreenControllerView.resetControls() },
        Pair(R.drawable.ic_toggle, toggleAction),
        Pair(R.drawable.ic_move, moveAction),
        Pair(R.drawable.ic_resize, resizeAction),
        Pair(R.drawable.ic_zoom_out) { binding.onScreenControllerView.decreaseScale() },
        Pair(R.drawable.ic_zoom_in) { binding.onScreenControllerView.increaseScale() },
        Pair(R.drawable.ic_opacity_minus) { binding.onScreenControllerView.decreaseOpacity() },
        Pair(R.drawable.ic_opacity_plus) { binding.onScreenControllerView.increaseOpacity() },
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

        binding.onScreenControllerView.vibrator = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S)
            (getSystemService(VIBRATOR_MANAGER_SERVICE) as VibratorManager).defaultVibrator
        else
            @Suppress("DEPRECATION")
            getSystemService(VIBRATOR_SERVICE) as Vibrator

        binding.onScreenControllerView.recenterSticks = appSettings.onScreenControlRecenterSticks

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
        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.R) {
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
