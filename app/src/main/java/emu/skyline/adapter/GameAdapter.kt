package emu.skyline.adapter

import android.app.Dialog
import android.content.Context
import android.graphics.Bitmap
import android.graphics.Color
import android.graphics.drawable.ColorDrawable
import android.net.Uri
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.Window
import android.widget.ImageView
import android.widget.RelativeLayout
import android.widget.TextView
import emu.skyline.R
import emu.skyline.loader.TitleEntry

internal class GameItem(val meta: TitleEntry) : BaseItem() {
    val icon: Bitmap?
        get() = meta.icon

    val title: String
        get() = meta.name + " (" + type + ")"

    val subTitle: String?
        get() = meta.author

    val uri: Uri
        get() = meta.uri

    private val type: String
        get() = meta.romType.name

    override fun key(): String? {
        return if (meta.valid) meta.name + " " + meta.author else meta.name
    }
}

internal class GameAdapter(val context: Context?) : HeaderAdapter<GameItem, BaseHeader>(), View.OnClickListener {
    fun addHeader(string: String) {
        super.addHeader(BaseHeader(string))
    }

    override fun onClick(view: View) {
        val position = view.tag as Int
        if (getItem(position) is GameItem) {
            val item = getItem(position) as GameItem
            if (view.id == R.id.icon) {
                val builder = Dialog(context!!)
                builder.requestWindowFeature(Window.FEATURE_NO_TITLE)
                builder.window!!.setBackgroundDrawable(ColorDrawable(Color.TRANSPARENT))
                val imageView = ImageView(context)
                imageView.setImageBitmap(item.icon)
                builder.addContentView(imageView, RelativeLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT))
                builder.show()
            }
        }
    }

    override fun getView(position: Int, convertView: View?, parent: ViewGroup): View {
        var view = convertView
        val viewHolder: ViewHolder
        val item = elementArray[visibleArray[position]]
        if (view == null) {
            viewHolder = ViewHolder()
            if (item is GameItem) {
                val inflater = LayoutInflater.from(context)
                view = inflater.inflate(R.layout.game_item, parent, false)
                viewHolder.icon = view.findViewById(R.id.icon)
                viewHolder.txtTitle = view.findViewById(R.id.text_title)
                viewHolder.txtSub = view.findViewById(R.id.text_subtitle)
                view.tag = viewHolder
            } else if (item is BaseHeader) {
                val inflater = LayoutInflater.from(context)
                view = inflater.inflate(R.layout.section_item, parent, false)
                viewHolder.txtTitle = view.findViewById(R.id.text_title)
                view.tag = viewHolder
            }
        } else {
            viewHolder = view.tag as ViewHolder
        }
        if (item is GameItem) {
            val data = getItem(position) as GameItem
            viewHolder.txtTitle!!.text = data.title
            viewHolder.txtSub!!.text = data.subTitle
            viewHolder.icon!!.setImageBitmap(data.icon)
            viewHolder.icon!!.setOnClickListener(this)
            viewHolder.icon!!.tag = position
        } else {
            viewHolder.txtTitle!!.text = (getItem(position) as BaseHeader).title
        }
        return view!!
    }

    private class ViewHolder {
        var icon: ImageView? = null
        var txtTitle: TextView? = null
        var txtSub: TextView? = null
    }
}
