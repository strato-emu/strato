package emu.skyline

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.os.ParcelFileDescriptor
import android.util.Log
import android.view.InputQueue
import android.view.Surface
import android.view.SurfaceHolder
import android.view.WindowManager
import androidx.appcompat.app.AppCompatActivity
import emu.skyline.loader.getTitleFormat
import kotlinx.android.synthetic.main.game_activity.*
import java.io.File
import java.lang.reflect.Method

class GameActivity : AppCompatActivity(), SurfaceHolder.Callback, InputQueue.Callback {
    init {
        System.loadLibrary("skyline") // libskyline.so
    }

    private lateinit var romFd: ParcelFileDescriptor
    private lateinit var preferenceFd: ParcelFileDescriptor
    private lateinit var logFd: ParcelFileDescriptor
    private var surface: Surface? = null
    private var inputQueue: Long = 0L
    private var shouldFinish: Boolean = true
    private lateinit var gameThread: Thread

    private external fun executeRom(romString: String, romType: Int, romFd: Int, preferenceFd: Int, logFd: Int)
    private external fun setHalt(halt: Boolean)
    private external fun setSurface(surface: Surface?)

    fun executeRom(rom : Uri) {
        val romType = getTitleFormat(rom, contentResolver).ordinal
        romFd = contentResolver.openFileDescriptor(rom, "r")!!
        gameThread = Thread {
            while ((surface == null))
                Thread.yield()
            executeRom(Uri.decode(rom.toString()), romType, romFd.fd, preferenceFd.fd, logFd.fd)
            if (shouldFinish)
                runOnUiThread { finish() }
        }
        gameThread.start()
    }

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

    override fun onNewIntent(intent: Intent?) {
        shouldFinish = false
        setHalt(true)
        gameThread.join()
        shouldFinish = true
        romFd.close()
        executeRom(intent?.data!!)
        super.onNewIntent(intent)
    }

    override fun onDestroy() {
        shouldFinish = false
        setHalt(true)
        gameThread.join()
        romFd.close()
        preferenceFd.close()
        logFd.close()
        super.onDestroy()
    }

    override fun surfaceCreated(holder: SurfaceHolder?) {
        Log.d("surfaceCreated", "Holder: ${holder.toString()}")
        surface = holder!!.surface
        setSurface(surface)
    }

    override fun surfaceChanged(holder: SurfaceHolder?, format: Int, width: Int, height: Int) {
        Log.d("surfaceChanged", "Holder: ${holder.toString()}, Format: $format, Width: $width, Height: $height")
    }

    override fun surfaceDestroyed(holder: SurfaceHolder?) {
        Log.d("surfaceDestroyed", "Holder: ${holder.toString()}")
        surface = null
        setSurface(surface)
    }

    override fun onInputQueueCreated(queue: InputQueue?) {
        Log.i("onInputQueueCreated", "InputQueue: ${queue.toString()}")
        val clazz = Class.forName("android.view.InputQueue")
        val method: Method = clazz.getMethod("getNativePtr")
        inputQueue = method.invoke(queue)!! as Long
        //setQueue(inputQueue)
    }

    override fun onInputQueueDestroyed(queue: InputQueue?) {
        Log.d("onInputQueueDestroyed", "InputQueue: ${queue.toString()}")
        inputQueue = 0L
        //setQueue(inputQueue)
    }
}
