/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline

import android.content.Intent
import android.os.Bundle
import android.util.Log
import android.view.KeyEvent
import android.view.Menu
import android.view.MenuItem
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.widget.SearchView
import androidx.recyclerview.widget.DividerItemDecoration
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.snackbar.Snackbar
import dagger.hilt.android.AndroidEntryPoint
import emu.skyline.adapter.GenericAdapter
import emu.skyline.adapter.HeaderViewItem
import emu.skyline.adapter.LogViewItem
import emu.skyline.databinding.LogActivityBinding
import emu.skyline.utils.Settings
import org.json.JSONObject
import java.io.File
import java.io.FileNotFoundException
import java.io.IOException
import java.net.URL
import javax.inject.Inject
import javax.net.ssl.HttpsURLConnection

@AndroidEntryPoint
class LogActivity : AppCompatActivity() {
    private val binding by lazy { LogActivityBinding.inflate(layoutInflater) }

    /**
     * The log file is used to read log entries from or to clear all entries
     */
    private lateinit var logFile : File

    private val adapter = GenericAdapter()

    @Inject
    lateinit var settings : Settings

    override fun onCreate(savedInstanceState : Bundle?) {
        super.onCreate(savedInstanceState)

        setContentView(binding.root)

        setSupportActionBar(binding.titlebar.toolbar)
        supportActionBar?.setDisplayHomeAsUpEnabled(true)

        val compact = settings.logCompact
        val logLevel = settings.logLevel.toInt()
        val logLevels = resources.getStringArray(R.array.log_level)

        binding.logList.adapter = adapter

        if (!compact) binding.logList.addItemDecoration(DividerItemDecoration(this, RecyclerView.VERTICAL))

        try {
            logFile = File(applicationContext.filesDir.canonicalPath + "/skyline.log")

            adapter.setItems(logFile.readLines().mapNotNull { logLine ->
                try {
                    val logMeta = logLine.split("|", limit = 3)

                    if (logMeta[0].startsWith("1")) {
                        val level = logMeta[1].toInt()
                        if (level > logLevel) return@mapNotNull null

                        return@mapNotNull LogViewItem(compact, "(" + logMeta[2] + ") " + logMeta[3].replace('\\', '\n'), logLevels[level])
                    } else {
                        return@mapNotNull HeaderViewItem(logMeta[1])
                    }
                } catch (ignored : IndexOutOfBoundsException) {
                } catch (ignored : NumberFormatException) {
                }
                null
            })
        } catch (e : FileNotFoundException) {
            Log.w("Logger", "IO Error during access of log file: " + e.message)
            Toast.makeText(applicationContext, getString(R.string.file_missing), Toast.LENGTH_LONG).show()

            finish()
        } catch (e : IOException) {
            Log.w("Logger", "IO Error during access of log file: " + e.message)
            Toast.makeText(applicationContext, getString(R.string.error) + ": ${e.localizedMessage}", Toast.LENGTH_LONG).show()
        }
    }

    /**
     * This inflates the layout for the menu [R.menu.toolbar_log] and sets up searching the logs
     */
    override fun onCreateOptionsMenu(menu : Menu) : Boolean {
        menuInflater.inflate(R.menu.toolbar_log, menu)

        val searchView = menu.findItem(R.id.action_search_log).actionView as SearchView
        searchView.isSubmitButtonEnabled = false

        searchView.setOnQueryTextListener(object : SearchView.OnQueryTextListener {
            override fun onQueryTextSubmit(query : String) : Boolean {
                searchView.isIconified = false
                return false
            }

            override fun onQueryTextChange(newText : String) : Boolean {
                adapter.filter.filter(newText)
                return true
            }
        })

        return super.onCreateOptionsMenu(menu)
    }

    /**
     * This handles menu selection for [R.id.action_clear] and [R.id.action_share_log]
     */
    override fun onOptionsItemSelected(item : MenuItem) : Boolean {
        return when (item.itemId) {
            R.id.action_clear -> {
                try {
                    logFile.writeText("")
                } catch (e : IOException) {
                    Log.w("Logger", "IO Error while clearing the log file: " + e.message)
                    Toast.makeText(applicationContext, getString(R.string.error) + ": ${e.localizedMessage}", Toast.LENGTH_LONG).show()
                }

                Toast.makeText(applicationContext, getString(R.string.cleared), Toast.LENGTH_LONG).show()
                finish()
                true
            }

            R.id.action_share_log -> {
                uploadAndShareLog()
                true
            }

            else -> super.onOptionsItemSelected(item)
        }
    }

    /**
     * This uploads the logs and launches the [Intent.ACTION_SEND] intent
     */
    private fun uploadAndShareLog() {
        Snackbar.make(findViewById(android.R.id.content), getString(R.string.upload_logs), Snackbar.LENGTH_SHORT).show()

        val shareThread = Thread {
            var urlConnection : HttpsURLConnection? = null

            try {
                val url = URL("https://hastebin.com/documents")

                urlConnection = url.openConnection() as HttpsURLConnection
                urlConnection.requestMethod = "POST"
                urlConnection.setRequestProperty("Host", "hastebin.com")
                urlConnection.setRequestProperty("Content-Type", "application/json; charset=utf-8")
                urlConnection.setRequestProperty("Referer", "https://hastebin.com/")

                urlConnection.outputStream.bufferedWriter().use {
                    it.write(logFile.readText())
                    it.flush()
                }

                if (urlConnection.responseCode != 200) {
                    Log.e("LogUpload", "HTTPS Status Code: " + urlConnection.responseCode)
                    throw Exception()
                }

                val bufferedReader = urlConnection.inputStream.bufferedReader()
                val key = JSONObject(bufferedReader.readText()).getString("key")
                bufferedReader.close()

                val result = "https://hastebin.com/$key"
                val sharingIntent = Intent(Intent.ACTION_SEND).setType("text/plain").putExtra(Intent.EXTRA_TEXT, result)

                startActivity(Intent.createChooser(sharingIntent, "Share log url with:"))
            } catch (e : Exception) {
                runOnUiThread { Snackbar.make(findViewById(android.R.id.content), getString(R.string.error) + ": ${e.localizedMessage}", Snackbar.LENGTH_LONG).show() }
                e.printStackTrace()
            } finally {
                urlConnection!!.disconnect()
            }
        }

        shareThread.start()
    }

    /**
     * This handles on calling [onBackPressed] when [KeyEvent.KEYCODE_BUTTON_B] is lifted
     */
    override fun onKeyUp(keyCode : Int, event : KeyEvent?) : Boolean {
        if (keyCode == KeyEvent.KEYCODE_BUTTON_B) {
            onBackPressed()
            return true
        }

        return super.onKeyUp(keyCode, event)
    }
}
