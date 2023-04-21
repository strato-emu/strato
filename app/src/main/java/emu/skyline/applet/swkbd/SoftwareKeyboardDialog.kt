/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.applet.swkbd

import android.annotation.SuppressLint
import android.os.Bundle
import android.text.InputType
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.core.widget.doOnTextChanged
import androidx.fragment.app.DialogFragment
import emu.skyline.databinding.KeyboardDialogBinding
import emu.skyline.utils.parcelable
import emu.skyline.utils.stringFromChars
import java.util.concurrent.FutureTask

data class SoftwareKeyboardResult(val cancelled : Boolean, val text : String)

class SoftwareKeyboardDialog : DialogFragment() {
    private val config by lazy { requireArguments().parcelable<SoftwareKeyboardConfig>("config")!! }
    private val initialText by lazy { requireArguments().getString("initialText")!! }
    private var stopped = false

    companion object {
        /**
         * @param config Holds the [SoftwareKeyboardConfig] that will be used to create the keyboard  between instances of this dialog
         * @param initialText Holds the text that was set by the guest when the dialog was created
         */
        fun newInstance(config : SoftwareKeyboardConfig, initialText : String) : SoftwareKeyboardDialog {
            val args = Bundle()
            args.putParcelable("config", config)
            args.putString("initialText", initialText)
            val fragment = SoftwareKeyboardDialog()
            fragment.arguments = args
            return fragment
        }

        const val validationConfirm = 2
        const val validationError = 1
    }

    private lateinit var binding : KeyboardDialogBinding

    private var cancelled : Boolean = false
    private var futureResult : FutureTask<SoftwareKeyboardResult> = FutureTask<SoftwareKeyboardResult> { return@FutureTask SoftwareKeyboardResult(cancelled, binding.textInput.text.toString()) }


    override fun onCreateView(inflater : LayoutInflater, container : ViewGroup?, savedInstanceState : Bundle?) = if (savedInstanceState?.getBoolean("stopped") != true) KeyboardDialogBinding.inflate(inflater).also { binding = it }.root else null

    @SuppressLint("SetTextI18n")
    override fun onViewCreated(view : View, savedInstanceState : Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        if (config.inputFormMode == InputFormMode.OneLine) {
            binding.header.text = stringFromChars(config.headerText)
            binding.header.visibility = View.VISIBLE
            binding.sub.text = stringFromChars(config.subText)
            binding.sub.visibility = View.VISIBLE
        }

        if (config.keyboardMode == KeyboardMode.Numeric) {
            binding.textInput.inputType = if (config.passwordMode == PasswordMode.Hide) InputType.TYPE_NUMBER_VARIATION_PASSWORD else InputType.TYPE_CLASS_NUMBER
        } else {
            binding.textInput.inputType = if (config.passwordMode == PasswordMode.Hide) InputType.TYPE_TEXT_VARIATION_PASSWORD else InputType.TYPE_CLASS_TEXT
            if (config.invalidCharsFlags.outsideOfDownloadCode)
                binding.textInput.inputType = binding.textInput.inputType or InputType.TYPE_TEXT_FLAG_CAP_CHARACTERS
            if (config.isUseNewLine)
                binding.textInput.inputType = binding.textInput.inputType or InputType.TYPE_TEXT_FLAG_MULTI_LINE
            else
                binding.textInput.inputType = binding.textInput.inputType and InputType.TYPE_TEXT_FLAG_MULTI_LINE.inv()
        }

        val okText = stringFromChars(config.okText)
        if (okText.isNotBlank())
            binding.okButton.text = okText
        val guideText = stringFromChars(config.guideText)
        if (guideText.isNotBlank())
            binding.inputLayout.hint = guideText

        binding.textInput.setText(initialText)
        binding.textInput.setSelection(if (config.initialCursorPos == InitialCursorPos.First) 0 else initialText.length)
        binding.textInput.filters = arrayOf(SoftwareKeyboardFilter(config))
        binding.textInput.doOnTextChanged { text, _, _, _ ->
            binding.okButton.isEnabled = config.isValid(text!!)
            binding.lengthStatus.text = "${text.length}/${config.textMaxLength}"
        }
        binding.lengthStatus.text = "${initialText.length}/${config.textMaxLength}"
        binding.okButton.isEnabled = config.isValid(initialText)
        binding.okButton.setOnClickListener {
            cancelled = false
            futureResult.run()
        }
        if (config.isCancelButtonDisabled) {
            binding.cancelButton.visibility = ViewGroup.GONE
        } else {
            binding.cancelButton.setOnClickListener {
                cancelled = true
                futureResult.run()
            }
        }
    }

    override fun onStart() {
        stopped = false
        super.onStart()
    }

    override fun onStop() {
        stopped = true
        super.onStop()
    }

    override fun onSaveInstanceState(outState : Bundle) {
        outState.putBoolean("stopped", stopped)
        super.onSaveInstanceState(outState)
    }

    fun waitForSubmitOrCancel() : SoftwareKeyboardResult {
        val result = futureResult.get()
        futureResult = FutureTask<SoftwareKeyboardResult> { return@FutureTask SoftwareKeyboardResult(cancelled, binding.textInput.text.toString()) }
        return result
    }
}
