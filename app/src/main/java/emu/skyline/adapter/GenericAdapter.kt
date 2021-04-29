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
class GenericAdapter : RecyclerView.Adapter<GenericViewHolder<ViewBinding>>(), Filterable {
    companion object {
        private val DIFFER = object : DiffUtil.ItemCallback<GenericListItem<ViewBinding>>() {
            override fun areItemsTheSame(oldItem : GenericListItem<ViewBinding>, newItem : GenericListItem<ViewBinding>) = oldItem.areItemsTheSame(newItem)

            override fun areContentsTheSame(oldItem : GenericListItem<ViewBinding>, newItem : GenericListItem<ViewBinding>) = oldItem.areContentsTheSame(newItem)
        }
    }

    private val asyncListDiffer = AsyncListDiffer(this, DIFFER)
    private val headerItems = mutableListOf<GenericListItem<out ViewBinding>>()
    val allItems = mutableListOf<GenericListItem<out ViewBinding>>()
    val currentItems : List<GenericListItem<in ViewBinding>> get() = asyncListDiffer.currentList

    var currentSearchTerm = ""

    private val viewTypesMapping = mutableMapOf<ViewBindingFactory, Int>()

    private var onFilterPublishedListener : OnFilterPublishedListener? = null

    override fun onCreateViewHolder(parent : ViewGroup, viewType : Int) = GenericViewHolder(viewTypesMapping.filterValues { it == viewType }.keys.single().createBinding(parent))

    override fun onBindViewHolder(holder : GenericViewHolder<ViewBinding>, position : Int) {
        currentItems[position].apply {
            adapter = this@GenericAdapter
            bind(holder.binding, position)
        }
    }

    override fun getItemCount() = currentItems.size

    override fun getItemViewType(position : Int) = viewTypesMapping.getOrPut(currentItems[position].getViewBindingFactory()) { viewTypesMapping.size }

    fun setHeaderItems(items : List<GenericListItem<*>>) {
        headerItems.clear()
        headerItems.addAll(items)
        filter.filter(currentSearchTerm)
    }

    fun setItems(items : List<GenericListItem<*>>) {
        allItems.clear()
        allItems.addAll(items)
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
            item.key().toLowerCase(Locale.getDefault()).let {
                val similarity = (jw.similarity(currentSearchTerm, it) + cos.similarity(currentSearchTerm, it)) / 2
                if (similarity != 0.0) ScoredItem(similarity, item) else null
            }
        }.sortedByDescending { it.score }

        /**
         * This performs filtering on the items in [allItems] based on similarity to [term]
         */
        override fun performFiltering(term : CharSequence) = (term as String).toLowerCase(Locale.getDefault()).let { lowerCaseTerm ->
            currentSearchTerm = lowerCaseTerm

            with(if (term.isEmpty()) {
                allItems.toMutableList()
            } else {
                val filterData = mutableListOf<GenericListItem<*>>()

                val topResults = extractSorted()
                val avgScore = topResults.sumByDouble { it.score } / topResults.size

                for (result in topResults)
                    if (result.score >= avgScore) filterData.add(result.item)

                filterData
            }) {
                FilterResults().apply {
                    values = headerItems + this@with
                    count = headerItems.size + size
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