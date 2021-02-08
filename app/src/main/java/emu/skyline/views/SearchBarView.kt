package emu.skyline.views

import android.content.Context
import android.text.Editable
import android.text.TextWatcher
import android.util.AttributeSet
import android.view.LayoutInflater
import android.view.inputmethod.InputMethodManager
import com.google.android.material.card.MaterialCardView
import emu.skyline.databinding.ViewSearchBarBinding

class SearchBarView @JvmOverloads constructor(context : Context, attrs : AttributeSet? = null, defStyleAttr : Int = com.google.android.material.R.attr.materialCardViewStyle) : MaterialCardView(context, attrs, defStyleAttr) {
    private val binding = ViewSearchBarBinding.inflate(LayoutInflater.from(context), this)

    init {
        useCompatPadding = true
    }

    var text : CharSequence
        get() = binding.searchField.text
        set(value) = binding.searchField.setText(value)

    fun startTitleAnimation() {
        binding.motionLayout.progress = 0f
        binding.motionLayout.transitionToEnd()
        binding.searchField.apply {
            setOnFocusChangeListener { v, hasFocus ->
                if (hasFocus) {
                    binding.motionLayout.progress = 1f
                    context.getSystemService(InputMethodManager::class.java).showSoftInput(v, InputMethodManager.SHOW_IMPLICIT)
                    onFocusChangeListener = null
                }
            }
        }
    }

    fun addTextChangedListener(
            beforeTextChanged : (
                    text : CharSequence?,
                    start : Int,
                    count : Int,
                    after : Int
            ) -> Unit = { _, _, _, _ -> },
            onTextChanged : (
                    text : CharSequence?,
                    start : Int,
                    before : Int,
                    count : Int
            ) -> Unit = { _, _, _, _ -> },
            afterTextChanged : (text : Editable?) -> Unit = {}
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
        binding.searchField.addTextChangedListener(textWatcher)

        return textWatcher
    }
}
