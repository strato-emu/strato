/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.adapter

import android.view.ViewGroup
import emu.skyline.data.GpuDriverMetadata
import emu.skyline.databinding.GpuDriverItemBinding

object GpuDriverBindingFactory : ViewBindingFactory {
    override fun createBinding(parent : ViewGroup) = GpuDriverItemBinding.inflate(parent.inflater(), parent, false)
}

open class GpuDriverViewItem(
    val driverMetadata : GpuDriverMetadata,
    private val onDelete : ((wasChecked : Boolean) -> Unit)? = null,
    private val onClick : (() -> Unit)? = null
) : SelectableGenericListItem<GpuDriverItemBinding>() {
    private var position = -1

    override fun getViewBindingFactory() = GpuDriverBindingFactory

    override fun bind(binding : GpuDriverItemBinding, position : Int) {
        this.position = position
        binding.name.text = driverMetadata.name

        if (driverMetadata.packageVersion.isNotBlank() || driverMetadata.packageVersion.isNotBlank()) {
            binding.authorPackageVersion.text = "v${driverMetadata.packageVersion} by ${driverMetadata.author}"
            binding.authorPackageVersion.visibility = ViewGroup.VISIBLE
        } else {
            binding.authorPackageVersion.visibility = ViewGroup.GONE
        }

        binding.vendorDriverVersion.text = "Driver: v${driverMetadata.driverVersion} • ${driverMetadata.vendor}"
        binding.description.text = driverMetadata.description
        binding.radioButton.isChecked = position == selectableAdapter?.selectedPosition

        binding.root.setOnClickListener {
            selectableAdapter?.selectAndNotify(position)
            onClick?.invoke()
        }

        if (onDelete != null) {
            binding.deleteButton.visibility = ViewGroup.VISIBLE
            binding.deleteButton.setOnClickListener {
                val wasChecked = position == selectableAdapter?.selectedPosition
                selectableAdapter?.removeItemAt(position)

                onDelete.invoke(wasChecked)
            }
        } else {
            binding.deleteButton.visibility = ViewGroup.GONE
        }
    }

    /**
     * The label of the driver is used as key as it's effectively unique
     */
    override fun key() = driverMetadata.label

    override fun areItemsTheSame(other : GenericListItem<GpuDriverItemBinding>) : Boolean = key() == other.key()

    override fun areContentsTheSame(other : GenericListItem<GpuDriverItemBinding>) : Boolean = other is GpuDriverViewItem && driverMetadata == other.driverMetadata
}
