package emu.skyline.views

import android.content.Context
import android.text.Editable
import android.text.TextWatcher
import android.util.AttributeSet
import android.util.TypedValue
import android.view.LayoutInflater
import android.view.inputmethod.InputMethodManager
import androidx.core.view.MarginLayoutParamsCompat
import androidx.core.view.isInvisible
import androidx.core.view.isVisible
import com.google.android.material.card.MaterialCardView
import emu.skyline.R
import kotlinx.android.synthetic.main.view_search_bar.view.*
import kotlin.math.roundToInt

class SearchBarView @JvmOverloads constructor(context : Context, attrs : AttributeSet? = null, defStyleAttr : Int = com.google.android.material.R.attr.materialCardViewStyle) : MaterialCardView(context, attrs, defStyleAttr) {
    init {
        LayoutInflater.from(context).inflate(R.layout.view_search_bar, this)
        useCompatPadding = true
    }

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()

        val margin = TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, 16f, context.resources.displayMetrics).roundToInt()
        MarginLayoutParamsCompat.setMarginStart(layoutParams as MarginLayoutParams?, margin)
        MarginLayoutParamsCompat.setMarginEnd(layoutParams as MarginLayoutParams?, margin)

        radius = margin / 2f
        cardElevation = radius / 2f
    }

    fun setRefreshIconListener(listener : OnClickListener) = refresh_icon.setOnClickListener(listener)
    fun setLogIconListener(listener : OnClickListener) = log_icon.setOnClickListener(listener)
    fun setSettingsIconListener(listener : OnClickListener) = settings_icon.setOnClickListener(listener)

    var refreshIconVisible = false
        set(visible) {
            field = visible
            refresh_icon.apply {
                if (visible != isVisible) {
                    refresh_icon.alpha = if (visible) 0f else 1f
                    animate().alpha(if (visible) 1f else 0f).withStartAction { isVisible = true }.withEndAction { isInvisible = !visible }.apply { duration = 500 }.start()
                }
            }
        }

    var text : CharSequence
        get() = search_field.text
        set(value) = search_field.setText(value)

    fun startTitleAnimation() {
        motion_layout.progress = 0f
        motion_layout.transitionToEnd()
        search_field.apply {
            setOnFocusChangeListener { v, hasFocus ->
                if (hasFocus) {
                    this@SearchBarView.motion_layout.progress = 1f
                    context.getSystemService(InputMethodManager::class.java).showSoftInput(v, InputMethodManager.SHOW_IMPLICIT)
                    onFocusChangeListener = null
                }
            }
        }
    }

    fun animateRefreshIcon() {
        refresh_icon.animate().rotationBy(-180f)
    }

    inline fun addTextChangedListener(
            crossinline beforeTextChanged : (
                    text : CharSequence?,
                    start : Int,
                    count : Int,
                    after : Int
            ) -> Unit = { _, _, _, _ -> },
            crossinline onTextChanged : (
                    text : CharSequence?,
                    start : Int,
                    before : Int,
                    count : Int
            ) -> Unit = { _, _, _, _ -> },
            crossinline afterTextChanged : (text : Editable?) -> Unit = {}
    ) : TextWatcher {
        val textWatcher = object : TextWatcher {
            override fun afterTextChanged(s : Editable?) {
                afterTextChanged.invoke(s)
            }

            override fun beforeTextChanged(text : CharSequence?, start : Int, count : Int, after : Int) {
                beforeTextChanged.invoke(text, start, count, after)
            }

            override fun onTextChanged(text : CharSequence?, start : Int, before : Int, count : Int) {
                onTextChanged.invoke(text, start, before, count)
            }
        }
        search_field.addTextChangedListener(textWatcher)

        return textWatcher
    }
}
