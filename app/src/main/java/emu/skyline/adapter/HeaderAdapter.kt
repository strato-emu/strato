/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.adapter

import android.util.SparseIntArray
import android.widget.Filter
import android.widget.Filterable
import androidx.recyclerview.widget.GridLayoutManager
import androidx.recyclerview.widget.RecyclerView
import info.debatty.java.stringsimilarity.Cosine
import info.debatty.java.stringsimilarity.JaroWinkler
import java.io.*
import java.util.*
import kotlin.collections.ArrayList


/**
 * An enumeration of the type of elements in this adapter
 */
enum class ElementType(val type: Int) {
    Header(0x0),
    Item(0x1)
}

/**
 * This is an abstract class that all adapter element classes inherit from
 */
abstract class BaseElement constructor(val elementType: ElementType) : Serializable

/**
 *  This is an abstract class that all adapter header classes inherit from
 */
class BaseHeader constructor(val title: String) : BaseElement(ElementType.Header)

/**
 * This is an abstract class that all adapter item classes inherit from
 */
abstract class BaseItem : BaseElement(ElementType.Item) {
    /**
     * This function returns a string used for searching
     */
    abstract fun key(): String?
}

/**
 * This adapter has the ability to have 2 types of elements specifically headers and items
 */
abstract class HeaderAdapter<ItemType : BaseItem?, HeaderType : BaseHeader?, ViewHolder : RecyclerView.ViewHolder?> : RecyclerView.Adapter<ViewHolder>(), Filterable, Serializable {
    /**
     * This holds all the elements in an array even if they may not be visible
     */
    var elementArray: ArrayList<BaseElement?> = ArrayList()

    /**
     * This holds the indices of all the visible items in [elementArray]
     */
    var visibleArray: ArrayList<Int> = ArrayList()

    /**
     * This holds the search term if there is any, to filter any items added during a search
     */
    private var searchTerm = ""

    /**
     * This functions adds [item] to [elementArray] and [visibleArray] based on the filter
     */
    fun addItem(item: ItemType) {
        elementArray.add(item)
        if (searchTerm.isNotEmpty())
            filter.filter(searchTerm)
        else {
            visibleArray.add(elementArray.size - 1)
            notifyDataSetChanged()
        }
    }

    /**
     * This function adds [header] to [elementArray] and [visibleArray] based on if the filter is active
     */
    fun addHeader(header: HeaderType) {
        elementArray.add(header)
        if (searchTerm.isEmpty())
            visibleArray.add(elementArray.size - 1)
        notifyDataSetChanged()
    }

    /**
     * This serializes [elementArray] into [file]
     */
    @Throws(IOException::class)
    fun save(file: File) {
        val fileObj = FileOutputStream(file)
        val out = ObjectOutputStream(fileObj)
        out.writeObject(elementArray)
        out.close()
        fileObj.close()
    }

    /**
     * This reads in [elementArray] from [file]
     */
    @Throws(IOException::class, ClassNotFoundException::class)
    open fun load(file: File) {
        val fileObj = FileInputStream(file)
        val input = ObjectInputStream(fileObj)
        @Suppress("UNCHECKED_CAST")
        elementArray = input.readObject() as ArrayList<BaseElement?>
        input.close()
        fileObj.close()
        filter.filter(searchTerm)
    }

    /**
     * This clears the view by clearing [elementArray] and [visibleArray]
     */
    fun clear() {
        elementArray.clear()
        visibleArray.clear()
        notifyDataSetChanged()
    }

    /**
     * This returns the amount of elements that should be drawn to the list
     */
    override fun getItemCount(): Int = visibleArray.size

    /**
     * This returns a particular element at [position]
     */
    fun getItem(position: Int): BaseElement? {
        return elementArray[visibleArray[position]]
    }

    /**
     * This returns the type of an element at the specified position
     *
     * @param position The position of the element
     */
    override fun getItemViewType(position: Int): Int {
        return elementArray[visibleArray[position]]!!.elementType.type
    }

    /**
     * This returns an instance of the filter object which is used to search for items in the view
     */
    override fun getFilter(): Filter {
        return object : Filter() {
            /**
             * We use Jaro-Winkler distance for string similarity (https://en.wikipedia.org/wiki/Jaro%E2%80%93Winkler_distance)
             */
            private val jw = JaroWinkler()

            /**
             * We use Cosine similarity for string similarity (https://en.wikipedia.org/wiki/Cosine_similarity)
             */
            private val cos = Cosine()

            /**
             * This class is used to store the results of the item sorting
             *
             * @param score The score of this result
             * @param index The index of this item
             */
            inner class ScoredItem(val score: Double, val index: Int) {}

            /**
             * This sorts the items in [keyArray] in relation to how similar they are to [term]
             */
            fun extractSorted(term: String, keyArray: ArrayList<String>): Array<ScoredItem> {
                val scoredItems: MutableList<ScoredItem> = ArrayList()

                keyArray.forEachIndexed { index, item ->
                    val similarity = (jw.similarity(term, item) + cos.similarity(term, item)) / 2

                    if (similarity != 0.0)
                        scoredItems.add(ScoredItem(similarity, index))
                }

                scoredItems.sortWith(compareByDescending { it.score })

                return scoredItems.toTypedArray()
            }

            /**
             * This performs filtering on the items in [elementArray] based on similarity to [term]
             */
            override fun performFiltering(term: CharSequence): FilterResults {
                val results = FilterResults()
                searchTerm = (term as String).toLowerCase(Locale.getDefault())

                if (term.isEmpty()) {
                    results.values = elementArray.indices.toMutableList()
                    results.count = elementArray.size
                } else {
                    val filterData = ArrayList<Int>()

                    val keyArray = ArrayList<String>()
                    val keyIndex = SparseIntArray()

                    for (index in elementArray.indices) {
                        val item = elementArray[index]!!

                        if (item is BaseItem) {
                            keyIndex.append(keyArray.size, index)
                            keyArray.add(item.key()!!.toLowerCase(Locale.getDefault()))
                        }
                    }

                    val topResults = extractSorted(searchTerm, keyArray)
                    val avgScore = topResults.sumByDouble { it.score } / topResults.size

                    for (result in topResults)
                        if (result.score > avgScore)
                            filterData.add(keyIndex[result.index])

                    results.values = filterData
                    results.count = filterData.size
                }

                return results
            }

            /**
             * This publishes the results that were calculated in [performFiltering] to the view
             */
            override fun publishResults(charSequence: CharSequence, results: FilterResults) {
                if (results.values is ArrayList<*>) {
                    @Suppress("UNCHECKED_CAST")
                    visibleArray = results.values as ArrayList<Int>

                    notifyDataSetChanged()
                }
            }
        }
    }
}

/**
 * This class is used to lookup the span based on the type of the element
 *
 * @param adapter The adapter which is used to deduce the type of the item based on the position
 * @param headerSpan The span size to return for headers
 */
class GridLayoutSpan<ItemType : BaseItem?, HeaderType : BaseHeader?, ViewHolder : RecyclerView.ViewHolder?>(val adapter: HeaderAdapter<ItemType, HeaderType, ViewHolder>, var headerSpan: Int) : GridLayoutManager.SpanSizeLookup() {
    /**
     * This returns the size of the span based on the type of the element at [position]
     */
    override fun getSpanSize(position: Int): Int {
        val item = adapter.getItem(position)!!
        return if (item.elementType == ElementType.Item)
            1
        else
            headerSpan
    }
}
