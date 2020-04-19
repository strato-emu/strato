/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline

import android.content.Intent
import android.content.SharedPreferences
import android.net.Uri
import android.os.Bundle
import android.util.Log
import android.view.Menu
import android.view.MenuItem
import android.view.View
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.app.AppCompatDelegate
import androidx.appcompat.widget.SearchView
import androidx.documentfile.provider.DocumentFile
import androidx.preference.PreferenceManager
import androidx.recyclerview.widget.DividerItemDecoration
import androidx.recyclerview.widget.GridLayoutManager
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.snackbar.Snackbar
import emu.skyline.adapter.AppAdapter
import emu.skyline.adapter.AppItem
import emu.skyline.adapter.GridLayoutSpan
import emu.skyline.adapter.LayoutType
import emu.skyline.loader.BaseLoader
import emu.skyline.loader.NroLoader
import emu.skyline.utility.AppDialog
import emu.skyline.utility.RandomAccessDocument
import kotlinx.android.synthetic.main.main_activity.*
import kotlinx.android.synthetic.main.titlebar.*
import java.io.File
import java.io.IOException
import kotlin.concurrent.thread
import kotlin.math.ceil

class MainActivity : AppCompatActivity(), View.OnClickListener, View.OnLongClickListener {
    /**
     * This is used to get/set shared preferences
     */
    private lateinit var sharedPreferences: SharedPreferences

    /**
     * The adapter used for adding elements to [app_list]
     */
    private lateinit var adapter: AppAdapter

    /**
     * This adds all files in [directory] with [extension] as an entry in [adapter] using [loader] to load metadata
     */
    private fun addEntries(extension: String, loader: BaseLoader, directory: DocumentFile, found: Boolean = false): Boolean {
        var foundCurrent = found

        directory.listFiles().forEach { file ->
            if (file.isDirectory) {
                foundCurrent = addEntries(extension, loader, file, foundCurrent)
            } else {
                if (extension.equals(file.name?.substringAfterLast("."), ignoreCase = true)) {
                    val document = RandomAccessDocument(this, file)

                    if (loader.verifyFile(document)) {
                        val entry = loader.getAppEntry(document, file.uri)

                        runOnUiThread {
                            if (!foundCurrent) {
                                adapter.addHeader(loader.format.name)
                            }

                            adapter.addItem(AppItem(entry))
                        }

                        foundCurrent = true
                    }

                    document.close()
                }
            }
        }

        return foundCurrent
    }

    /**
     * This refreshes the contents of the adapter by either trying to load cached adapter data or searches for them to recreate a list
     *
     * @param tryLoad If this is false then trying to load cached adapter data is skipped entirely
     */
    private fun refreshAdapter(tryLoad: Boolean) {
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

                val foundNros = addEntries("nro", NroLoader(this), DocumentFile.fromTreeUri(this, Uri.parse(sharedPreferences.getString("search_location", "")))!!)

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
                    Snackbar.make(findViewById(android.R.id.content), getString(R.string.error) + ": ${e.localizedMessage}", Snackbar.LENGTH_SHORT).show()
                }
            }

            runOnUiThread { snackbar.dismiss() }
        }
    }

    /**
     * This initializes [toolbar], [open_fab], [log_fab] and [app_list]
     */
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        setContentView(R.layout.main_activity)

        setSupportActionBar(toolbar)

        PreferenceManager.setDefaultValues(this, R.xml.preferences, false)
        sharedPreferences = PreferenceManager.getDefaultSharedPreferences(this)

        AppCompatDelegate.setDefaultNightMode(when ((sharedPreferences.getString("app_theme", "2")?.toInt())) {
            0 -> AppCompatDelegate.MODE_NIGHT_NO
            1 -> AppCompatDelegate.MODE_NIGHT_YES
            2 -> AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM
            else -> AppCompatDelegate.MODE_NIGHT_UNSPECIFIED
        })

        open_fab.setOnClickListener(this)
        log_fab.setOnClickListener(this)

        val layoutType = LayoutType.values()[sharedPreferences.getString("layout_type", "1")!!.toInt()]

        adapter = AppAdapter(this, layoutType)
        app_list.adapter = adapter

        when (layoutType) {
            LayoutType.List -> {
                app_list.layoutManager = LinearLayoutManager(this)
                app_list.addItemDecoration(DividerItemDecoration(this, RecyclerView.VERTICAL))
            }

            LayoutType.Grid -> {
                val itemWidth = 225
                val metrics = resources.displayMetrics
                val span = ceil((metrics.widthPixels / metrics.density) / itemWidth).toInt()

                val layoutManager = GridLayoutManager(this, span)
                layoutManager.spanSizeLookup = GridLayoutSpan(adapter, span)

                app_list.layoutManager = layoutManager
            }
        }

        if (sharedPreferences.getString("search_location", "") == "") {
            val intent = Intent(Intent.ACTION_OPEN_DOCUMENT_TREE)
            intent.flags = Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION or Intent.FLAG_GRANT_PREFIX_URI_PERMISSION or Intent.FLAG_GRANT_READ_URI_PERMISSION

            startActivityForResult(intent, 1)
        } else
            refreshAdapter(!sharedPreferences.getBoolean("refresh_required", false))
    }

    /**
     * This inflates the layout for the menu [R.menu.toolbar_main] and sets up searching the logs
     */
    override fun onCreateOptionsMenu(menu: Menu): Boolean {
        menuInflater.inflate(R.menu.toolbar_main, menu)

        val searchView = menu.findItem(R.id.action_search_main).actionView as SearchView

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

    /**
     * This handles on-click interaction with [R.id.log_fab], [R.id.open_fab], [R.id.app_item_linear] and [R.id.app_item_grid]
     */
    override fun onClick(view: View) {
        when (view.id) {
            R.id.log_fab -> startActivity(Intent(this, LogActivity::class.java))

            R.id.open_fab -> {
                val intent = Intent(Intent.ACTION_OPEN_DOCUMENT)
                intent.addCategory(Intent.CATEGORY_OPENABLE)
                intent.type = "*/*"

                startActivityForResult(intent, 2)
            }

            R.id.app_item_linear, R.id.app_item_grid -> {
                val tag = view.tag
                if (tag is AppItem) {
                    if (sharedPreferences.getBoolean("select_action", false)) {
                        val dialog = AppDialog(tag)
                        dialog.show(supportFragmentManager, "game")
                    } else {
                        val intent = Intent(this, EmulationActivity::class.java)
                        intent.data = tag.uri
                        startActivity(intent)
                    }
                }
            }
        }
    }

    /**
     * This handles long-click interaction with [R.id.app_item_linear] and [R.id.app_item_grid]
     */
    override fun onLongClick(view: View?): Boolean {
        when (view?.id) {
            R.id.app_item_linear, R.id.app_item_grid -> {
                val tag = view.tag
                if (tag is AppItem) {
                    val dialog = AppDialog(tag)
                    dialog.show(supportFragmentManager, "game")

                    return true
                }
            }
        }

        return false
    }

    /**
     * This handles menu interaction for [R.id.action_settings] and [R.id.action_refresh]
     */
    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        return when (item.itemId) {
            R.id.action_settings -> {
                startActivityForResult(Intent(this, SettingsActivity::class.java), 3)
                true
            }

            R.id.action_refresh -> {
                refreshAdapter(false)
                true
            }

            else -> super.onOptionsItemSelected(item)
        }
    }

    /**
     * This handles receiving activity result from [Intent.ACTION_OPEN_DOCUMENT_TREE], [Intent.ACTION_OPEN_DOCUMENT] and [SettingsActivity]
     */
    override fun onActivityResult(requestCode: Int, resultCode: Int, intent: Intent?) {
        super.onActivityResult(requestCode, resultCode, intent)

        if (resultCode == RESULT_OK) {
            when (requestCode) {
                1 -> {
                    val uri = intent!!.data!!
                    contentResolver.takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION)
                    sharedPreferences.edit().putString("search_location", uri.toString()).apply()

                    refreshAdapter(!sharedPreferences.getBoolean("refresh_required", false))
                }

                2 -> {
                    try {
                        val intentGame = Intent(this, EmulationActivity::class.java)
                        intentGame.data = intent!!.data!!

                        if (resultCode != 0)
                            startActivityForResult(intentGame, resultCode)
                        else
                            startActivity(intentGame)
                    } catch (e: Exception) {
                        Snackbar.make(findViewById(android.R.id.content), getString(R.string.error) + ": ${e.localizedMessage}", Snackbar.LENGTH_SHORT).show()
                    }
                }

                3 -> {
                    if (sharedPreferences.getBoolean("refresh_required", false))
                        refreshAdapter(false)
                }
            }
        }
    }
}
