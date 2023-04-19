/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.input.onscreen

import android.annotation.SuppressLint
import android.graphics.PointF
import android.os.Build
import android.os.Bundle
import android.os.Vibrator
import android.os.VibratorManager
import android.util.TypedValue
import android.view.*
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

    private var currentButtonName = ""

    private fun paletteAction() {
        DoubleColorPicker(this@OnScreenEditActivity).apply {
            setTitle(this@OnScreenEditActivity.getString(R.string.osc_background_color))
            setDefaultColorButton(binding.onScreenControllerView.getButtonBackgroundColor())
            setRoundColorButton(true)
            setColors(*SwitchColors.colors.toIntArray())
            setDefaultDoubleColorButton(binding.onScreenControllerView.getButtonTextColor())
            setSecondTitle(this@OnScreenEditActivity.getString(R.string.osc_text_color))
            setOnChooseDoubleColorListener(object : OnChooseDoubleColorListener {
                override fun onChooseColor(position : Int, color : Int, position2 : Int, color2 : Int) {
                    binding.onScreenControllerView.setButtonBackgroundColor(SwitchColors.colors[position])
                    binding.onScreenControllerView.setButtonTextColor(SwitchColors.colors[position2])
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
            .setTitle(getString(R.string.osc_reset, currentButtonName))
            .setMessage(R.string.osc_reset_confirm)
            .setPositiveButton(R.string.confirm) { _, _ -> binding.onScreenControllerView.resetButton() }
            .setNegativeButton(R.string.cancel, null)
            .setOnDismissListener { fullScreen() }
            .show()
    }

    @SuppressLint("ClickableViewAccessibility", "SetTextI18n")
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
        binding.onScreenControllerView.stickRegions = appSettings.onScreenControlUseStickRegions

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
        populateSlider(binding.activationSlider, getString(R.string.osc_activation_radius)) {
            binding.onScreenControllerView.setStickActivationRadius(it)
        }

        binding.enabledCheckbox.setOnClickListener { _ ->
            binding.onScreenControllerView.setButtonEnabled(binding.enabledCheckbox.isChecked)
        }
        binding.toggleModeCheckbox.setOnClickListener { _ ->
            binding.onScreenControllerView.setButtonToggleMode(binding.toggleModeCheckbox.isChecked)
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
        binding.closeButton.setOnClickListener { togglePanelVisibility() }

        binding.onScreenControllerView.setOnButtonStateChangedListener { buttonId, state ->
            binding.lastInputEvent.text = "Timestamp: ${System.currentTimeMillis()}\nButton: ${buttonId.short}\n$state"
        }
        binding.onScreenControllerView.setOnStickStateChangedListener { stickId, position ->
            val x = "%9.6f".format(position.x)
            val y = "%9.6f".format(position.y)
            binding.lastInputEvent.text = "Timestamp: ${System.currentTimeMillis()}\nStick: ${stickId.button.short}\nX: $x\nY: $y"
        }

        binding.onScreenControllerView.setEditMode(true)
    }

    override fun onResume() {
        super.onResume()
        binding.onScreenControllerView.updateEditButtonInfo()
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
            // Update the value label with the initial value to avoid it being blank
            slider.valueLabel.text = "${value.roundToInt()}%"
        }
    }

    /**
     * Updates the control panel UI elements to reflect the currently selected button
     */
    private fun updateActiveButtonDisplayInfo(button : ConfigurableButton) {
        fun Float.toSliderRange(min : Float, max : Float) : Float = ((this - min) / (max - min) * 100f).coerceIn(0f, 100f)
        fun Int.toSliderRange(min : Int, max : Int) : Float = ((this - min) / (max - min).toFloat() * 100f).coerceIn(0f, 100f)

        binding.enabledCheckbox.checkedState = button.config.groupEnabled
        currentButtonName = button.buttonId.short ?: ""
        binding.currentButton.text = getString(R.string.osc_current_button, currentButtonName)
        binding.scaleSlider.slider.value = button.config.scale.toSliderRange(OnScreenConfiguration.MinScale, OnScreenConfiguration.MaxScale)
        binding.opacitySlider.slider.value = button.config.alpha.toSliderRange(OnScreenConfiguration.MinAlpha, OnScreenConfiguration.MaxAlpha)
        if (button is JoystickButton) {
            binding.toggleModeCheckbox.isGone = true
            binding.activationSlider.all.isGone = false
            binding.activationSlider.slider.value = button.config.activationRadius.toSliderRange(OnScreenConfiguration.MinActivationRadius, OnScreenConfiguration.MaxActivationRadius)
        } else {
            binding.toggleModeCheckbox.isGone = false
            binding.toggleModeCheckbox.checkedState = button.config.groupToggleMode
            binding.activationSlider.all.isGone = true
        }
    }

    @SuppressLint("ClickableViewAccessibility")
    private val dragPanelListener = { view : View, event : MotionEvent ->
        if (event.action == MotionEvent.ACTION_MOVE) {
            binding.controlPanel.x = event.rawX - binding.controlPanel.width / 2
            binding.controlPanel.y = event.rawY - view.height / 2
        }
        true
    }

    private var isPanelVisible = true
    private val openPanelTranslation = PointF()

    private fun togglePanelVisibility() {
        isPanelVisible = !isPanelVisible
        binding.lastInputEvent.text = null
        binding.content.isGone = !isPanelVisible
        binding.dragHandle.isGone = !isPanelVisible

        binding.closeButton.apply {
            if (isPanelVisible) {
                animate().rotation(0f).start()

                val buttonSize = TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, 32f, resources.displayMetrics).toInt() // 32dp
                layoutParams.width = buttonSize
                layoutParams.height = buttonSize
            } else {
                animate().rotation(-225f).start()

                val buttonSize = TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, 40f, resources.displayMetrics).toInt() // 40dp
                layoutParams.width = buttonSize
                layoutParams.height = buttonSize
            }
        }

        if (!isPanelVisible) {
            // Save the current open position to restore later
            openPanelTranslation.set(binding.controlPanel.translationX, binding.controlPanel.translationY)

            // Animate to the closed position
            binding.controlPanel.animate()
                .translationX(0f)
                .translationY(TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, 12f, resources.displayMetrics)) // 12dp
                .start()
        } else {
            // Animate to the previously saved open position
            binding.controlPanel.animate()
                .translationX(openPanelTranslation.x)
                .translationY(openPanelTranslation.y)
                .start()
        }

        binding.onScreenControllerView.setEditMode(isPanelVisible)
    }
}
