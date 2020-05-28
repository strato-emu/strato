/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.adapter

import android.content.Context
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView
import emu.skyline.R
import emu.skyline.data.BaseItem
import emu.skyline.input.*

/**
 * This is a class that holds everything relevant to a single item in the controller configuration list
 *
 * @param content The main line of text describing what the item is
 * @param subContent The secondary line of text to show data more specific data about the item
 */
abstract class ControllerItem(var content : String, var subContent : String) : BaseItem() {
    /**
     * The underlying adapter this item is contained within
     */
    var adapter : ControllerAdapter? = null

    /**
     * The position of this item in the adapter
     */
    var position : Int? = null

    /**
     * This function updates the visible contents of the item
     */
    fun update(content : String?, subContent : String?) {
        if (content != null)
            this.content = content

        if (subContent != null)
            this.subContent = subContent

        position?.let { adapter?.notifyItemChanged(it) }
    }

    /**
     * This is used as a generic function to update the contents of the item
     */
    abstract fun update()
}

/**
 * This item is used to display the [type] of the currently active controller
 */
class ControllerTypeItem(val context : Context, val type : ControllerType) : ControllerItem(context.getString(R.string.controller_type), context.getString(type.stringRes)) {
    /**
     * This function just updates [subContent] based on [type]
     */
    override fun update() = update(null, context.getString(type.stringRes))
}

/**
 * This item is used to display general settings items regarding controller
 *
 * @param type The type of controller setting this item is displaying
 */
class ControllerGeneralItem(val context : ControllerActivity, val type : GeneralType) : ControllerItem(context.getString(type.stringRes), getSummary(context, type)) {
    companion object {
        /**
         * This returns the summary for [type] by using data encapsulated within [Controller]
         */
        fun getSummary(context : ControllerActivity, type : GeneralType) : String {
            val controller = context.manager.controllers[context.id]!!

            return when (type) {
                GeneralType.PartnerJoyCon -> {
                    val partner = (controller as JoyConLeftController).partnerId

                    if (partner != null)
                        "${context.getString(R.string.controller)} #${partner + 1}"
                    else
                        context.getString(R.string.none)
                }
                GeneralType.RumbleDevice -> controller.rumbleDevice?.second ?: context.getString(R.string.none)
            }
        }
    }

    /**
     * This function just updates [subContent] based on [getSummary]
     */
    override fun update() = update(null, getSummary(context, type))
}

/**
 * This item is used to display a particular [button] mapping for the controller
 */
class ControllerButtonItem(val context : ControllerActivity, val button : ButtonId) : ControllerItem(button.long?.let { context.getString(it) } ?: button.toString(), getSummary(context, button)) {
    companion object {
        /**
         * This returns the summary for [button] by doing a reverse-lookup in [InputManager.eventMap]
         */
        fun getSummary(context : ControllerActivity, button : ButtonId) : String {
            val guestEvent = ButtonGuestEvent(context.id, button)
            return context.manager.eventMap.filter { it.value is ButtonGuestEvent && it.value == guestEvent }.keys.firstOrNull()?.toString() ?: context.getString(R.string.none)
        }
    }

    /**
     * This function just updates [subContent] based on [getSummary]
     */
    override fun update() = update(null, getSummary(context, button))
}

/**
 * This item is used to display all information regarding a [stick] and it's mappings for the controller
 */
class ControllerStickItem(val context : ControllerActivity, val stick : StickId) : ControllerItem(stick.toString(), getSummary(context, stick)) {
    companion object {
        /**
         * This returns the summary for [stick] by doing reverse-lookups in [InputManager.eventMap]
         */
        fun getSummary(context : ControllerActivity, stick : StickId) : String {
            val buttonGuestEvent = ButtonGuestEvent(context.id, stick.button)
            val button = context.manager.eventMap.filter { it.value is ButtonGuestEvent && it.value == buttonGuestEvent }.keys.firstOrNull()?.toString() ?: context.getString(R.string.none)

            var axisGuestEvent = AxisGuestEvent(context.id, stick.yAxis, true)
            val yAxisPlus = context.manager.eventMap.filter { it.value is AxisGuestEvent && it.value == axisGuestEvent }.keys.firstOrNull()?.toString() ?: context.getString(R.string.none)

            axisGuestEvent = AxisGuestEvent(context.id, stick.yAxis, false)
            val yAxisMinus = context.manager.eventMap.filter { it.value is AxisGuestEvent && it.value == axisGuestEvent }.keys.firstOrNull()?.toString() ?: context.getString(R.string.none)

            axisGuestEvent = AxisGuestEvent(context.id, stick.xAxis, true)
            val xAxisPlus = context.manager.eventMap.filter { it.value is AxisGuestEvent && it.value == axisGuestEvent }.keys.firstOrNull()?.toString() ?: context.getString(R.string.none)

            axisGuestEvent = AxisGuestEvent(context.id, stick.xAxis, false)
            val xAxisMinus = context.manager.eventMap.filter { it.value is AxisGuestEvent && it.value == axisGuestEvent }.keys.firstOrNull()?.toString() ?: context.getString(R.string.none)

            return "${context.getString(R.string.button)}: $button\n${context.getString(R.string.up)}: $yAxisPlus\n${context.getString(R.string.down)}: $yAxisMinus\n${context.getString(R.string.left)}: $xAxisMinus\n${context.getString(R.string.right)}: $xAxisPlus"
        }
    }

    /**
     * This function just updates [subContent] based on [getSummary]
     */
    override fun update() = update(null, getSummary(context, stick))
}

/**
 * This adapter is used to create a list which handles having a simple view
 */
class ControllerAdapter(val context : Context) : HeaderAdapter<ControllerItem?, BaseHeader, RecyclerView.ViewHolder>() {
    /**
     * This adds a header to the view with the contents of [string]
     */
    fun addHeader(string : String) {
        super.addHeader(BaseHeader(string))
    }

    /**
     * This functions sets [ControllerItem.adapter] and delegates the call to [HeaderAdapter.addItem]
     */
    fun addItem(item : ControllerItem) {
        item.adapter = this
        super.addItem(item)
    }

    /**
     * The ViewHolder used by items is used to hold the views associated with an item
     *
     * @param parent The parent view that contains all the others
     * @param title The TextView associated with the title
     * @param subtitle The TextView associated with the subtitle
     * @param item The View containing the two other views
     */
    class ItemViewHolder(val parent : View, var title : TextView, var subtitle : TextView, var item : View) : RecyclerView.ViewHolder(parent)

    /**
     * The ViewHolder used by headers is used to hold the views associated with an headers
     *
     * @param parent The parent view that contains all the others
     * @param header The TextView associated with the header
     */
    private class HeaderViewHolder(val parent : View, var header : TextView? = null) : RecyclerView.ViewHolder(parent)

    /**
     * This function creates the view-holder of type [viewType] with the layout parent as [parent]
     */
    override fun onCreateViewHolder(parent : ViewGroup, viewType : Int) : RecyclerView.ViewHolder {
        val inflater = LayoutInflater.from(context)
        var holder : RecyclerView.ViewHolder? = null

        if (viewType == ElementType.Item.ordinal) {
            val view = inflater.inflate(R.layout.controller_item, parent, false)
            holder = ItemViewHolder(view, view.findViewById(R.id.text_title), view.findViewById(R.id.text_subtitle), view.findViewById(R.id.controller_item))

            if (context is View.OnClickListener)
                holder.item.setOnClickListener(context as View.OnClickListener)
        } else if (viewType == ElementType.Header.ordinal) {
            val view = inflater.inflate(R.layout.section_item, parent, false)
            holder = HeaderViewHolder(view)

            holder.header = view.findViewById(R.id.text_title)
        }

        return holder!!
    }

    /**
     * This function binds the item at [position] to the supplied [holder]
     */
    override fun onBindViewHolder(holder : RecyclerView.ViewHolder, position : Int) {
        val item = getItem(position)

        if (item is ControllerItem && holder is ItemViewHolder) {
            item.position = position

            holder.title.text = item.content
            holder.subtitle.text = item.subContent

            holder.parent.tag = item
        } else if (item is BaseHeader && holder is HeaderViewHolder) {
            holder.header?.text = item.title
        }
    }
}
