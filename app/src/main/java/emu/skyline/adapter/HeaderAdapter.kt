/*
 * SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.adapter

import android.util.SparseIntArray
import android.view.View
import android.view.ViewGroup
import android.widget.BaseAdapter
import android.widget.Filter
import android.widget.Filterable
import info.debatty.java.stringsimilarity.Cosine
import info.debatty.java.stringsimilarity.JaroWinkler
import java.io.*
import java.util.*
import kotlin.collections.ArrayList


enum class ElementType(val type: Int) {
    Header(0x0),
    Item(0x1)
}

/**
 * @brief This is the interface class that all element classes inherit from
 */
abstract class BaseElement constructor(val elementType: ElementType) : Serializable

/**
 * @brief This is the interface class that all header classes inherit from
 */
class BaseHeader constructor(val title: String) : BaseElement(ElementType.Header)

/**
 * @brief This is the interface class that all item classes inherit from
 */
abstract class BaseItem : BaseElement(ElementType.Item) {
    abstract fun key(): String?
}

internal abstract class HeaderAdapter<ItemType : BaseItem?, HeaderType : BaseHeader?> : BaseAdapter(), Filterable, Serializable {
    var elementArray: ArrayList<BaseElement?> = ArrayList()
    var visibleArray: ArrayList<Int> = ArrayList()
    private var searchTerm = ""

    fun addItem(element: ItemType) {
        elementArray.add(element)
        if (searchTerm.isNotEmpty())
            filter.filter(searchTerm)
        else {
            visibleArray.add(elementArray.size - 1)
            notifyDataSetChanged()
        }
    }

    fun addHeader(element: HeaderType) {
        elementArray.add(element)
        if (searchTerm.isNotEmpty())
            filter.filter(searchTerm)
        else {
            visibleArray.add(elementArray.size - 1)
            notifyDataSetChanged()
        }
    }

    internal inner class State(val elementArray: ArrayList<BaseElement?>) : Serializable

    @Throws(IOException::class)
    fun save(file: File) {
        val fileObj = FileOutputStream(file)
        val out = ObjectOutputStream(fileObj)
        out.writeObject(elementArray)
        out.close()
        fileObj.close()
    }

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

    fun clear() {
        elementArray.clear()
        visibleArray.clear()
        notifyDataSetChanged()
    }

    override fun getCount(): Int {
        return visibleArray.size
    }

    override fun getItem(index: Int): BaseElement? {
        return elementArray[visibleArray[index]]
    }

    override fun getItemId(position: Int): Long {
        return position.toLong()
    }

    override fun getItemViewType(position: Int): Int {
        return elementArray[visibleArray[position]]!!.elementType.type
    }

    override fun getViewTypeCount(): Int {
        return ElementType.values().size
    }

    abstract override fun getView(position: Int, convertView: View?, parent: ViewGroup): View

    override fun getFilter(): Filter {
        return object : Filter() {
            inner class ScoredItem(val score: Double, val index: Int, val item:String) {}

            fun extractSorted(term: String, keyArray: ArrayList<String>): Array<ScoredItem> {
                val scoredItems : MutableList<ScoredItem> = ArrayList()

                val jw = JaroWinkler()
                val cos = Cosine()

                keyArray.forEachIndexed { index, item -> scoredItems.add(ScoredItem((jw.similarity(term, item) + cos.similarity(term, item)) / 2, index, item)) }
                scoredItems.sortWith(compareByDescending { it.score })

                return scoredItems.toTypedArray()
            }

            override fun performFiltering(charSequence: CharSequence): FilterResults {
                val results = FilterResults()
                searchTerm = (charSequence as String).toLowerCase(Locale.getDefault())
                if (charSequence.isEmpty()) {
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
