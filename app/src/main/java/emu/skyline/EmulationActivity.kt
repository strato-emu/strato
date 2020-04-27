/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline

import android.annotation.SuppressLint
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.ParcelFileDescriptor
import android.util.Log
import android.view.*
import androidx.appcompat.app.AppCompatActivity
import androidx.preference.PreferenceManager
import emu.skyline.input.ButtonState
import emu.skyline.input.NpadAxis
import emu.skyline.input.NpadButton
import emu.skyline.loader.getRomFormat
import kotlinx.android.synthetic.main.emu_activity.*
import java.io.File
import kotlin.math.abs

class EmulationActivity : AppCompatActivity(), SurfaceHolder.Callback {
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
     * The surface object used for displaying frames
     */
    private var surface : Surface? = null

    /**
     * A boolean flag denoting if the emulation thread should call finish() or not
     */
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
     * This sets the state of a specific button
     */
    private external fun setButtonState(id : Long, state : Int)

    /**
     * This sets the value of a specific axis
     */
    private external fun setAxisValue(id : Int, value : Int)

    /**
     * This executes the specified ROM, [preferenceFd] is assumed to be valid beforehand
     *
     * @param rom The URI of the ROM to execute
     */
    private fun executeApplication(rom : Uri) {
        val romType = getRomFormat(rom, contentResolver).ordinal
        romFd = contentResolver.openFileDescriptor(rom, "r")!!

        emulationThread = Thread {
            while ((surface == null))
                Thread.yield()

            executeApplication(Uri.decode(rom.toString()), romType, romFd.fd, preferenceFd.fd, applicationContext.filesDir.canonicalPath + "/")

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
            window.decorView.systemUiVisibility = (View.SYSTEM_UI_FLAG_IMMERSIVE
                    or View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                    or View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                    or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                    or View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                    or View.SYSTEM_UI_FLAG_FULLSCREEN)
        }

        val preference = File("${applicationInfo.dataDir}/shared_prefs/${applicationInfo.packageName}_preferences.xml")
        preferenceFd = ParcelFileDescriptor.open(preference, ParcelFileDescriptor.MODE_READ_WRITE)

        game_view.holder.addCallback(this)

        val sharedPreferences = PreferenceManager.getDefaultSharedPreferences(this)

        if (sharedPreferences.getBoolean("perf_stats", false)) {
            lateinit var perfRunnable : Runnable

            perfRunnable = Runnable {
                perf_stats.text = "${getFps()} FPS\n${getFrametime()}ms"
                perf_stats.postDelayed(perfRunnable, 250)
            }

            perf_stats.postDelayed(perfRunnable, 250)
        }

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
        emulationThread.join()

        romFd.close()
        preferenceFd.close()

        super.onDestroy()
    }

    /**
     * This sets [surface] to [holder].surface and passes it into libskyline
     */
    override fun surfaceCreated(holder : SurfaceHolder) {
        Log.d("surfaceCreated", "Holder: ${holder.toString()}")
        surface = holder.surface
        setSurface(surface)
    }

    /**
     * This is purely used for debugging surface changes
     */
    override fun surfaceChanged(holder : SurfaceHolder, format : Int, width : Int, height : Int) {
        Log.d("surfaceChanged", "Holder: ${holder.toString()}, Format: $format, Width: $width, Height: $height")
    }

    /**
     * This sets [surface] to null and passes it into libskyline
     */
    override fun surfaceDestroyed(holder : SurfaceHolder) {
        Log.d("surfaceDestroyed", "Holder: ${holder.toString()}")
        surface = null
        setSurface(surface)
    }

    /**
     * This handles passing on any key events to libskyline
     */
    override fun dispatchKeyEvent(event : KeyEvent) : Boolean {
        val action : ButtonState = when (event.action) {
            KeyEvent.ACTION_DOWN -> ButtonState.Pressed
            KeyEvent.ACTION_UP -> ButtonState.Released
            else -> return false
        }

        val buttonMap : Map<Int, NpadButton> = mapOf(
                KeyEvent.KEYCODE_BUTTON_A to NpadButton.A,
                KeyEvent.KEYCODE_BUTTON_B to NpadButton.B,
                KeyEvent.KEYCODE_BUTTON_X to NpadButton.X,
                KeyEvent.KEYCODE_BUTTON_Y to NpadButton.Y,
                KeyEvent.KEYCODE_BUTTON_THUMBL to NpadButton.LeftStick,
                KeyEvent.KEYCODE_BUTTON_THUMBR to NpadButton.RightStick,
                KeyEvent.KEYCODE_BUTTON_L1 to NpadButton.L,
                KeyEvent.KEYCODE_BUTTON_R1 to NpadButton.R,
                KeyEvent.KEYCODE_BUTTON_L2 to NpadButton.ZL,
                KeyEvent.KEYCODE_BUTTON_R2 to NpadButton.ZR,
                KeyEvent.KEYCODE_BUTTON_START to NpadButton.Plus,
                KeyEvent.KEYCODE_BUTTON_SELECT to NpadButton.Minus,
                KeyEvent.KEYCODE_DPAD_DOWN to NpadButton.DpadDown,
                KeyEvent.KEYCODE_DPAD_UP to NpadButton.DpadUp,
                KeyEvent.KEYCODE_DPAD_LEFT to NpadButton.DpadLeft,
                KeyEvent.KEYCODE_DPAD_RIGHT to NpadButton.DpadRight)

        return try {
            setButtonState(buttonMap.getValue(event.keyCode).value(), action.ordinal);
            true
        } catch (ignored : NoSuchElementException) {
            super.dispatchKeyEvent(event)
        }
    }

    /**
     * This is the controller HAT X value
     */
    private var controllerHatX : Float = 0.0f

    /**
     * This is the controller HAT Y value
     */
    private var controllerHatY : Float = 0.0f

    /**
     * This handles passing on any motion events to libskyline
     */
    override fun dispatchGenericMotionEvent(event : MotionEvent) : Boolean {
        if ((event.source and InputDevice.SOURCE_DPAD) == InputDevice.SOURCE_DPAD ||
                (event.source and InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK) {
            val hatXMap : Map<Float, NpadButton> = mapOf(
                    -1.0f to NpadButton.DpadLeft,
                    +1.0f to NpadButton.DpadRight)

            val hatYMap : Map<Float, NpadButton> = mapOf(
                    -1.0f to NpadButton.DpadUp,
                    +1.0f to NpadButton.DpadDown)

            if (controllerHatX != event.getAxisValue(MotionEvent.AXIS_HAT_X)) {
                if (event.getAxisValue(MotionEvent.AXIS_HAT_X) == 0.0f)
                    setButtonState(hatXMap.getValue(controllerHatX).value(), ButtonState.Released.ordinal)
                else
                    setButtonState(hatXMap.getValue(event.getAxisValue(MotionEvent.AXIS_HAT_X)).value(), ButtonState.Pressed.ordinal)

                controllerHatX = event.getAxisValue(MotionEvent.AXIS_HAT_X)

                return true
            }

            if (controllerHatY != event.getAxisValue(MotionEvent.AXIS_HAT_Y)) {
                if (event.getAxisValue(MotionEvent.AXIS_HAT_Y) == 0.0f)
                    setButtonState(hatYMap.getValue(controllerHatY).value(), ButtonState.Released.ordinal)
                else
                    setButtonState(hatYMap.getValue(event.getAxisValue(MotionEvent.AXIS_HAT_Y)).value(), ButtonState.Pressed.ordinal)

                controllerHatY = event.getAxisValue(MotionEvent.AXIS_HAT_Y)

                return true
            }
        }

        if ((event.source and InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK && event.action == MotionEvent.ACTION_MOVE) {
            val axisMap : Map<Int, NpadAxis> = mapOf(
                    MotionEvent.AXIS_X to NpadAxis.LX,
                    MotionEvent.AXIS_Y to NpadAxis.LY,
                    MotionEvent.AXIS_Z to NpadAxis.RX,
                    MotionEvent.AXIS_RZ to NpadAxis.RY)

            //TODO: Digital inputs based off of analog sticks
            event.device.motionRanges.forEach {
                if (axisMap.containsKey(it.axis)) {
                    var axisValue : Float = event.getAxisValue(it.axis)
                    if (abs(axisValue) <= it.flat)
                        axisValue = 0.0f

                    val ratio : Float = axisValue / (it.max - it.min)
                    val rangedAxisValue : Int = (ratio * (Short.MAX_VALUE - Short.MIN_VALUE)).toInt()

                    setAxisValue(axisMap.getValue(it.axis).ordinal, rangedAxisValue)
                }
            }
            return true
        }

        return super.dispatchGenericMotionEvent(event)
    }
}
