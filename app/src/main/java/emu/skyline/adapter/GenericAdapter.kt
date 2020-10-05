/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.adapter

import android.view.ViewGroup
import android.widget.Filter
import android.widget.Filterable
import androidx.recyclerview.widget.RecyclerView
import info.debatty.java.stringsimilarity.Cosine
import info.debatty.java.stringsimilarity.JaroWinkler
import java.util.*

/**
 * Can handle any view types with [GenericViewHolderBinder] implemented, [GenericViewHolderBinder] are differentiated by the return value of [GenericViewHolderBinder.getLayoutFactory]
 */
class GenericAdapter : RecyclerView.Adapter<GenericViewHolder>(), Filterable {
    var currentSearchTerm = ""

    val currentItems get() = if (currentSearchTerm.isEmpty()) allItems else filteredItems
    val allItems = mutableListOf<GenericViewHolderBinder>()
    private var filteredItems = listOf<GenericViewHolderBinder>()

    private val viewTypesMapping = mutableMapOf<GenericLayoutFactory, Int>()

    override fun onCreateViewHolder(parent : ViewGroup, viewType : Int) = GenericViewHolder(viewTypesMapping.filterValues { it == viewType }.keys.single().createLayout(parent))

    override fun onBindViewHolder(holder : GenericViewHolder, position : Int) {
        currentItems[position].apply {
            adapter = this@GenericAdapter
            bind(holder, position)
        }
    }

    override fun getItemCount() = currentItems.size

    override fun getItemViewType(position : Int) = viewTypesMapping.getOrPut(currentItems[position].getLayoutFactory(), { viewTypesMapping.size })

    fun addItem(item : GenericViewHolderBinder) {
        allItems.add(item)
        notifyItemInserted(currentItems.size)
    }

    fun removeAllItems() {
        val size = currentItems.size
        allItems.clear()
        notifyItemRangeRemoved(0, size)
    }

    fun notifyAllItemsChanged() {
        notifyItemRangeChanged(0, currentItems.size)
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

        inner class ScoredItem(val score : Double, val item : GenericViewHolderBinder)

        /**
         * This sorts the items in [allItems] in relation to how similar they are to [currentSearchTerm]
         */
        fun extractSorted() = allItems.mapNotNull { item ->
            item.key().toLowerCase(Locale.getDefault()).let {
                val similarity = (jw.similarity(currentSearchTerm, it)) + cos.similarity(currentSearchTerm, it) / 2
                if (similarity != 0.0) ScoredItem(similarity, item) else null
            }
        }.apply {
            sortedByDescending { it.score }
        }

        /**
         * This performs filtering on the items in [allItems] based on similarity to [term]
         */
        override fun performFiltering(term : CharSequence) : FilterResults {
            val results = FilterResults()
            currentSearchTerm = (term as String).toLowerCase(Locale.getDefault())

            if (term.isEmpty()) {
                results.values = allItems.toMutableList()
                results.count = allItems.size
            } else {
                val filterData = mutableListOf<GenericViewHolderBinder>()

                val topResults = extractSorted()
                val avgScore = topResults.sumByDouble { it.score } / topResults.size

                for (result in topResults)
                    if (result.score > avgScore) filterData.add(result.item)

                results.values = filterData
                results.count = filterData.size
            }
            return results
        }

        /**
         * This publishes the results that were calculated in [performFiltering] to the view
         */
        override fun publishResults(charSequence : CharSequence, results : FilterResults) {
            @Suppress("UNCHECKED_CAST")
            filteredItems = results.values as List<GenericViewHolderBinder>

            notifyDataSetChanged()
        }
    }
}