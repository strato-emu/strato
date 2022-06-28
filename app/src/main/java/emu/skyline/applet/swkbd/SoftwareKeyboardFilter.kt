/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.applet.swkbd

import android.text.InputFilter
import android.text.SpannableStringBuilder
import android.text.Spanned

class SoftwareKeyboardFilter(val config : SoftwareKeyboardConfig) : InputFilter {
    override fun filter(source : CharSequence, start : Int, end : Int, dest : Spanned, dstart : Int, dend : Int) : CharSequence? {
        val filteredStringBuilder = SpannableStringBuilder()
        val newCharacterIndex = arrayOfNulls<Int>(end - start)
        var currentIndex : Int

        val sourceCodepoints = source.subSequence(start, end).codePoints().iterator()
        val replacedLength = dest.subSequence(dstart, dend).length

        var nextIndex = start
        while (sourceCodepoints.hasNext()) {
            var codepoint = sourceCodepoints.next()
            currentIndex = nextIndex
            nextIndex += Character.charCount(codepoint)

            if (config.invalidCharsFlags.outsideOfDownloadCode)
                codepoint = Character.toUpperCase(codepoint)
            if (!config.isValid(codepoint))
                continue
            // Check if the string would be over the maximum length after adding this character
            if (dest.length - replacedLength + filteredStringBuilder.length + Character.charCount(codepoint) > config.textMaxLength.toInt())
                break
            else
                // Keep track where in the new string each character ended up
                newCharacterIndex[currentIndex - start] = filteredStringBuilder.length
            filteredStringBuilder.append(String(Character.toChars(codepoint)))
        }

        if (source is Spanned) {
            // Copy the spans from the source to the returned SpannableStringBuilder
                for (span in source.getSpans(start, end, Any::class.java)) {
                    val spanStart = source.getSpanStart(span) - start
                    val spanEnd = source.getSpanEnd(span) - start

                    currentIndex = spanStart
                    var newStart = newCharacterIndex[currentIndex]
                    // Make the new span's start be at the first character that was in the span's range and wasn't removed
                    while (newStart == null && currentIndex < spanEnd - 1) {
                        newStart = newCharacterIndex[++currentIndex]
                    }

                    currentIndex = spanEnd - 1
                    var newEnd = newCharacterIndex[currentIndex]
                    // Make the span end be at the last character that was in the span's range and wasn't removed
                    while (newEnd == null && currentIndex > spanStart) {
                        newEnd = newCharacterIndex[--currentIndex]
                    }

                    if(newStart != null && newEnd != null)
                        filteredStringBuilder.setSpan(span, newStart, newEnd + 1, source.getSpanFlags(span))
                }
        }
        return filteredStringBuilder
    }
}
