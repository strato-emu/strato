/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline

import android.animation.ObjectAnimator
import android.content.Intent
import android.content.SharedPreferences
import android.content.pm.ActivityInfo
import android.net.Uri
import android.os.Bundle
import android.util.Log
import android.view.Menu
import android.view.MenuItem
import android.view.View
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.app.AppCompatDelegate
import androidx.appcompat.widget.SearchView
import androidx.core.animation.doOnEnd
import androidx.documentfile.provider.DocumentFile
import androidx.preference.PreferenceManager
import androidx.recyclerview.widget.DividerItemDecoration
import androidx.recyclerview.widget.GridLayoutManager
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.snackbar.Snackbar
import emu.skyline.adapter.AppAdapter
import emu.skyline.adapter.GridLayoutSpan
import emu.skyline.adapter.LayoutType
import emu.skyline.data.AppItem
import emu.skyline.loader.RomFile
import emu.skyline.loader.RomFormat
import kotlinx.android.synthetic.main.main_activity.*
import kotlinx.android.synthetic.main.titlebar.*
import java.io.File
import java.io.IOException
import kotlin.concurrent.thread
import kotlin.math.ceil

class MainActivity : AppCompatActivity(), View.OnClickListener {
    /**
     * This is used to get/set shared preferences
     */
    private lateinit var sharedPreferences : SharedPreferences

    /**
     * The adapter used for adding elements to [app_list]
     */
    private lateinit var adapter : AppAdapter

    /**
     * This adds all files in [directory] with [extension] as an entry in [adapter] using [loader] to load metadata
     */
    private fun addEntries(extension : String, romFormat : RomFormat, directory : DocumentFile, found : Boolean = false) : Boolean {
        var foundCurrent = found

        directory.listFiles().forEach { file ->
            if (file.isDirectory) {
                foundCurrent = addEntries(extension, romFormat, file, foundCurrent)
            } else {
                if (extension.equals(file.name?.substringAfterLast("."), ignoreCase = true)) {
                    val romFd = contentResolver.openFileDescriptor(file.uri, "r")!!
                    val romFile = RomFile(this, romFormat, romFd)

                    if (romFile.valid()) {
                        romFile.use {
                            val entry = romFile.getAppEntry(file.uri)

                            val finalFoundCurrent = foundCurrent
                            runOnUiThread {
                                if (!finalFoundCurrent) adapter.addHeader(romFormat.name)

                                adapter.addItem(AppItem(entry))
                            }

                            foundCurrent = true
                        }
                    }

                    romFd.close()
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
    private fun refreshAdapter(tryLoad : Boolean) {
        if (tryLoad) {
            try {
                adapter.load(File(applicationContext.filesDir.canonicalPath + "/roms.bin"))
                return
            } catch (e : Exception) {
                Log.w("refreshFiles", "Ran into exception while loading: ${e.message}")
            }
        }

        thread(start = true) {
            val snackbar = Snackbar.make(coordinatorLayout, getString(R.string.searching_roms), Snackbar.LENGTH_INDEFINITE)
            runOnUiThread {
                snackbar.show()
                requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_LOCKED
            }

            try {
                runOnUiThread { adapter.clear() }

                val searchLocation = DocumentFile.fromTreeUri(this, Uri.parse(sharedPreferences.getString("search_location", "")))!!

                var foundRoms = addEntries("nro", RomFormat.NRO, searchLocation)
                foundRoms = foundRoms or addEntries("nso", RomFormat.NSO, searchLocation)
                foundRoms = foundRoms or addEntries("nca", RomFormat.NCA, searchLocation)
                foundRoms = foundRoms or addEntries("nsp", RomFormat.NSP, searchLocation)

                runOnUiThread {
                    if (!foundRoms) adapter.addHeader(getString(R.string.no_rom))

                    try {
                        adapter.save(File(applicationContext.filesDir.canonicalPath + "/roms.bin"))
                    } catch (e : IOException) {
                        Log.w("refreshFiles", "Ran into exception while saving: ${e.message}")
                    }
                }

                sharedPreferences.edit().putBoolean("refresh_required", false).apply()
            } catch (e : IllegalArgumentException) {
                runOnUiThread {
                    sharedPreferences.edit().remove("search_location").apply()

                    val intent = intent
                    finish()
                    startActivity(intent)
                }
            } catch (e : Exception) {
                runOnUiThread {
                    Snackbar.make(findViewById(android.R.id.content), getString(R.string.error) + ": ${e.localizedMessage}", Snackbar.LENGTH_SHORT).show()
                }
            }

            runOnUiThread {
                snackbar.dismiss()
                requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED
            }
        }
    }

    /**
     * This initializes [toolbar], [open_fab], [log_fab] and [app_list]
     */
    override fun onCreate(savedInstanceState : Bundle?) {
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

        refresh_fab.setOnClickListener(this)
        settings_fab.setOnClickListener(this)
        open_fab.setOnClickListener(this)
        log_fab.setOnClickListener(this)

        setupAppList()

        app_list.addOnScrollListener(object : RecyclerView.OnScrollListener() {
            var y = 0

            override fun onScrolled(recyclerView : RecyclerView, dx : Int, dy : Int) {
                y += dy

                if (!app_list.isInTouchMode)
                    toolbar_layout.setExpanded(y == 0)

                super.onScrolled(recyclerView, dx, dy)
            }
        })

        val controllerFabX = controller_fabs.translationX
        window.decorView.findViewById<View>(android.R.id.content).viewTreeObserver.addOnTouchModeChangeListener {
            if (!it) {
                toolbar_layout.setExpanded(false)

                controller_fabs.visibility = View.VISIBLE
                ObjectAnimator.ofFloat(controller_fabs, "translationX", 0f).apply {
                    duration = 250
                    start()
                }
            } else {
                ObjectAnimator.ofFloat(controller_fabs, "translationX", controllerFabX).apply {
                    duration = 250
                    start()
                }.doOnEnd { controller_fabs.visibility = View.GONE }
            }
        }
    }

    private fun setupAppList() {
        val itemWidth = 225
        val metrics = resources.displayMetrics
        val gridSpan = ceil((metrics.widthPixels / metrics.density) / itemWidth).toInt()

        val layoutType = LayoutType.values()[sharedPreferences.getString("layout_type", "1")!!.toInt()]

        adapter = AppAdapter(layoutType = layoutType, gridSpan = gridSpan, onClick = selectStartGame, onLongClick = selectShowGameDialog)
        app_list.adapter = adapter

        app_list.layoutManager = when (layoutType) {
            LayoutType.List -> LinearLayoutManager(this).also { app_list.addItemDecoration(DividerItemDecoration(this, RecyclerView.VERTICAL)) }

            LayoutType.Grid, LayoutType.GridCompact -> GridLayoutManager(this, gridSpan).apply {
                spanSizeLookup = GridLayoutSpan(adapter, gridSpan).also {
                    if (app_list.itemDecorationCount > 0) app_list.removeItemDecorationAt(0)
                }
            }
        }

        if (sharedPreferences.getString("search_location", "") == "") {
            val intent = Intent(Intent.ACTION_OPEN_DOCUMENT_TREE)
            intent.flags = Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION or Intent.FLAG_GRANT_PREFIX_URI_PERMISSION or Intent.FLAG_GRANT_READ_URI_PERMISSION

            startActivityForResult(intent, 1)
        } else {
            refreshAdapter(!sharedPreferences.getBoolean("refresh_required", false))
        }
    }

    /**
     * This inflates the layout for the menu [R.menu.toolbar_main] and sets up searching the logs
     */
    override fun onCreateOptionsMenu(menu : Menu) : Boolean {
        menuInflater.inflate(R.menu.toolbar_main, menu)

        val searchView = menu.findItem(R.id.action_search_main).actionView as SearchView

        searchView.setOnQueryTextListener(object : SearchView.OnQueryTextListener {
            override fun onQueryTextSubmit(query : String) : Boolean {
                searchView.clearFocus()
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
     * This handles on-click interaction with [R.id.refresh_fab], [R.id.settings_fab], [R.id.log_fab], [R.id.open_fab]
     */
    override fun onClick(view : View) {
        when (view.id) {
            R.id.refresh_fab -> refreshAdapter(false)

            R.id.settings_fab -> startActivityForResult(Intent(this, SettingsActivity::class.java), 3)

            R.id.log_fab -> startActivity(Intent(this, LogActivity::class.java))

            R.id.open_fab -> {
                val intent = Intent(Intent.ACTION_OPEN_DOCUMENT)
                intent.addCategory(Intent.CATEGORY_OPENABLE)
                intent.type = "*/*"

                startActivityForResult(intent, 2)
            }
        }
    }

    private val selectStartGame : (appItem : AppItem) -> Unit = {
        if (sharedPreferences.getBoolean("select_action", false)) {
            AppDialog(it).show(supportFragmentManager, "game")
        } else {
            startActivity(Intent(this, EmulationActivity::class.java).apply { data = it.uri })
        }
    }

    private val selectShowGameDialog : (appItem : AppItem) -> Unit = {
        AppDialog(it).show(supportFragmentManager, "game")
    }

    /**
     * This handles menu interaction for [R.id.action_settings] and [R.id.action_refresh]
     */
    override fun onOptionsItemSelected(item : MenuItem) : Boolean {
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
    override fun onActivityResult(requestCode : Int, resultCode : Int, intent : Intent?) {
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
                    } catch (e : Exception) {
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

    override fun onResume() {
        super.onResume()

        val layoutType = LayoutType.values()[sharedPreferences.getString("layout_type", "1")!!.toInt()]
        if (layoutType != adapter.layoutType) {
            setupAppList()
        }

        val gridCardMagin = resources.getDimensionPixelSize(R.dimen.app_card_margin_half)
        when (layoutType) {
            LayoutType.List -> app_list.setPadding(0, 0, 0, 0)
            LayoutType.Grid, LayoutType.GridCompact -> app_list.setPadding(gridCardMagin, 0, gridCardMagin, 0)
        }
    }
}
