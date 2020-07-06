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
import emu.skyline.loader.getRomFormat
import kotlinx.android.synthetic.main.emu_activity.*
import java.io.File

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
     * The file descriptor of the Log file
     */
    private lateinit var logFd : ParcelFileDescriptor

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
     * @param logFd The file descriptor of the Log file
     */
    private external fun executeApplication(romUri : String, romType : Int, romFd : Int, preferenceFd : Int, logFd : Int)

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
     * This executes the specified ROM, [preferenceFd] and [logFd] are assumed to be valid beforehand
     *
     * @param rom The URI of the ROM to execute
     */
    private fun executeApplication(rom : Uri) {
        val romType = getRomFormat(rom, contentResolver).ordinal
        romFd = contentResolver.openFileDescriptor(rom, "r")!!

        emulationThread = Thread {
            while ((surface == null))
                Thread.yield()

            executeApplication(Uri.decode(rom.toString()), romType, romFd.fd, preferenceFd.fd, logFd.fd)

            if (shouldFinish)
                runOnUiThread { finish() }
        }

        emulationThread.start()
    }

    /**
     * This makes the window fullscreen then sets up [preferenceFd] and [logFd], sets up the performance statistics and finally calls [executeApplication] for executing the application
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

        val log = File("${applicationInfo.dataDir}/skyline.log")
        logFd = ParcelFileDescriptor.open(log, ParcelFileDescriptor.MODE_CREATE or ParcelFileDescriptor.MODE_READ_WRITE)

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
        logFd.close()

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
}
