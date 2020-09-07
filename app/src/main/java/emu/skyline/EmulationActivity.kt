/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline

import android.annotation.SuppressLint
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.*
import android.util.Log
import android.view.*
import androidx.appcompat.app.AppCompatActivity
import androidx.preference.PreferenceManager
import emu.skyline.input.*
import emu.skyline.loader.getRomFormat
import kotlinx.android.synthetic.main.emu_activity.*
import java.io.File
import kotlin.math.abs

class EmulationActivity : AppCompatActivity(), SurfaceHolder.Callback, View.OnTouchListener {
    init {
        System.loadLibrary("skyline") // libskyline.so
    }

    /**
     * The file descriptor of the ROM
     */
    private lateinit var romFd : ParcelFileDescriptor

    /**
     * The file descriptor of the application Preference XML
     */
    private lateinit var preferenceFd : ParcelFileDescriptor

    /**
     * The [InputManager] class handles loading/saving the input data
     */
    private lateinit var input : InputManager

    /**
     * A map of [Vibrator]s that correspond to [InputManager.controllers]
     */
    private var vibrators = HashMap<Int, Vibrator>()

    /**
     * A boolean flag denoting the current operation mode of the emulator (Docked = true/Handheld = false)
     */
    private var operationMode = true

    /**
     * The surface object used for displaying frames
     */
    @Volatile
    private var surface : Surface? = null

    /**
     * A condition variable keeping track of if the surface is ready or not
     */
    private var surfaceReady = ConditionVariable()

    /**
     * A boolean flag denoting if the emulation thread should call finish() or not
     */
    @Volatile
    private var shouldFinish : Boolean = true

    /**
     * The Kotlin thread on which emulation code executes
     */
    private lateinit var emulationThread : Thread

    /**
     * This is the entry point into the emulation code for libskyline
     *
     * @param romUri The URI of the ROM as a string, used to print out in the logs
     * @param romType The type of the ROM as an enum value
     * @param romFd The file descriptor of the ROM object
     * @param preferenceFd The file descriptor of the Preference XML
     * @param appFilesPath The full path to the app files directory
     */
    private external fun executeApplication(romUri : String, romType : Int, romFd : Int, preferenceFd : Int, appFilesPath : String)

    /**
     * This sets the halt flag in libskyline to the provided value, if set to true it causes libskyline to halt emulation
     *
     * @param halt The value to set halt to
     */
    private external fun setHalt(halt : Boolean)

    /**
     * This sets the surface object in libskyline to the provided value, emulation is halted if set to null
     *
     * @param surface The value to set surface to
     */
    private external fun setSurface(surface : Surface?)

    /**
     * This returns the current FPS of the application
     */
    private external fun getFps() : Int

    /**
     * This returns the current frame-time of the application
     */
    private external fun getFrametime() : Float

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
     * This initializes all of the controllers from [input] on the guest
     */
    private fun initializeControllers() {
        for (entry in input.controllers) {
            val controller = entry.value

            if (controller.type != ControllerType.None) {
                val type = when (controller.type) {
                    ControllerType.None -> throw IllegalArgumentException()
                    ControllerType.HandheldProController -> if (operationMode) ControllerType.ProController.id else ControllerType.HandheldProController.id
                    ControllerType.ProController, ControllerType.JoyConLeft, ControllerType.JoyConRight -> controller.type.id
                }

                val partnerIndex = when (controller) {
                    is JoyConLeftController -> controller.partnerId
                    is JoyConRightController -> controller.partnerId
                    else -> null
                }

                setController(entry.key, type, partnerIndex ?: -1)
            }
        }

        updateControllers()
    }

    /**
     * This executes the specified ROM, [preferenceFd] is assumed to be valid beforehand
     *
     * @param rom The URI of the ROM to execute
     */
    private fun executeApplication(rom : Uri) {
        val romType = getRomFormat(rom, contentResolver).ordinal
        romFd = contentResolver.openFileDescriptor(rom, "r")!!

        emulationThread = Thread {
            surfaceReady.block()

            executeApplication(rom.toString(), romType, romFd.fd, preferenceFd.fd, applicationContext.filesDir.canonicalPath + "/")

            if (shouldFinish)
                runOnUiThread { finish() }
        }

        emulationThread.start()
    }

    /**
     * This makes the window fullscreen then sets up [preferenceFd], sets up the performance statistics and finally calls [executeApplication] for executing the application
     */
    @SuppressLint("SetTextI18n")
    override fun onCreate(savedInstanceState : Bundle?) {
        super.onCreate(savedInstanceState)

        setContentView(R.layout.emu_activity)

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            window.insetsController?.hide(WindowInsets.Type.navigationBars() or WindowInsets.Type.systemBars() or WindowInsets.Type.systemGestures() or WindowInsets.Type.statusBars())
            window.insetsController?.systemBarsBehavior = WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        } else {
            @Suppress("DEPRECATION")
            window.decorView.systemUiVisibility = (View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                    or View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                    or View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                    or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                    or View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                    or View.SYSTEM_UI_FLAG_FULLSCREEN)
        }

        input = InputManager(this)

        val preference = File("${applicationInfo.dataDir}/shared_prefs/${applicationInfo.packageName}_preferences.xml")
        preferenceFd = ParcelFileDescriptor.open(preference, ParcelFileDescriptor.MODE_READ_WRITE)

        game_view.holder.addCallback(this)

        val sharedPreferences = PreferenceManager.getDefaultSharedPreferences(this)

        if (sharedPreferences.getBoolean("perf_stats", false)) {
            perf_stats.postDelayed(object : Runnable {
                override fun run() {
                    perf_stats.text = "${getFps()} FPS\n${getFrametime()}ms"
                    perf_stats.postDelayed(this, 250)
                }
            }, 250)
        }

        operationMode = sharedPreferences.getBoolean("operation_mode", operationMode)

        @Suppress("DEPRECATION") val display = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) display!! else windowManager.defaultDisplay
        display?.supportedModes?.maxBy { it.refreshRate + (it.physicalHeight * it.physicalWidth) }?.let { window.attributes.preferredDisplayModeId = it.modeId }

        game_view.setOnTouchListener(this)

        executeApplication(intent.data!!)
    }

    /**
     * This is used to stop the currently executing ROM and replace it with the one specified in the new intent
     */
    override fun onNewIntent(intent : Intent?) {
        shouldFinish = false

        setHalt(true)
        emulationThread.join()

        shouldFinish = true

        romFd.close()

        executeApplication(intent?.data!!)

        super.onNewIntent(intent)
    }

    /**
     * This is used to halt emulation entirely
     */
    override fun onDestroy() {
        shouldFinish = false

        setHalt(true)
        emulationThread.join(1000)

        vibrators.forEach { (_, vibrator) -> vibrator.cancel() }
        vibrators.clear()

        romFd.close()
        preferenceFd.close()

        super.onDestroy()
    }

    /**
     * This sets [surface] to [holder].surface and passes it into libskyline
     */
    override fun surfaceCreated(holder : SurfaceHolder) {
        Log.d("surfaceCreated", "Holder: $holder")
        surface = holder.surface
        setSurface(surface)
        surfaceReady.open()
    }

    /**
     * This is purely used for debugging surface changes
     */
    override fun surfaceChanged(holder : SurfaceHolder, format : Int, width : Int, height : Int) {
        Log.d("surfaceChanged", "Holder: $holder, Format: $format, Width: $width, Height: $height")
    }

    /**
     * This sets [surface] to null and passes it into libskyline
     */
    override fun surfaceDestroyed(holder : SurfaceHolder) {
        Log.d("surfaceDestroyed", "Holder: $holder")
        surfaceReady.close()
        surface = null
        setSurface(surface)
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

        return when (val guestEvent = input.eventMap[KeyHostEvent(event.device.descriptor, event.keyCode)]) {
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
    private var oldHat = Pair(0.0f, 0.0f)

    /**
     * This handles translating any [MotionHostEvent]s to a [GuestEvent] that is passed into libskyline
     */
    override fun onGenericMotionEvent(event : MotionEvent) : Boolean {
        if ((event.isFromSource(InputDevice.SOURCE_CLASS_JOYSTICK) || event.isFromSource(InputDevice.SOURCE_CLASS_BUTTON)) && event.action == MotionEvent.ACTION_MOVE) {
            val hat = Pair(event.getAxisValue(MotionEvent.AXIS_HAT_X), event.getAxisValue(MotionEvent.AXIS_HAT_Y))

            if (hat == oldHat) {
                for (axisItem in MotionHostEvent.axes.withIndex()) {
                    val axis = axisItem.value
                    var value = event.getAxisValue(axis)

                    if ((event.historySize != 0 && value != event.getHistoricalAxisValue(axis, 0)) || (axesHistory[axisItem.index]?.let { it == value } == false)) {
                        var polarity = value >= 0

                        val guestEvent = input.eventMap[MotionHostEvent(event.device.descriptor, axis, polarity)] ?: if (value == 0f) {
                            polarity = false
                            input.eventMap[MotionHostEvent(event.device.descriptor, axis, polarity)]
                        } else {
                            null
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
        val count = if(event.action != MotionEvent.ACTION_UP && event.action != MotionEvent.ACTION_CANCEL) event.pointerCount else 0
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

    @SuppressLint("WrongConstant")
    fun vibrateDevice(index : Int, timing : LongArray, amplitude : IntArray) {
        val vibrator = if (vibrators[index] != null) {
            vibrators[index]!!
        } else {
            input.controllers[index]?.rumbleDeviceDescriptor?.let {
                if (it == "builtin") {
                    val vibrator = getSystemService(Context.VIBRATOR_SERVICE) as Vibrator
                    vibrators[index] = vibrator
                    vibrator
                } else {
                    for (id in InputDevice.getDeviceIds()) {
                        val device = InputDevice.getDevice(id)
                        if (device.descriptor == input.controllers[index]?.rumbleDeviceDescriptor) {
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

    fun clearVibrationDevice(index : Int) {
        vibrators[index]?.cancel()
    }
}
