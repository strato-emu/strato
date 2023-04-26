/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline

import android.content.Intent
import android.graphics.Color
import android.net.Uri
import android.os.Bundle
import android.provider.DocumentsContract
import android.view.View
import androidx.activity.OnBackPressedCallback
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.viewModels
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.app.AppCompatDelegate
import androidx.core.content.res.use
import androidx.core.view.WindowCompat
import androidx.core.view.isInvisible
import androidx.core.view.isVisible
import androidx.documentfile.provider.DocumentFile
import androidx.preference.PreferenceManager
import androidx.recyclerview.widget.GridLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.snackbar.Snackbar
import dagger.hilt.android.AndroidEntryPoint
import emu.skyline.adapter.*
import emu.skyline.data.AppItem
import emu.skyline.data.AppItemTag
import emu.skyline.databinding.MainActivityBinding
import emu.skyline.loader.AppEntry
import emu.skyline.loader.LoaderResult
import emu.skyline.provider.DocumentsProvider
import emu.skyline.settings.AppSettings
import emu.skyline.settings.EmulationSettings
import emu.skyline.settings.SettingsActivity
import emu.skyline.utils.GpuDriverHelper
import emu.skyline.utils.WindowInsetsHelper
import javax.inject.Inject
import kotlin.math.ceil

@AndroidEntryPoint
class MainActivity : AppCompatActivity() {

    private val binding by lazy { MainActivityBinding.inflate(layoutInflater) }

    @Inject
    lateinit var appSettings : AppSettings

    private val adapter = GenericAdapter()

    private val layoutType get() = LayoutType.values()[appSettings.layoutType]

    private val viewModel by viewModels<MainViewModel>()

    private var appEntries : List<AppEntry>? = null

    enum class SortingOrder {
        AlphabeticalAsc,
        AlphabeticalDesc
    }

    private var refreshIconVisible = false
        set(visible) {
            field = visible
            binding.refreshIcon.apply {
                if (visible != isVisible) {
                    binding.refreshIcon.alpha = if (visible) 0f else 1f
                    animate().alpha(if (visible) 1f else 0f).withStartAction { isVisible = true }.withEndAction { isInvisible = !visible }.apply { duration = 500 }.start()
                }
            }
        }

    private val documentPicker = registerForActivityResult(ActivityResultContracts.OpenDocumentTree()) {
        it?.let { uri ->
            contentResolver.takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION)
            appSettings.searchLocation = uri.toString()

            loadRoms(false)
        }
    }

    private val settingsCallback = registerForActivityResult(ActivityResultContracts.StartActivityForResult()) {
        if (appSettings.refreshRequired) loadRoms(false)
    }

    private fun AppItem.toViewItem() = AppViewItem(layoutType, this, ::selectStartGame, ::selectShowGameDialog)

    override fun onCreate(savedInstanceState : Bundle?) {
        // Need to create new instance of settings, dependency injection happens
        AppCompatDelegate.setDefaultNightMode(
            when ((AppSettings(this).appTheme)) {
                0 -> AppCompatDelegate.MODE_NIGHT_NO
                1 -> AppCompatDelegate.MODE_NIGHT_YES
                2 -> AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM
                else -> AppCompatDelegate.MODE_NIGHT_UNSPECIFIED
            }
        )
        super.onCreate(savedInstanceState)

        setContentView(binding.root)
        WindowCompat.setDecorFitsSystemWindows(window, false)
        WindowInsetsHelper.applyToActivity(binding.root, binding.appList)

        PreferenceManager.setDefaultValues(this, R.xml.app_preferences, false)
        PreferenceManager.setDefaultValues(this, R.xml.emulation_preferences, false)

        binding.appList.setHasFixedSize(true)

        setupAppList()

        binding.swipeRefreshLayout.apply {
            setProgressBackgroundColorSchemeColor(obtainStyledAttributes(intArrayOf(R.attr.colorSurfaceVariant)).use { it.getColor(0, Color.BLACK) })
            setColorSchemeColors(obtainStyledAttributes(intArrayOf(R.attr.colorPrimary)).use { it.getColor(0, Color.WHITE) })
            post { setDistanceToTriggerSync(binding.swipeRefreshLayout.height / 3) }
            setOnRefreshListener { loadRoms(false) }
        }

        viewModel.stateData.observe(this, ::handleState)
        loadRoms(!appSettings.refreshRequired)

        binding.searchBar.apply {
            binding.logIcon.setOnClickListener {
                val file = DocumentFile.fromSingleUri(this@MainActivity, DocumentsContract.buildDocumentUri(DocumentsProvider.AUTHORITY, "${DocumentsProvider.ROOT_ID}/logs/emulation.sklog"))!!
                if (file.exists() && file.length() != 0L) {
                    val intent = Intent(Intent.ACTION_SEND)
                        .setDataAndType(file.uri, "text/plain")
                        .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
                        .putExtra(Intent.EXTRA_STREAM, file.uri)
                    startActivity(Intent.createChooser(intent, getString(R.string.log_share_prompt)))
                } else {
                    Snackbar.make(this@MainActivity.findViewById(android.R.id.content), getString(R.string.logs_not_found), Snackbar.LENGTH_SHORT).show()
                }
            }
            binding.settingsIcon.setOnClickListener { settingsCallback.launch(Intent(context, SettingsActivity::class.java)) }
            binding.refreshIcon.setOnClickListener { loadRoms(false) }
            addTextChangedListener(afterTextChanged = { editable ->
                editable?.let { text -> adapter.filter.filter(text.toString()) }
            })
        }

        window.decorView.findViewById<View>(android.R.id.content).viewTreeObserver.addOnTouchModeChangeListener { isInTouchMode ->
            refreshIconVisible = !isInTouchMode
        }
    }

    private fun setAppListDecoration() {
        binding.appList.apply {
            while (itemDecorationCount > 0) removeItemDecorationAt(0)
            when (layoutType) {
                LayoutType.List -> Unit

                LayoutType.Grid, LayoutType.GridCompact -> addItemDecoration(GridSpacingItemDecoration(resources.getDimensionPixelSize(R.dimen.grid_padding)))
            }
        }
    }

    /**
     * This layout manager handles situations where [onFocusSearchFailed] gets called, when possible we always want to focus on the item with the same span index
     */
    private inner class CustomLayoutManager(gridSpan : Int) : GridLayoutManager(this, gridSpan) {
        init {
            spanSizeLookup = object : SpanSizeLookup() {
                override fun getSpanSize(position : Int) = if (layoutType == LayoutType.List || adapter.currentItems[position].fullSpan) gridSpan else 1
            }
        }

        override fun onRequestChildFocus(parent : RecyclerView, state : RecyclerView.State, child : View, focused : View?) : Boolean {
            binding.appBarLayout.setExpanded(false)
            return super.onRequestChildFocus(parent, state, child, focused)
        }

        override fun onFocusSearchFailed(focused : View, focusDirection : Int, recycler : RecyclerView.Recycler, state : RecyclerView.State) : View? {
            val nextFocus = super.onFocusSearchFailed(focused, focusDirection, recycler, state)
            when (focusDirection) {
                View.FOCUS_DOWN -> {
                    return null
                }

                View.FOCUS_UP -> {
                    if (nextFocus?.isFocusable != true) {
                        binding.searchBar.requestFocus()
                        binding.appBarLayout.setExpanded(true)
                        binding.appList.smoothScrollToPosition(0)
                        return null
                    }
                }
            }
            return nextFocus
        }
    }

    private fun setupAppList() {
        binding.appList.adapter = adapter

        val itemWidth = 225
        val metrics = resources.displayMetrics
        val gridSpan = ceil((metrics.widthPixels / metrics.density) / itemWidth).toInt()

        binding.appList.layoutManager = CustomLayoutManager(gridSpan)
        setAppListDecoration()

        if (appSettings.searchLocation.isEmpty()) documentPicker.launch(null)
    }

    private fun getAppItems() = mutableListOf<AppViewItem>().apply {
        appEntries?.let { entries ->
            sortGameList(entries.toList()).forEach { entry ->
                add(AppItem(entry).toViewItem())
            }
        }
    }

    private fun sortGameList(gameList : List<AppEntry>) : List<AppEntry> {
        val sortedApps : MutableList<AppEntry> = mutableListOf()
        gameList.forEach { entry -> sortedApps.add(entry) }
        when (appSettings.sortAppsBy) {
            SortingOrder.AlphabeticalAsc.ordinal -> sortedApps.sortBy { it.name }
            SortingOrder.AlphabeticalDesc.ordinal -> sortedApps.sortByDescending { it.name }
        }
        return sortedApps
    }

    private fun handleState(state : MainState) = when (state) {
        MainState.Loading -> {
            binding.refreshIcon.apply { animate().rotation(rotation - 180f) }
            binding.swipeRefreshLayout.isRefreshing = true
        }

        is MainState.Loaded -> {
            binding.swipeRefreshLayout.isRefreshing = false

            appEntries = state.items
            populateAdapter()
        }

        is MainState.Error -> Snackbar.make(findViewById(android.R.id.content), getString(R.string.error) + ": ${state.ex.localizedMessage}", Snackbar.LENGTH_SHORT).show()
    }

    private fun selectStartGame(appItem : AppItem) {
        if (binding.swipeRefreshLayout.isRefreshing) return

        if (appSettings.selectAction) {
            AppDialog.newInstance(appItem).show(supportFragmentManager, "game")
        } else if (appItem.loaderResult == LoaderResult.Success) {
            startActivity(Intent(this, EmulationActivity::class.java).apply {
                putExtra(AppItemTag, appItem)
                putExtra(EmulationActivity.ReturnToMainTag, true)
            })
        }
    }

    private fun selectShowGameDialog(appItem : AppItem) {
        if (binding.swipeRefreshLayout.isRefreshing) return

        AppDialog.newInstance(appItem).show(supportFragmentManager, "game")
    }

    private fun loadRoms(loadFromFile : Boolean) {
        if (!loadFromFile) {
            binding.romPlaceholder.isVisible = true
            binding.romPlaceholder.text = getString(R.string.searching_roms)
        }
        viewModel.loadRoms(this, loadFromFile, Uri.parse(appSettings.searchLocation), EmulationSettings.global.systemLanguage)
        appSettings.refreshRequired = false
    }

    private fun populateAdapter() {
        val items = getAppItems()
        if (items.isEmpty())
            binding.romPlaceholder.text = getString(R.string.no_rom)
        else
            binding.romPlaceholder.isVisible = false
        adapter.setItems(items)
    }

    override fun onStart() {
        super.onStart()

        onBackPressedDispatcher.addCallback(object : OnBackPressedCallback(true) {
            override fun handleOnBackPressed() {
                binding.searchBar.apply {
                    if (hasFocus() && text.isNotEmpty()) {
                        text = ""
                        clearFocus()
                    } else {
                        finish()
                    }
                }
            }
        })
    }

    override fun onResume() {
        super.onResume()

        // Try to return to normal GPU clocks upon resuming back to main activity, to avoid GPU being stuck at max clocks after a crash
        GpuDriverHelper.forceMaxGpuClocks(false)

        var layoutTypeChanged = false
        for (appViewItem in adapter.allItems.filterIsInstance(AppViewItem::class.java)) {
            if (layoutType != appViewItem.layoutType) {
                appViewItem.layoutType = layoutType
                layoutTypeChanged = true
            } else {
                break
            }
        }

        if (layoutTypeChanged) {
            setAppListDecoration()
            adapter.notifyItemRangeChanged(0, adapter.currentItems.size)
        }

        viewModel.checkRomHash(Uri.parse(appSettings.searchLocation), EmulationSettings.global.systemLanguage)
    }
}
