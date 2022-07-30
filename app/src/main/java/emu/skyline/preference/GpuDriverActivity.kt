/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.preference

import android.content.Intent
import android.content.Intent.FLAG_GRANT_READ_URI_PERMISSION
import android.os.Bundle
import android.view.ViewTreeObserver
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.viewbinding.ViewBinding
import com.google.android.material.snackbar.Snackbar
import dagger.hilt.android.AndroidEntryPoint
import emu.skyline.R
import emu.skyline.adapter.GenericListItem
import emu.skyline.adapter.GpuDriverViewItem
import emu.skyline.adapter.SelectableGenericAdapter
import emu.skyline.adapter.SpacingItemDecoration
import emu.skyline.databinding.GpuDriverActivityBinding
import emu.skyline.utils.GpuDriverHelper
import emu.skyline.utils.GpuDriverInstallResult
import emu.skyline.utils.PreferenceSettings
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import javax.inject.Inject

/**
 * This activity is used to manage the installed gpu drivers and select one to use.
 */
@AndroidEntryPoint
class GpuDriverActivity : AppCompatActivity() {
    private val binding by lazy { GpuDriverActivityBinding.inflate(layoutInflater) }

    private val adapter = SelectableGenericAdapter(0)

    @Inject
    lateinit var preferenceSettings : PreferenceSettings

    /**
     * The callback called after a user picked a driver to install.
     */
    private val installCallback = registerForActivityResult(ActivityResultContracts.StartActivityForResult()) {
        if (it.resultCode == RESULT_OK) {
            it.data?.data?.let { uri ->
                val fileStream = contentResolver.openInputStream(uri) ?: return@let

                Snackbar.make(binding.root, getString(R.string.gpu_driver_install_inprogress), Snackbar.LENGTH_INDEFINITE).show()
                CoroutineScope(Dispatchers.IO).launch {
                    val result = GpuDriverHelper.installDriver(this@GpuDriverActivity, fileStream)
                    runOnUiThread {
                        Snackbar.make(binding.root, resolveInstallResultString(result), Snackbar.LENGTH_LONG).show()
                        if (result == GpuDriverInstallResult.SUCCESS)
                            populateAdapter()
                    }
                }
            }
        }
    }

    /**
     * Updates the [adapter] with the current list of installed drivers.
     */
    private fun populateAdapter() {
        val items : MutableList<GenericListItem<out ViewBinding>> = ArrayList()

        // Insert the system driver entry at the top of the list.
        items.add(GpuDriverViewItem(GpuDriverHelper.getSystemDriverMetadata(this)) {
            preferenceSettings.gpuDriver = PreferenceSettings.SYSTEM_GPU_DRIVER
        })

        if (preferenceSettings.gpuDriver == PreferenceSettings.SYSTEM_GPU_DRIVER) {
            adapter.selectedPosition = 0
        }

        GpuDriverHelper.getInstalledDrivers(this).onEachIndexed { index, (file, metadata) ->
            items.add(GpuDriverViewItem(metadata).apply {
                onDelete = { position, wasChecked ->
                    // If the driver was selected, select the system driver as the active one
                    if (wasChecked)
                        preferenceSettings.gpuDriver = PreferenceSettings.SYSTEM_GPU_DRIVER

                    Snackbar.make(binding.root, "${metadata.label} deleted", Snackbar.LENGTH_LONG).setAction(R.string.undo) {
                        this@GpuDriverActivity.adapter.run {
                            addItemAt(position, this@apply)
                            // If the item was selected before removal, set it back as the active one when undoing
                            if (wasChecked) {
                                // Only notify previous to avoid notifying items before indexes have updated, the newly inserted item will be updated on bind
                                selectAndNotifyPrevious(position)
                                preferenceSettings.gpuDriver = metadata.label
                            }
                        }
                    }.addCallback(object : Snackbar.Callback() {
                        override fun onDismissed(transientBottomBar : Snackbar?, event : Int) {
                            // Only delete the driver directory if the user didn't undo the deletion
                            if (event != DISMISS_EVENT_ACTION) {
                                file.deleteRecursively()
                            }
                        }
                    }).show()
                }

                onClick = {
                    preferenceSettings.gpuDriver = metadata.label
                }
            })

            if (preferenceSettings.gpuDriver == metadata.label) {
                adapter.selectedPosition = index + 1 // Add 1 to account for the system driver entry
            }
        }

        adapter.setItems(items)
    }

    override fun onCreate(savedInstanceState : Bundle?) {
        super.onCreate(savedInstanceState)

        setContentView(binding.root)

        setSupportActionBar(binding.titlebar.toolbar)
        supportActionBar?.setDisplayHomeAsUpEnabled(true)
        supportActionBar?.title = getString(R.string.gpu_driver_config)

        val layoutManager = LinearLayoutManager(this)
        binding.driverList.layoutManager = layoutManager
        binding.driverList.adapter = adapter

        var layoutDone = false // Tracks if the layout is complete to avoid retrieving invalid attributes
        binding.coordinatorLayout.viewTreeObserver.addOnTouchModeChangeListener { isTouchMode ->
            val layoutUpdate = {
                val params = binding.driverList.layoutParams as CoordinatorLayout.LayoutParams
                if (!isTouchMode) {
                    binding.titlebar.appBarLayout.setExpanded(true)
                    params.height = binding.coordinatorLayout.height - binding.titlebar.toolbar.height
                } else {
                    params.height = CoordinatorLayout.LayoutParams.MATCH_PARENT
                }

                binding.driverList.layoutParams = params
                binding.driverList.requestLayout()
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

        binding.driverList.addItemDecoration(SpacingItemDecoration(resources.getDimensionPixelSize(R.dimen.grid_padding)))

        binding.addDriverButton.setOnClickListener {
            val intent = Intent(Intent.ACTION_GET_CONTENT).apply {
                addFlags(FLAG_GRANT_READ_URI_PERMISSION)
                type = "application/zip"
            }
            installCallback.launch(intent)
        }

        populateAdapter()
    }

    private fun resolveInstallResultString(result : GpuDriverInstallResult) = when (result) {
        GpuDriverInstallResult.SUCCESS -> getString(R.string.gpu_driver_install_success)
        GpuDriverInstallResult.INVALID_ARCHIVE -> getString(R.string.gpu_driver_install_invalid_archive)
        GpuDriverInstallResult.MISSING_METADATA -> getString(R.string.gpu_driver_install_missing_metadata)
        GpuDriverInstallResult.INVALID_METADATA -> getString(R.string.gpu_driver_install_invalid_metadata)
        GpuDriverInstallResult.UNSUPPORTED_ANDROID_VERSION -> getString(R.string.gpu_driver_install_unsupported_android_version)
        GpuDriverInstallResult.ALREADY_INSTALLED -> getString(R.string.gpu_driver_install_already_installed)
    }
}
