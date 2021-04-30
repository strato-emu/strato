/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline

import android.annotation.SuppressLint
import android.content.Context
import android.content.Intent
import android.content.res.AssetManager
import android.graphics.PointF
import android.net.Uri
import android.os.*
import android.util.Log
import android.view.*
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.isGone
import androidx.core.view.isInvisible
import dagger.hilt.android.AndroidEntryPoint
import emu.skyline.databinding.EmuActivityBinding
import emu.skyline.input.*
import emu.skyline.loader.getRomFormat
import emu.skyline.utils.Settings
import java.io.File
import javax.inject.Inject
import kotlin.math.abs

@AndroidEntryPoint
class EmulationActivity : AppCompatActivity(), SurfaceHolder.Callback, View.OnTouchListener {
    companion object {
        private val Tag = EmulationActivity::class.java.simpleName
    }

    init {
        System.loadLibrary("skyline") // libskyline.so
    }

    private val binding by lazy { EmuActivityBinding.inflate(layoutInflater) }

    /**
     * A map of [Vibrator]s that correspond to [InputManager.controllers]
     */
    private var vibrators = HashMap<Int, Vibrator>()

    /**
     * A boolean flag denoting if the emulation thread should call finish() or not
     */
    @Volatile
    private var shouldFinish : Boolean = true

    /**
     * The Kotlin thread on which emulation code executes
     */
    private lateinit var emulationThread : Thread

    @Inject
    lateinit var settings : Settings

    @Inject
    lateinit var inputManager : InputManager

    /**
     * This is the entry point into the emulation code for libskyline
     *
     * @param romUri The URI of the ROM as a string, used to print out in the logs
     * @param romType The type of the ROM as an enum value
     * @param romFd The file descriptor of the ROM object
     * @param preferenceFd The file descriptor of the Preference XML
     * @param appFilesPath The full path to the app files directory
     * @param assetManager The asset manager used for accessing app assets
     */
    private external fun executeApplication(romUri : String, romType : Int, romFd : Int, preferenceFd : Int, appFilesPath : String, assetManager : AssetManager)

    /**
     * @return If it successfully caused the [emulationThread] to gracefully stop
     */
    private external fun stopEmulation() : Boolean

    /**
     * This sets the surface object in libskyline to the provided value, emulation is halted if set to null
     *
     * @param surface The value to set surface to
     * @return If the value was successfully set
     */
    private external fun setSurface(surface : Surface?) : Boolean

    var fps : Int = 0
    var frametime : Float = 0.0f

    /**
     * Writes the current performance statistics into [fps] and [frametime] fields
     */
    private external fun updatePerformanceStatistics()

    /**
     * This initializes a guest controller in libskyline
     *
     * @param index The arbitrary index of the controller, this is to handle matching with a partner Joy-Con
     * @param type The type of the host controller
     * @param partnerIndex The index of a partner Joy-Con if there is one
     * @note This is blocking and will stall till input has been initialized on the guest
     */
    private external fun setController(index : Int, type : Int, partnerIndex : Int = -1)

    /**
     * This flushes the controller updates on the guest
     *
     * @note This is blocking and will stall till input has been initialized on the guest
     */
    private external fun updateControllers()

    /**
     * This sets the state of the buttons specified in the mask on a specific controller
     *
     * @param index The index of the controller this is directed to
     * @param mask The mask of the button that are being set
     * @param pressed If the buttons are being pressed or released
     */
    private external fun setButtonState(index : Int, mask : Long, pressed : Boolean)

    /**
     * This sets the value of a specific axis on a specific controller
     *
     * @param index The index of the controller this is directed to
     * @param axis The ID of the axis that is being modified
     * @param value The value to set the axis to
     */
    private external fun setAxisValue(index : Int, axis : Int, value : Int)

    /**
     * This sets the values of the points on the guest touch-screen
     *
     * @param points An array of skyline::input::TouchScreenPoint in C++ represented as integers
     */
    private external fun setTouchState(points : IntArray)

    /**
     * This initializes all of the controllers from [InputManager] on the guest
     */
    @Suppress("unused")
    private fun initializeControllers() {
        for (controller in inputManager.controllers.values) {
            if (controller.type != ControllerType.None) {
                val type = when (controller.type) {
                    ControllerType.None -> throw IllegalArgumentException()
                    ControllerType.HandheldProController -> if (settings.operationMode) ControllerType.ProController.id else ControllerType.HandheldProController.id
                    ControllerType.ProController, ControllerType.JoyConLeft, ControllerType.JoyConRight -> controller.type.id
                }

                val partnerIndex = when (controller) {
                    is JoyConLeftController -> controller.partnerId
                    is JoyConRightController -> controller.partnerId
                    else -> null
                }

                setController(controller.id, type, partnerIndex ?: -1)
            }
        }

        updateControllers()
    }

    /**
     * This executes the specified ROM
     *
     * @param rom The URI of the ROM to execute
     */
    private fun executeApplication(rom : Uri) {
        val romType = getRomFormat(rom, contentResolver).ordinal
        val romFd = contentResolver.openFileDescriptor(rom, "r")!!
        val preferenceFd = ParcelFileDescriptor.open(File("${applicationInfo.dataDir}/shared_prefs/${applicationInfo.packageName}_preferences.xml"), ParcelFileDescriptor.MODE_READ_WRITE)

        emulationThread = Thread {
            executeApplication(rom.toString(), romType, romFd.detachFd(), preferenceFd.detachFd(), applicationContext.filesDir.canonicalPath + "/", assets)
            if (shouldFinish)
                runOnUiThread {
                    emulationThread.join()
                    finish()
                }
        }

        emulationThread.start()
    }

    /**
     * This makes the window fullscreen, sets up the performance statistics and finally calls [executeApplication] for executing the application
     */
    @SuppressLint("SetTextI18n", "ClickableViewAccessibility")
    override fun onCreate(savedInstanceState : Bundle?) {
        super.onCreate(savedInstanceState)

        setContentView(binding.root)

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            window.insetsController?.hide(WindowInsets.Type.navigationBars() or WindowInsets.Type.systemBars() or WindowInsets.Type.systemGestures() or WindowInsets.Type.statusBars())
            window.insetsController?.systemBarsBehavior = WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        }

        binding.gameView.holder.addCallback(this)

        if (settings.perfStats) {
            binding.perfStats.apply {
                postDelayed(object : Runnable {
                    override fun run() {
                        updatePerformanceStatistics()
                        text = "$fps FPS\n${frametime}ms"
                        postDelayed(this, 250)
                    }
                }, 250)
            }
        }

        @Suppress("DEPRECATION") val display = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) display!! else windowManager.defaultDisplay
        display?.supportedModes?.maxByOrNull { it.refreshRate + (it.physicalHeight * it.physicalWidth) }?.let { window.attributes.preferredDisplayModeId = it.modeId }

        binding.gameView.setOnTouchListener(this)

        // Hide on screen controls when first controller is not set
        binding.onScreenControllerView.apply {
            inputManager.controllers[0]!!.type.let {
                controllerType = it
                isGone = it == ControllerType.None || !settings.onScreenControl
            }
            setOnButtonStateChangedListener(::onButtonStateChanged)
            setOnStickStateChangedListener(::onStickStateChanged)
            recenterSticks = settings.onScreenControlRecenterSticks
        }

        binding.onScreenControllerToggle.apply {
            isGone = binding.onScreenControllerView.isGone
            setOnClickListener { binding.onScreenControllerView.isInvisible = !binding.onScreenControllerView.isInvisible }
        }

        executeApplication(intent.data!!)
    }

    override fun onResume() {
        super.onResume()

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

    /**
     * This is used to stop the currently executing ROM and replace it with the one specified in the new intent
     */
    override fun onNewIntent(intent : Intent?) {
        super.onNewIntent(intent)
        shouldFinish = false

        while (emulationThread.isAlive)
            if (stopEmulation())
                emulationThread.join()

        vibrators.forEach { (_, vibrator) -> vibrator.cancel() }

        shouldFinish = true

        executeApplication(intent?.data!!)
    }

    override fun onDestroy() {
        super.onDestroy()
        shouldFinish = false

        while (emulationThread.isAlive)
            if (stopEmulation())
                emulationThread.join()

        vibrators.forEach { (_, vibrator) -> vibrator.cancel() }
        vibrators.clear()
    }

    override fun surfaceCreated(holder : SurfaceHolder) {
        Log.d(Tag, "surfaceCreated Holder: $holder")
        while (emulationThread.isAlive)
            if (setSurface(holder.surface))
                return
    }

    /**
     * This is purely used for debugging surface changes
     */
    override fun surfaceChanged(holder : SurfaceHolder, format : Int, width : Int, height : Int) {
        Log.d(Tag, "surfaceChanged Holder: $holder, Format: $format, Width: $width, Height: $height")
    }

    override fun surfaceDestroyed(holder : SurfaceHolder) {
        Log.d(Tag, "surfaceDestroyed Holder: $holder")
        while (emulationThread.isAlive)
            if (setSurface(null))
                return
    }

    /**
     * This handles translating any [KeyHostEvent]s to a [GuestEvent] that is passed into libskyline
     */
    override fun dispatchKeyEvent(event : KeyEvent) : Boolean {
        if (event.repeatCount != 0)
            return super.dispatchKeyEvent(event)

        val action = when (event.action) {
            KeyEvent.ACTION_DOWN -> ButtonState.Pressed
            KeyEvent.ACTION_UP -> ButtonState.Released
            else -> return super.dispatchKeyEvent(event)
        }

        return when (val guestEvent = inputManager.eventMap[KeyHostEvent(event.device.descriptor, event.keyCode)]) {
            is ButtonGuestEvent -> {
                if (guestEvent.button != ButtonId.Menu)
                    setButtonState(guestEvent.id, guestEvent.button.value(), action.state)
                true
            }

            is AxisGuestEvent -> {
                setAxisValue(guestEvent.id, guestEvent.axis.ordinal, (if (action == ButtonState.Pressed) if (guestEvent.polarity) Short.MAX_VALUE else Short.MIN_VALUE else 0).toInt())
                true
            }

            else -> super.dispatchKeyEvent(event)
        }
    }

    /**
     * The last value of the axes so the stagnant axes can be eliminated to not wastefully look them up
     */
    private val axesHistory = arrayOfNulls<Float>(MotionHostEvent.axes.size)

    /**
     * The last value of the HAT axes so it can be ignored in [onGenericMotionEvent] so they are handled by [dispatchKeyEvent] instead
     */
    private var oldHat = PointF()

    /**
     * This handles translating any [MotionHostEvent]s to a [GuestEvent] that is passed into libskyline
     */
    override fun onGenericMotionEvent(event : MotionEvent) : Boolean {
        if ((event.isFromSource(InputDevice.SOURCE_CLASS_JOYSTICK) || event.isFromSource(InputDevice.SOURCE_CLASS_BUTTON)) && event.action == MotionEvent.ACTION_MOVE) {
            val hat = PointF(event.getAxisValue(MotionEvent.AXIS_HAT_X), event.getAxisValue(MotionEvent.AXIS_HAT_Y))

            if (hat == oldHat) {
                for (axisItem in MotionHostEvent.axes.withIndex()) {
                    val axis = axisItem.value
                    var value = event.getAxisValue(axis)

                    if ((event.historySize != 0 && value != event.getHistoricalAxisValue(axis, 0)) || (axesHistory[axisItem.index]?.let { it == value } == false)) {
                        var polarity = value >= 0

                        val guestEvent = MotionHostEvent(event.device.descriptor, axis, polarity).let { hostEvent ->
                            inputManager.eventMap[hostEvent] ?: if (value == 0f) {
                                polarity = false
                                inputManager.eventMap[hostEvent.copy(polarity = false)]
                            } else {
                                null
                            }
                        }

                        when (guestEvent) {
                            is ButtonGuestEvent -> {
                                if (guestEvent.button != ButtonId.Menu)
                                    setButtonState(guestEvent.id, guestEvent.button.value(), if (abs(value) >= guestEvent.threshold) ButtonState.Pressed.state else ButtonState.Released.state)
                            }

                            is AxisGuestEvent -> {
                                value = guestEvent.value(value)
                                value = if (polarity) abs(value) else -abs(value)
                                value = if (guestEvent.axis == AxisId.LX || guestEvent.axis == AxisId.RX) value else -value // TODO: Test this

                                setAxisValue(guestEvent.id, guestEvent.axis.ordinal, (value * Short.MAX_VALUE).toInt())
                            }
                        }
                    }

                    axesHistory[axisItem.index] = value
                }

                return true
            } else {
                oldHat = hat
            }
        }

        return super.onGenericMotionEvent(event)
    }

    @SuppressLint("ClickableViewAccessibility")
    override fun onTouch(view : View, event : MotionEvent) : Boolean {
        val count = if (event.action != MotionEvent.ACTION_UP && event.action != MotionEvent.ACTION_CANCEL) event.pointerCount else 0
        val points = IntArray(count * 5) // This is an array of skyline::input::TouchScreenPoint in C++ as that allows for efficient transfer of values to it
        var offset = 0
        for (index in 0 until count) {
            val pointer = MotionEvent.PointerCoords()
            event.getPointerCoords(index, pointer)

            val x = 0f.coerceAtLeast(pointer.x * 1280 / view.width).toInt()
            val y = 0f.coerceAtLeast(pointer.y * 720 / view.height).toInt()

            points[offset++] = x
            points[offset++] = y
            points[offset++] = pointer.touchMinor.toInt()
            points[offset++] = pointer.touchMajor.toInt()
            points[offset++] = (pointer.orientation * 180 / Math.PI).toInt()
        }

        setTouchState(points)

        return true
    }

    private fun onButtonStateChanged(buttonId : ButtonId, state : ButtonState) = setButtonState(0, buttonId.value(), state.state)

    private fun onStickStateChanged(stickId : StickId, position : PointF) {
        setAxisValue(0, stickId.xAxis.ordinal, (position.x * Short.MAX_VALUE).toInt())
        setAxisValue(0, stickId.yAxis.ordinal, (-position.y * Short.MAX_VALUE).toInt()) // Y is inverted, since drawing starts from top left
    }

    @SuppressLint("WrongConstant")
    @Suppress("unused")
    fun vibrateDevice(index : Int, timing : LongArray, amplitude : IntArray) {
        val vibrator = if (vibrators[index] != null) {
            vibrators[index]!!
        } else {
            inputManager.controllers[index]!!.rumbleDeviceDescriptor?.let {
                if (it == "builtin") {
                    val vibrator = getSystemService(Context.VIBRATOR_SERVICE) as Vibrator
                    vibrators[index] = vibrator
                    vibrator
                } else {
                    for (id in InputDevice.getDeviceIds()) {
                        val device = InputDevice.getDevice(id)
                        if (device.descriptor == inputManager.controllers[index]!!.rumbleDeviceDescriptor) {
                            vibrators[index] = device.vibrator
                            device.vibrator
                        }
                    }
                }
            }
            return
        }

        val effect = VibrationEffect.createWaveform(timing, amplitude, 0)
        vibrator.vibrate(effect)
    }

    @Suppress("unused")
    fun clearVibrationDevice(index : Int) {
        vibrators[index]?.cancel()
    }
}
