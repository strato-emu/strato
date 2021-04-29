/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline

import android.content.Intent
import android.graphics.Color
import android.graphics.Rect
import android.net.Uri
import android.os.Bundle
import android.view.View
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.viewModels
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.app.AppCompatDelegate
import androidx.core.content.ContextCompat
import androidx.core.content.res.use
import androidx.core.graphics.drawable.toBitmap
import androidx.core.view.isInvisible
import androidx.core.view.isVisible
import androidx.core.view.size
import androidx.preference.PreferenceManager
import androidx.recyclerview.widget.GridLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.snackbar.Snackbar
import dagger.hilt.android.AndroidEntryPoint
import emu.skyline.adapter.*
import emu.skyline.data.AppItem
import emu.skyline.data.DataItem
import emu.skyline.data.HeaderItem
import emu.skyline.databinding.MainActivityBinding
import emu.skyline.loader.AppEntry
import emu.skyline.loader.LoaderResult
import emu.skyline.loader.RomFormat
import emu.skyline.utils.Settings
import javax.inject.Inject
import kotlin.math.ceil
import kotlin.math.roundToInt

@AndroidEntryPoint
class MainActivity : AppCompatActivity() {
    companion object {
        private val formatOrder = listOf(RomFormat.NSP, RomFormat.XCI, RomFormat.NRO, RomFormat.NSO, RomFormat.NCA)
    }

    private val binding by lazy { MainActivityBinding.inflate(layoutInflater) }

    @Inject
    lateinit var settings : Settings

    private val adapter = GenericAdapter()

    private val layoutType get() = LayoutType.values()[settings.layoutType.toInt()]

    private val missingIcon by lazy { ContextCompat.getDrawable(this, R.drawable.default_icon)!!.toBitmap(256, 256) }

    private val viewModel by viewModels<MainViewModel>()

    private var formatFilter : RomFormat? = null
    private var appEntries : Map<RomFormat, List<AppEntry>>? = null

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
            settings.searchLocation = uri.toString()

            loadRoms(false)
        }
    }

    private val settingsCallback = registerForActivityResult(ActivityResultContracts.StartActivityForResult()) {
        if (settings.refreshRequired) loadRoms(false)
    }

    private fun AppItem.toViewItem() = AppViewItem(layoutType, this, missingIcon, ::selectStartGame, ::selectShowGameDialog)

    override fun onCreate(savedInstanceState : Bundle?) {
        // Need to create new instance of settings, dependency injection happens
        AppCompatDelegate.setDefaultNightMode(
            when ((Settings(this).appTheme.toInt())) {
                0 -> AppCompatDelegate.MODE_NIGHT_NO
                1 -> AppCompatDelegate.MODE_NIGHT_YES
                2 -> AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM
                else -> AppCompatDelegate.MODE_NIGHT_UNSPECIFIED
            }
        )
        super.onCreate(savedInstanceState)

        setContentView(binding.root)

        PreferenceManager.setDefaultValues(this, R.xml.preferences, false)

        adapter.apply {
            setHeaderItems(listOf(HeaderRomFilterItem(formatOrder, if (settings.filter == 0) null else formatOrder[settings.filter - 1]) { romFormat ->
                settings.filter = romFormat?.let { formatOrder.indexOf(romFormat) + 1 } ?: 0
                formatFilter = romFormat
                populateAdapter()
            }))

            setOnFilterPublishedListener {
                binding.appList.post { binding.appList.smoothScrollToPosition(0) }
            }
        }

        setupAppList()

        binding.swipeRefreshLayout.apply {
            setProgressBackgroundColorSchemeColor(obtainStyledAttributes(intArrayOf(R.attr.colorPrimary)).use { it.getColor(0, Color.BLACK) })
            setColorSchemeColors(obtainStyledAttributes(intArrayOf(R.attr.colorAccent)).use { it.getColor(0, Color.BLACK) })
            post { setDistanceToTriggerSync((binding.swipeRefreshLayout.height / 2.5f).roundToInt()) }
            setOnRefreshListener { loadRoms(false) }
        }

        viewModel.stateData.observe(this, ::handleState)
        loadRoms(!settings.refreshRequired)

        binding.searchBar.apply {
            binding.logIcon.setOnClickListener { startActivity(Intent(context, LogActivity::class.java)) }
            binding.settingsIcon.setOnClickListener { settingsCallback.launch(Intent(context, SettingsActivity::class.java)) }
            binding.refreshIcon.setOnClickListener { loadRoms(false) }
            addTextChangedListener(afterTextChanged = { editable ->
                editable?.let { text -> adapter.filter.filter(text.toString()) }
            })
            if (!viewModel.searchBarAnimated) {
                viewModel.searchBarAnimated = true
                post { startTitleAnimation() }
            }
        }
        window.decorView.findViewById<View>(android.R.id.content).viewTreeObserver.addOnTouchModeChangeListener { isInTouchMode ->
            refreshIconVisible = !isInTouchMode
        }
    }

    private inner class GridSpacingItemDecoration : RecyclerView.ItemDecoration() {
        private val padding = resources.getDimensionPixelSize(R.dimen.grid_padding)

        override fun getItemOffsets(outRect : Rect, view : View, parent : RecyclerView, state : RecyclerView.State) {
            super.getItemOffsets(outRect, view, parent, state)

            val gridLayoutManager = parent.layoutManager as GridLayoutManager
            val layoutParams = view.layoutParams as GridLayoutManager.LayoutParams
            when (layoutParams.spanIndex) {
                0 -> outRect.left = padding

                gridLayoutManager.spanCount - 1 -> outRect.right = padding

                else -> {
                    outRect.left = padding / 2
                    outRect.right = padding / 2
                }
            }

            if (layoutParams.spanSize == gridLayoutManager.spanCount) {
                outRect.left = 0
                outRect.right = 0
            }
        }
    }

    private fun setAppListDecoration() {
        binding.appList.apply {
            while (itemDecorationCount > 0) removeItemDecorationAt(0)
            when (layoutType) {
                LayoutType.List -> Unit

                LayoutType.Grid, LayoutType.GridCompact -> addItemDecoration(GridSpacingItemDecoration())
            }
        }
    }

    /**
     * This layout manager handles situations where [onFocusSearchFailed] gets called, when possible we always want to focus on the item with the same span index
     */
    private inner class CustomLayoutManager(gridSpan : Int) : GridLayoutManager(this, gridSpan) {
        init {
            spanSizeLookup = object : GridLayoutManager.SpanSizeLookup() {
                override fun getSpanSize(position : Int) = if (layoutType == LayoutType.List || adapter.currentItems[position].fullSpan) gridSpan else 1
            }
        }

        override fun onFocusSearchFailed(focused : View, focusDirection : Int, recycler : RecyclerView.Recycler, state : RecyclerView.State) : View? {
            val nextFocus = super.onFocusSearchFailed(focused, focusDirection, recycler, state)
            when (focusDirection) {
                View.FOCUS_DOWN -> {
                    findContainingItemView(focused)?.let { focusedChild ->
                        val current = binding.appList.indexOfChild(focusedChild)
                        val currentSpanIndex = (focusedChild.layoutParams as LayoutParams).spanIndex
                        for (i in current + 1 until binding.appList.size) {
                            val candidate = getChildAt(i)!!
                            // Return candidate when span index matches
                            if (currentSpanIndex == (candidate.layoutParams as LayoutParams).spanIndex) return candidate
                        }
                        nextFocus?.let { if ((it.layoutParams as LayoutParams).spanIndex == currentSpanIndex) return nextFocus }

                        binding.appBarLayout.setExpanded(false) // End of list, hide app bar, so bottom row is fully visible
                        binding.appList.smoothScrollToPosition(adapter.itemCount)
                    }
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

        if (settings.searchLocation.isEmpty()) documentPicker.launch(null)
    }

    private fun getDataItems() = mutableListOf<DataItem>().apply {
        appEntries?.let { entries ->
            val formats = formatFilter?.let { listOf(it) } ?: formatOrder
            for (format in formats) {
                entries[format]?.let {
                    add(HeaderItem(format.name))
                    it.forEach { entry -> add(AppItem(entry)) }
                }
            }
        }
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

        if (settings.selectAction)
            AppDialog.newInstance(appItem).show(supportFragmentManager, "game")
        else if (appItem.loaderResult == LoaderResult.Success)
            startActivity(Intent(this, EmulationActivity::class.java).apply { data = appItem.uri })
    }

    private fun selectShowGameDialog(appItem : AppItem) {
        if (binding.swipeRefreshLayout.isRefreshing) return

        AppDialog.newInstance(appItem).show(supportFragmentManager, "game")
    }

    private fun loadRoms(loadFromFile : Boolean) {
        viewModel.loadRoms(loadFromFile, Uri.parse(settings.searchLocation))
        settings.refreshRequired = false
    }

    private fun populateAdapter() {
        val items = getDataItems()
        adapter.setItems(items.map {
            when (it) {
                is HeaderItem -> HeaderViewItem(it.title)
                is AppItem -> it.toViewItem()
            }
        })
        if (items.isEmpty()) adapter.setItems(listOf(HeaderViewItem(getString(R.string.no_rom))))
    }

    override fun onResume() {
        super.onResume()

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
    }

    override fun onBackPressed() {
        binding.searchBar.apply {
            if (hasFocus() && text.isNotEmpty()) {
                text = ""
                clearFocus()
            } else {
                super.onBackPressed()
            }
        }
    }
}
