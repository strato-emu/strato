/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2023 Strato Team and Contributors (https://github.com/strato-emu/)
 */

package emu.skyline.preference

import android.content.Intent
import android.os.Bundle
import android.util.Log
import android.view.ViewTreeObserver
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.core.view.WindowCompat
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.viewbinding.ViewBinding
import com.google.android.material.snackbar.Snackbar
import dagger.hilt.android.AndroidEntryPoint
import emu.skyline.R
import emu.skyline.adapter.GenericListItem
import emu.skyline.adapter.SearchLocationViewItem
import emu.skyline.adapter.SelectableGenericAdapter
import emu.skyline.adapter.SpacingItemDecoration
import emu.skyline.data.AppItem
import emu.skyline.data.AppItemTag
import emu.skyline.databinding.SearchLocationActivityBinding
import emu.skyline.settings.AppSettings
import emu.skyline.utils.SearchLocationHelper
import emu.skyline.utils.SearchLocationResult
import emu.skyline.utils.WindowInsetsHelper
import emu.skyline.utils.serializable

/**
 * This activity is used to manage the selected search locations to use.
 */
@AndroidEntryPoint
class SearchLocationActivity : AppCompatActivity() {
    private val binding by lazy { SearchLocationActivityBinding.inflate(layoutInflater) }

    private val item by lazy { intent.extras?.serializable(AppItemTag) as AppItem? }

    private val adapter = SelectableGenericAdapter(0)

    lateinit var appSettings : AppSettings

    /**
     * The callback called after a user picked a search location to add.
     */
    private val documentPicker = registerForActivityResult(ActivityResultContracts.OpenDocumentTree()) {
        it?.let { uri ->
            Log.i("Uri selected", uri.toString())
            contentResolver.takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION)
            val result = SearchLocationHelper.addLocation(this@SearchLocationActivity, uri)
            runOnUiThread {
                Snackbar.make(binding.root, resolveActionResultString(result), Snackbar.LENGTH_LONG).show()
                if (result == SearchLocationResult.Success)
                    populateAdapter()
            }
        }
    }

    /**
     * Updates the [adapter] with the current list of search locations.
     */
    private fun populateAdapter() {
        val items : MutableList<GenericListItem<out ViewBinding>> = ArrayList();

        SearchLocationHelper.getSearchLocations(this).onEach { uri ->
            items.add(SearchLocationViewItem(uri).apply {
                onDelete = { position ->
                    Snackbar.make(binding.root, getString(R.string.search_location_delete_success), Snackbar.LENGTH_LONG).setAction(R.string.undo) {
                        this@SearchLocationActivity.adapter.run {
                            addItemAt(position, this@apply)
                        }
                    }.addCallback(object : Snackbar.Callback() {
                        override fun onDismissed(transientBottomBar : Snackbar?, event : Int) {
                            if (event != DISMISS_EVENT_ACTION) {
                                SearchLocationHelper.deleteLocation(this@SearchLocationActivity, uri!!)
                            }
                        }
                    }).show()
                };
            })
        }

        adapter.setItems(items)
    }

    override fun onCreate(savedInstanceState : Bundle?) {
        super.onCreate(savedInstanceState)

        setContentView(binding.root)
        WindowCompat.setDecorFitsSystemWindows(window, false)
        WindowInsetsHelper.applyToActivity(binding.root, binding.locationsList)
        WindowInsetsHelper.addMargin(binding.addLocationButton, bottom = true)

        setSupportActionBar(binding.titlebar.toolbar)
        supportActionBar?.apply {
            setDisplayHomeAsUpEnabled(true)
            title = getString(R.string.search_location_config)
            subtitle = item?.title
        }

        val layoutManager = LinearLayoutManager(this)
        binding.locationsList.layoutManager = layoutManager
        binding.locationsList.adapter = adapter

        var layoutDone = false // Tracks if the layout is complete to avoid retrieving invalid attributes
        binding.coordinatorLayout.viewTreeObserver.addOnTouchModeChangeListener { isTouchMode ->
            val layoutUpdate = {
                val params = binding.locationsList.layoutParams as CoordinatorLayout.LayoutParams
                if (!isTouchMode) {
                    binding.titlebar.appBarLayout.setExpanded(true)
                    params.height = binding.coordinatorLayout.height - binding.titlebar.toolbar.height
                } else {
                    params.height = CoordinatorLayout.LayoutParams.MATCH_PARENT
                }

                binding.locationsList.layoutParams = params
                binding.locationsList.requestLayout()
            }

            if (!layoutDone) {
                binding.coordinatorLayout.viewTreeObserver.addOnGlobalLayoutListener(object : ViewTreeObserver.OnGlobalLayoutListener {
                    override fun onGlobalLayout() {
                        // We need to wait till the layout is done to get the correct height of the toolbar
                        binding.coordinatorLayout.viewTreeObserver.removeOnGlobalLayoutListener(this)
                        layoutUpdate()
                        layoutDone = true
                    }
                })
            } else {
                layoutUpdate()
            }
        }

        binding.locationsList.addItemDecoration(SpacingItemDecoration(resources.getDimensionPixelSize(R.dimen.grid_padding)))

        binding.addLocationButton.setOnClickListener {
            documentPicker.launch(null)
        }

        populateAdapter()
    }

    private fun resolveActionResultString(result : SearchLocationResult) = when (result) {
        SearchLocationResult.Success -> getString(R.string.search_location_add_success)
        SearchLocationResult.Deleted -> getString(R.string.search_location_delete_success)
        SearchLocationResult.AlreadyAdded -> getString(R.string.search_location_duplicated)
    }
}
