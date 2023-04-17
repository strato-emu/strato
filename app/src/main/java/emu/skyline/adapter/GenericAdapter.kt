/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.adapter

import android.view.ViewGroup
import android.widget.Filter
import android.widget.Filterable
import androidx.recyclerview.widget.AsyncListDiffer
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.RecyclerView
import androidx.viewbinding.ViewBinding
import info.debatty.java.stringsimilarity.Cosine
import info.debatty.java.stringsimilarity.JaroWinkler
import java.util.*

typealias OnFilterPublishedListener = () -> Unit

/**
 * Can handle any view types with [GenericListItem] implemented, [GenericListItem] are differentiated by the return value of [GenericListItem.getViewBindingFactory]
 */
open class GenericAdapter : RecyclerView.Adapter<GenericViewHolder<ViewBinding>>(), Filterable {
    companion object {
        private val DIFFER = object : DiffUtil.ItemCallback<GenericListItem<ViewBinding>>() {
            override fun areItemsTheSame(oldItem : GenericListItem<ViewBinding>, newItem : GenericListItem<ViewBinding>) = oldItem.areItemsTheSame(newItem)

            override fun areContentsTheSame(oldItem : GenericListItem<ViewBinding>, newItem : GenericListItem<ViewBinding>) = oldItem.areContentsTheSame(newItem)
        }
    }

    private val asyncListDiffer = AsyncListDiffer(this, DIFFER)
    val allItems = mutableListOf<GenericListItem<out ViewBinding>>()
    val currentItems : List<GenericListItem<in ViewBinding>> get() = asyncListDiffer.currentList

    var currentSearchTerm = ""

    private val viewTypesMapping = mutableMapOf<ViewBindingFactory, Int>()

    private var onFilterPublishedListener : OnFilterPublishedListener? = null

    override fun onCreateViewHolder(parent : ViewGroup, viewType : Int) = GenericViewHolder(viewTypesMapping.filterValues { it == viewType }.keys.single().createBinding(parent))

    override fun onBindViewHolder(holder : GenericViewHolder<ViewBinding>, position : Int) {
        currentItems[position].apply {
            adapter = this@GenericAdapter
            bind(holder, position)
        }
    }

    override fun getItemCount() = currentItems.size

    fun getFactoryViewType(factory : ViewBindingFactory) = viewTypesMapping.get(factory)

    override fun getItemViewType(position : Int) = viewTypesMapping.getOrPut(currentItems[position].getViewBindingFactory()) { viewTypesMapping.size }

    fun setItems(items : List<GenericListItem<*>>) {
        allItems.clear()
        allItems.addAll(items)
        filter.filter(currentSearchTerm)
    }

    open fun removeItemAt(position : Int) {
        allItems.removeAt(position)
        filter.filter(currentSearchTerm)
    }

    open fun addItemAt(position : Int, item : GenericListItem<out ViewBinding>) {
        allItems.add(position, item)
        filter.filter(currentSearchTerm)
    }

    fun setOnFilterPublishedListener(listener : OnFilterPublishedListener) {
        onFilterPublishedListener = listener
    }

    /**
     * This returns an instance of the filter object which is used to search for items in the view
     */
    override fun getFilter() = object : Filter() {
        /**
         * We use Jaro-Winkler distance for string similarity (https://en.wikipedia.org/wiki/Jaro%E2%80%93Winkler_distance)
         */
        private val jw = JaroWinkler()

        /**
         * We use Cosine similarity for string similarity (https://en.wikipedia.org/wiki/Cosine_similarity)
         */
        private val cos = Cosine()

        inner class ScoredItem(val score : Double, val item : GenericListItem<*>)

        /**
         * This sorts the items in [allItems] in relation to how similar they are to [currentSearchTerm]
         */
        fun extractSorted() = allItems.mapNotNull { item ->
            item.key().lowercase(Locale.getDefault()).let {
                val similarity = (jw.similarity(currentSearchTerm, it) + cos.similarity(currentSearchTerm, it)) / 2
                if (similarity != 0.0) ScoredItem(similarity, item) else null
            }
        }.sortedByDescending { it.score }

        /**
         * This performs filtering on the items in [allItems] based on similarity to [term]
         */
        override fun performFiltering(term : CharSequence) = (term as String).lowercase(Locale.getDefault()).let { lowerCaseTerm ->
            currentSearchTerm = lowerCaseTerm

            with(if (term.isEmpty()) {
                allItems.toMutableList()
            } else {
                val filterData = mutableListOf<GenericListItem<*>>()

                val topResults = extractSorted()
                val avgScore = topResults.sumOf { it.score } / topResults.size

                for (result in topResults)
                    if (result.score >= avgScore) filterData.add(result.item)

                filterData
            }) {
                FilterResults().apply {
                    values = this@with
                    count = size
                }
            }
        }

        /**
         * This publishes the results that were calculated in [performFiltering] to the view
         */
        override fun publishResults(charSequence : CharSequence, results : FilterResults) {
            @Suppress("UNCHECKED_CAST")
            asyncListDiffer.submitList(results.values as List<GenericListItem<ViewBinding>>)
            onFilterPublishedListener?.invoke()
        }
    }
}

/**
 * A [GenericAdapter] that supports marking one item as selected. List items that need to access the selected position should inherit from [SelectableGenericListItem]
 */
class SelectableGenericAdapter(private val defaultPosition : Int) : GenericAdapter() {
    var selectedPosition : Int = defaultPosition

    /**
     * Sets the selected position to [position] and notifies previous and new selected positions
     */
    fun selectAndNotify(position : Int) {
        val previousSelectedPosition = selectedPosition
        selectedPosition = position
        notifyItemChanged(previousSelectedPosition)
        notifyItemChanged(position)
    }

    /**
     * Sets the selected position to [position] and only notifies the previous selected position
     */
    fun selectAndNotifyPrevious(position : Int) {
        val previousSelectedPosition = selectedPosition
        selectedPosition = position
        notifyItemChanged(previousSelectedPosition)
    }

    /**
     * Removes the item at [position] from the list and updates the selected position accordingly
     */
    override fun removeItemAt(position : Int) {
        super.removeItemAt(position)
        if (position < selectedPosition)
            selectedPosition--
        else if (position == selectedPosition)
            selectAndNotify(defaultPosition)
    }

    /**
     * Inserts the given item to the list at [position] and updates the selected position accordingly
     */
    override fun addItemAt(position : Int, item : GenericListItem<out ViewBinding>) {
        super.addItemAt(position, item)
        if (position <= selectedPosition)
            selectedPosition++
    }
}
