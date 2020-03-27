/*
 * SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline

import android.content.Intent
import android.os.Bundle
import android.util.Log
import android.view.Menu
import android.view.MenuItem
import android.widget.ListView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.widget.SearchView
import androidx.preference.PreferenceManager
import emu.skyline.adapter.LogAdapter
import org.json.JSONObject
import java.io.*
import java.net.URL
import java.nio.charset.StandardCharsets
import java.util.stream.Collectors
import javax.net.ssl.HttpsURLConnection

class LogActivity : AppCompatActivity() {
    private lateinit var logFile: File
    private lateinit var adapter: LogAdapter

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.log_activity)
        setSupportActionBar(findViewById(R.id.toolbar))
        supportActionBar?.setDisplayHomeAsUpEnabled(true)
        val prefs = PreferenceManager.getDefaultSharedPreferences(this)
        val logList = findViewById<ListView>(R.id.log_list)
        adapter = LogAdapter(this, prefs.getBoolean("log_compact", false), prefs.getString("log_level", "3")!!.toInt(), resources.getStringArray(R.array.log_level))
        logList.adapter = adapter
        try {
            logFile = File("${applicationInfo.dataDir}/skyline.log")
            logFile.forEachLine {
                adapter.add(it)
            }
        } catch (e: FileNotFoundException) {
            Log.w("Logger", "IO Error during access of log file: " + e.message)
            Toast.makeText(applicationContext, getString(R.string.file_missing), Toast.LENGTH_LONG).show()
            finish()
        } catch (e: IOException) {
            Log.w("Logger", "IO Error during access of log file: " + e.message)
            Toast.makeText(applicationContext, getString(R.string.io_error) + ": " + e.message, Toast.LENGTH_LONG).show()
        }
    }

    override fun onCreateOptionsMenu(menu: Menu): Boolean {
        menuInflater.inflate(R.menu.toolbar_log, menu)
        val mSearch = menu.findItem(R.id.action_search_log)
        val searchView = mSearch.actionView as SearchView
        searchView.isSubmitButtonEnabled = false
        searchView.setOnQueryTextListener(object : SearchView.OnQueryTextListener {
            override fun onQueryTextSubmit(query: String): Boolean {
                searchView.isIconified = false
                return false
            }

            override fun onQueryTextChange(newText: String): Boolean {
                adapter.filter.filter(newText)
                return true
            }
        })
        return super.onCreateOptionsMenu(menu)
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        return when (item.itemId) {
            R.id.action_clear -> {
                try {
                    logFile.writeText("")
                } catch (e: IOException) {
                    Log.w("Logger", "IO Error while clearing the log file: " + e.message)
                    Toast.makeText(applicationContext, getString(R.string.io_error) + ": " + e.message, Toast.LENGTH_LONG).show()
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

    private fun uploadAndShareLog() {
        val shareThread = Thread(Runnable {
            var urlConnection: HttpsURLConnection? = null
            try {
                val url = URL("https://hastebin.com/documents")
                urlConnection = url.openConnection() as HttpsURLConnection
                urlConnection.requestMethod = "POST"
                urlConnection.setRequestProperty("Host", "hastebin.com")
                urlConnection.setRequestProperty("Content-Type", "application/json; charset=utf-8")
                urlConnection.setRequestProperty("Referer", "https://hastebin.com/")
                val bufferedWriter = urlConnection.outputStream.bufferedWriter()
                bufferedWriter.write(logFile.readText())
                bufferedWriter.flush()
                bufferedWriter.close()
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
            } catch (e: Exception) {
                runOnUiThread { Toast.makeText(applicationContext, getString(R.string.share_error), Toast.LENGTH_LONG).show() }
                e.printStackTrace()
            } finally {
                urlConnection!!.disconnect()
            }
        })
        shareThread.start()
        try {
            shareThread.join(1000)
        } catch (e: Exception) {
            Toast.makeText(applicationContext, getString(R.string.share_error), Toast.LENGTH_LONG).show()
            e.printStackTrace()
        }
    }
}
