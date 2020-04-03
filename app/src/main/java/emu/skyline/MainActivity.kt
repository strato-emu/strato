// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

package emu.skyline

import android.content.Intent
import android.content.SharedPreferences
import android.net.Uri
import android.os.Bundle
import android.util.Log
import android.view.Menu
import android.view.MenuItem
import android.view.View
import android.widget.AdapterView
import android.widget.AdapterView.OnItemClickListener
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.app.AppCompatDelegate
import androidx.appcompat.widget.SearchView
import androidx.documentfile.provider.DocumentFile
import androidx.preference.PreferenceManager
import com.google.android.material.snackbar.Snackbar
import emu.skyline.adapter.AppAdapter
import emu.skyline.adapter.AppItem
import emu.skyline.loader.BaseLoader
import emu.skyline.loader.NroLoader
import emu.skyline.utility.GameDialog
import emu.skyline.utility.RandomAccessDocument
import kotlinx.android.synthetic.main.main_activity.*
import java.io.File
import java.io.IOException
import kotlin.concurrent.thread

class MainActivity : AppCompatActivity(), View.OnClickListener {
    private lateinit var sharedPreferences: SharedPreferences
    private var adapter = AppAdapter(this)

    private fun notifyUser(text: String) {
        Snackbar.make(findViewById(android.R.id.content), text, Snackbar.LENGTH_SHORT).show()
    }

    private fun findFile(ext: String, loader: BaseLoader, directory: DocumentFile, found: Boolean = false): Boolean {
        var foundCurrent = found

        directory.listFiles()
                .forEach { file ->
                    if (file.isDirectory) {
                        foundCurrent = findFile(ext, loader, file, foundCurrent)
                    } else {
                        if (ext.equals(file.name?.substringAfterLast("."), ignoreCase = true)) {
                            val document = RandomAccessDocument(this, file)
                            if (loader.verifyFile(document)) {
                                val entry = loader.getAppEntry(document, file.uri)
                                runOnUiThread {
                                    if (!foundCurrent) {
                                        adapter.addHeader(loader.format.name)
                                        foundCurrent = true
                                    }
                                    adapter.addItem(AppItem(entry))
                                }
                            }
                            document.close()
                        }
                    }
                }

        return foundCurrent
    }

    private fun refreshFiles(tryLoad: Boolean) {
        if (tryLoad) {
            try {
                adapter.load(File("${applicationInfo.dataDir}/roms.bin"))
                return
            } catch (e: Exception) {
                Log.w("refreshFiles", "Ran into exception while loading: ${e.message}")
            }
        }

        thread(start = true) {
            val snackbar = Snackbar.make(findViewById(android.R.id.content), getString(R.string.searching_roms), Snackbar.LENGTH_INDEFINITE)
            runOnUiThread { snackbar.show() }

            try {
                runOnUiThread { adapter.clear() }
                val foundNros = findFile("nro", NroLoader(this), DocumentFile.fromTreeUri(this, Uri.parse(sharedPreferences.getString("search_location", "")))!!)

                runOnUiThread {
                    if (!foundNros)
                        adapter.addHeader(getString(R.string.no_rom))

                    try {
                        adapter.save(File("${applicationInfo.dataDir}/roms.bin"))
                    } catch (e: IOException) {
                        Log.w("refreshFiles", "Ran into exception while saving: ${e.message}")
                    }
                }

                sharedPreferences.edit().putBoolean("refresh_required", false).apply()
            } catch (e: IllegalArgumentException) {
                runOnUiThread {
                    sharedPreferences.edit().remove("search_location").apply()

                    val intent = intent
                    finish()
                    startActivity(intent)
                }
            } catch (e: Exception) {
                runOnUiThread {
                    notifyUser(e.message!!)
                }
            }

            runOnUiThread { snackbar.dismiss() }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.main_activity)
        PreferenceManager.setDefaultValues(this, R.xml.preferences, false)
        sharedPreferences = PreferenceManager.getDefaultSharedPreferences(this)
        AppCompatDelegate.setDefaultNightMode(when ((sharedPreferences.getString("app_theme", "2")?.toInt())) {
            0 -> AppCompatDelegate.MODE_NIGHT_NO
            1 -> AppCompatDelegate.MODE_NIGHT_YES
            2 -> AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM
            else -> AppCompatDelegate.MODE_NIGHT_UNSPECIFIED
        })
        setSupportActionBar(toolbar)
        open_fab.setOnClickListener(this)
        log_fab.setOnClickListener(this)
        game_list.adapter = adapter
        game_list.onItemClickListener = OnItemClickListener { parent: AdapterView<*>, _: View?, position: Int, _: Long ->
            val item = parent.getItemAtPosition(position)
            if (item is AppItem) {
                val intent = Intent(this, EmulationActivity::class.java)
                intent.data = item.uri
                startActivity(intent)
            }
        }
        game_list.onItemLongClickListener = AdapterView.OnItemLongClickListener { parent, _, position, _ ->
            val item = parent.getItemAtPosition(position)
            if (item is AppItem) {
                val dialog = GameDialog(item)
                dialog.show(supportFragmentManager, "game")
            }
            true
        }
        if (sharedPreferences.getString("search_location", "") == "") {
            val intent = Intent(Intent.ACTION_OPEN_DOCUMENT_TREE)
            intent.flags = Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION or Intent.FLAG_GRANT_PREFIX_URI_PERMISSION or Intent.FLAG_GRANT_READ_URI_PERMISSION
            startActivityForResult(intent, 1)
        } else
            refreshFiles(!sharedPreferences.getBoolean("refresh_required", false))
    }

    override fun onCreateOptionsMenu(menu: Menu): Boolean {
        menuInflater.inflate(R.menu.toolbar_main, menu)
        val mSearch = menu.findItem(R.id.action_search_main)
        val searchView = mSearch.actionView as SearchView
        searchView.isSubmitButtonEnabled = false
        searchView.setOnQueryTextListener(object : SearchView.OnQueryTextListener {
            override fun onQueryTextSubmit(query: String): Boolean {
                searchView.clearFocus()
                return false
            }

            override fun onQueryTextChange(newText: String): Boolean {
                adapter.filter.filter(newText)
                return true
            }
        })
        return super.onCreateOptionsMenu(menu)
    }

    override fun onClick(view: View) {
        when (view.id) {
            R.id.log_fab -> startActivity(Intent(this, LogActivity::class.java))
            R.id.open_fab -> {
                val intent = Intent(Intent.ACTION_OPEN_DOCUMENT)
                intent.addCategory(Intent.CATEGORY_OPENABLE)
                intent.type = "*/*"
                startActivityForResult(intent, 2)
            }
        }
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        return when (item.itemId) {
            R.id.action_settings -> {
                startActivityForResult(Intent(this, SettingsActivity::class.java), 3)
                true
            }
            R.id.action_refresh -> {
                refreshFiles(false)
                notifyUser(getString(R.string.refreshed))
                true
            }
            else -> super.onOptionsItemSelected(item)
        }
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, intent: Intent?) {
        super.onActivityResult(requestCode, resultCode, intent)
        if (resultCode == RESULT_OK) {
            when (requestCode) {
                1 -> {
                    val uri = intent!!.data!!
                    contentResolver.takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION)
                    sharedPreferences.edit().putString("search_location", uri.toString()).apply()
                    refreshFiles(!sharedPreferences.getBoolean("refresh_required", false))
                }
                2 -> {
                    try {
                        val uri = (intent!!.data!!)
                        val intentGame = Intent(this, EmulationActivity::class.java)
                        intentGame.data = uri
                        if (resultCode != 0)
                            startActivityForResult(intentGame, resultCode)
                        else
                            startActivity(intentGame)
                    } catch (e: Exception) {
                        notifyUser(e.message!!)
                    }
                }
                3 -> {
                    if (sharedPreferences.getBoolean("refresh_required", false))
                        refreshFiles(false)
                }
            }
        }
    }
}
