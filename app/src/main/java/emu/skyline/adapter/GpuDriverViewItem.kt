/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.adapter

import android.annotation.SuppressLint
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import emu.skyline.data.GpuDriverMetadata
import emu.skyline.databinding.GpuDriverItemBinding

object GpuDriverBindingFactory : ViewBindingFactory {
    override fun createBinding(parent : ViewGroup) = GpuDriverItemBinding.inflate(parent.inflater(), parent, false)
}

open class GpuDriverViewItem(
    val driverMetadata : GpuDriverMetadata,
    var onDelete : ((position : Int, wasChecked : Boolean) -> Unit)? = null,
    var onClick : (() -> Unit)? = null
) : SelectableGenericListItem<GpuDriverItemBinding>() {
    private var holder : GenericViewHolder<GpuDriverItemBinding>? = null
    private val adapterPosition get() = holder?.adapterPosition ?: RecyclerView.NO_POSITION

    override fun getViewBindingFactory() = GpuDriverBindingFactory

    @SuppressLint("SetTextI18n")
    override fun bind(holder : GenericViewHolder<GpuDriverItemBinding>, position : Int) {
        this.holder = holder
        val binding = holder.binding

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
            selectableAdapter?.selectAndNotify(adapterPosition)
            onClick?.invoke()
        }

        onDelete?.let { onDelete ->
            binding.deleteButton.visibility = ViewGroup.VISIBLE
            binding.deleteButton.setOnClickListener {
                val pos = adapterPosition
                if (pos == RecyclerView.NO_POSITION)
                    return@setOnClickListener

                val wasChecked = pos == selectableAdapter?.selectedPosition
                selectableAdapter?.removeItemAt(pos)

                onDelete.invoke(pos, wasChecked)
            }
        } ?: run {
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
