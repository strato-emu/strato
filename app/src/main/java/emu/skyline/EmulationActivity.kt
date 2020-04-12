/*
 * SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.os.ParcelFileDescriptor
import android.util.Log
import android.view.Surface
import android.view.SurfaceHolder
import android.view.WindowManager
import androidx.appcompat.app.AppCompatActivity
import emu.skyline.loader.getRomFormat
import kotlinx.android.synthetic.main.app_activity.*
import java.io.File

/**
 * This activity is used for emulation using libskyline
 */
class EmulationActivity : AppCompatActivity(), SurfaceHolder.Callback {
    init {
        System.loadLibrary("skyline") // libskyline.so
    }

    /**
     * The file descriptor of the ROM
     */
    private lateinit var romFd: ParcelFileDescriptor

    /**
     * The file descriptor of the application Preference XML
     */
    private lateinit var preferenceFd: ParcelFileDescriptor

    /**
     * The file descriptor of the Log file
     */
    private lateinit var logFd: ParcelFileDescriptor

    /**
     * The surface object used for displaying frames
     */
    private var surface: Surface? = null

    /**
     * A boolean flag denoting if the emulation thread should call finish() or not
     */
    private var shouldFinish: Boolean = true

    /**
     * The Kotlin thread on which emulation code executes
     */
    private lateinit var emulationThread: Thread

    /**
     * This is the entry point into the emulation code for libskyline
     *
     * @param romUri The URI of the ROM as a string, used to print out in the logs
     * @param romType The type of the ROM as an enum value
     * @param romFd The file descriptor of the ROM object
     * @param preferenceFd The file descriptor of the Preference XML
     * @param logFd The file descriptor of the Log file
     */
    private external fun executeRom(romUri: String, romType: Int, romFd: Int, preferenceFd: Int, logFd: Int)

    /**
     * This sets the halt flag in libskyline to the provided value, if set to true it causes libskyline to halt emulation
     *
     * @param halt The value to set halt to
     */
    private external fun setHalt(halt: Boolean)

    /**
     * This sets the surface object in libskyline to the provided value, emulation is halted if set to null
     *
     * @param surface The value to set surface to
     */
    private external fun setSurface(surface: Surface?)

    /**
     * This executes the specified ROM, [preferenceFd] and [logFd] are assumed to be valid beforehand
     *
     * @param rom The URI of the ROM to execute
     */
    private fun executeRom(rom : Uri) {
        val romType = getRomFormat(rom, contentResolver).ordinal
        romFd = contentResolver.openFileDescriptor(rom, "r")!!

        emulationThread = Thread {
            while ((surface == null))
                Thread.yield()

            executeRom(Uri.decode(rom.toString()), romType, romFd.fd, preferenceFd.fd, logFd.fd)

            if (shouldFinish)
                runOnUiThread { finish() }
        }

        emulationThread.start()
    }

    /**
     * The onCreate handler for the activity, it sets up the FDs and calls [executeRom]
     */
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        setContentView(R.layout.game_activity)

        val preference = File("${applicationInfo.dataDir}/shared_prefs/${applicationInfo.packageName}_preferences.xml")
        preferenceFd = ParcelFileDescriptor.open(preference, ParcelFileDescriptor.MODE_READ_WRITE)

        val log = File("${applicationInfo.dataDir}/skyline.log")
        logFd = ParcelFileDescriptor.open(log, ParcelFileDescriptor.MODE_CREATE or ParcelFileDescriptor.MODE_READ_WRITE)

        window.setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN, WindowManager.LayoutParams.FLAG_FULLSCREEN)
        game_view.holder.addCallback(this)

        executeRom(intent.data!!)
    }

    /**
     * The onNewIntent handler is used to stop the currently executing ROM and replace it with the one specified in the new intent
     */
    override fun onNewIntent(intent: Intent?) {
        shouldFinish = false

        setHalt(true)
        emulationThread.join()

        shouldFinish = true

        romFd.close()

        executeRom(intent?.data!!)

        super.onNewIntent(intent)
    }

    /**
     * The onDestroy handler is used to halt emulation
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
     * The surfaceCreated handler passes in the surface to libskyline
     */
    override fun surfaceCreated(holder: SurfaceHolder?) {
        Log.d("surfaceCreated", "Holder: ${holder.toString()}")
        surface = holder!!.surface
        setSurface(surface)
    }

    /**
     * The surfaceChanged handler is purely used for debugging purposes
     */
    override fun surfaceChanged(holder: SurfaceHolder?, format: Int, width: Int, height: Int) {
        Log.d("surfaceChanged", "Holder: ${holder.toString()}, Format: $format, Width: $width, Height: $height")
    }

    /**
     * The surfaceDestroyed handler passes sets the surface to null
     */
    override fun surfaceDestroyed(holder: SurfaceHolder?) {
        Log.d("surfaceDestroyed", "Holder: ${holder.toString()}")
        surface = null
        setSurface(surface)
    }
}
