/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.input.onscreen

import android.annotation.SuppressLint
import android.os.Build
import android.os.Bundle
import android.os.Vibrator
import android.os.VibratorManager
import android.view.*
import android.view.View.OnTouchListener
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.isGone
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import dagger.hilt.android.AndroidEntryPoint
import emu.skyline.R
import emu.skyline.databinding.OnScreenEditActivityBinding
import emu.skyline.databinding.OscSliderBinding
import emu.skyline.settings.AppSettings
import emu.skyline.utils.SwitchColors
import emu.skyline.utils.SwitchColors.*
import petrov.kristiyan.colorpicker.DoubleColorPicker
import petrov.kristiyan.colorpicker.DoubleColorPicker.OnChooseDoubleColorListener
import javax.inject.Inject
import kotlin.math.roundToInt

@AndroidEntryPoint
class OnScreenEditActivity : AppCompatActivity() {
    private val binding by lazy { OnScreenEditActivityBinding.inflate(layoutInflater) }

    @Inject
    lateinit var appSettings : AppSettings

    private fun paletteAction() {
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

    private fun toggleGridAction() {
        val snapToGrid = !appSettings.onScreenControlSnapToGrid
        appSettings.onScreenControlSnapToGrid = snapToGrid

        binding.onScreenControllerView.setSnapToGrid(snapToGrid)
        binding.alignmentGrid.isGone = !snapToGrid
        binding.gridButton.setIconResource(if (!snapToGrid) R.drawable.ic_grid_on else R.drawable.ic_grid_off)
    }

    private fun resetAction() {
        MaterialAlertDialogBuilder(this)
            .setTitle(getString(R.string.osc_reset, binding.onScreenControllerView.editButton.buttonId.short))
            .setMessage(R.string.osc_reset_confirm)
            .setPositiveButton(R.string.confirm) { _, _ -> binding.onScreenControllerView.resetButton() }
            .setNegativeButton(R.string.cancel, null)
            .setOnDismissListener { fullScreen() }
            .show()
    }

    @SuppressLint("ClickableViewAccessibility")
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

        val snapToGrid = appSettings.onScreenControlSnapToGrid
        binding.onScreenControllerView.setSnapToGrid(snapToGrid)

        binding.alignmentGrid.isGone = !snapToGrid
        binding.alignmentGrid.gridSize = OnScreenEditInfo.GridSize

        binding.onScreenControllerView.setOnEditButtonChangedListener { button ->
            updateActiveButtonDisplayInfo(button)
        }

        binding.selectAllButton.setOnClickListener { binding.onScreenControllerView.selectAllButtons() }

        populateSlider(binding.scaleSlider, getString(R.string.osc_scale)) {
            binding.onScreenControllerView.setButtonScale(it)
        }
        populateSlider(binding.opacitySlider, getString(R.string.osc_opacity)) {
            binding.onScreenControllerView.setButtonOpacity(it)
        }

        binding.enabledCheckbox.setOnClickListener { _ ->
            binding.onScreenControllerView.setButtonEnabled(binding.enabledCheckbox.isChecked)
        }

        binding.moveUpButton.setOnClickListener { binding.onScreenControllerView.moveButtonUp() }
        binding.moveDownButton.setOnClickListener { binding.onScreenControllerView.moveButtonDown() }
        binding.moveLeftButton.setOnClickListener { binding.onScreenControllerView.moveButtonLeft() }
        binding.moveRightButton.setOnClickListener { binding.onScreenControllerView.moveButtonRight() }

        binding.colorButton.setOnClickListener { paletteAction() }
        binding.gridButton.setOnClickListener { toggleGridAction() }
        binding.gridButton.setIconResource(if (!snapToGrid) R.drawable.ic_grid_on else R.drawable.ic_grid_off)
        binding.resetButton.setOnClickListener { resetAction() }

        binding.dragHandle.setOnTouchListener(dragPanelListener)

        binding.onScreenControllerView.setEditMode(true)
    }

    override fun onResume() {
        super.onResume()
        updateActiveButtonDisplayInfo(binding.onScreenControllerView.editButton)
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

    /**
     * Initializes the slider in the range [0,100], with the given label and value listener
     */
    @SuppressLint("SetTextI18n")
    private fun populateSlider(slider : OscSliderBinding, label : String, valueListener : ((Int) -> Unit)? = null) {
        slider.title.text = label
        slider.slider.apply {
            valueFrom = 0f
            valueTo = 100f
            stepSize = 0f
            // Always update the value label
            addOnChangeListener { _, value, _ ->
                slider.valueLabel.text = "${value.roundToInt()}%"
            }
            // Only call the value listener if the user is dragging the slider
            addOnChangeListener { _, value, fromUser ->
                if (fromUser)
                    valueListener?.invoke(value.roundToInt())
            }
        }
    }

    /**
     * Updates the control panel UI elements to reflect the currently selected button
     */
    private fun updateActiveButtonDisplayInfo(button : ConfigurableButton) {
        binding.enabledCheckbox.checkedState = button.config.groupEnabled
        binding.currentButton.text = getString(R.string.osc_current_button, button.buttonId.short)
        binding.scaleSlider.slider.value = (button.config.scale - OnScreenConfiguration.MinScale) / (OnScreenConfiguration.MaxScale - OnScreenConfiguration.MinScale) * 100f
        binding.opacitySlider.slider.value = (button.config.alpha - OnScreenConfiguration.MinAlpha) / (OnScreenConfiguration.MaxAlpha - OnScreenConfiguration.MinAlpha).toFloat() * 100f
    }

    @SuppressLint("ClickableViewAccessibility")
    private val dragPanelListener = OnTouchListener { view : View, event : MotionEvent ->
        if (event.action == MotionEvent.ACTION_MOVE) {
            binding.controlPanel.x = event.rawX - binding.controlPanel.width / 2
            binding.controlPanel.y = event.rawY - view.height / 2
        }
        true
    }
}
