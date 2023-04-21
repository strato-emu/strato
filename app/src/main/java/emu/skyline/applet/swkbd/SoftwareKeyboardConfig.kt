/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

@file:OptIn(ExperimentalUnsignedTypes::class)

package emu.skyline.applet.swkbd

import emu.skyline.utils.*

fun getCodepointArray(vararg chars : Char) : IntArray {
    val codepointArray = IntArray(chars.size)
    for (i in chars.indices)
        codepointArray[i] = chars[i].code
    return codepointArray
}

val DownloadCodeCodepoints = getCodepointArray('0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'J', 'K', 'L', 'M', 'N', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X')
val OutsideOfMiiNicknameCodepoints = getCodepointArray('@', '%', '\\', 'ō', 'Ō', '₽', '₩', '♥', '♡')

/**
 * This data class matches KeyboardMode in skyline/applet/swkbd/software_keyboard_config.h
 */
data class KeyboardMode(
    var mode : u32 = 0u
) : ByteBufferSerializable {
    @Suppress("unused")
    companion object {
        val Full = KeyboardMode(0u)
        val Numeric = KeyboardMode(1u)
        val ASCII = KeyboardMode(2u)
        val FullLatin = KeyboardMode(3u)
        val Alphabet = KeyboardMode(4u)
        val SimplifiedChinese = KeyboardMode(5u)
        val TraditionalChinese = KeyboardMode(6u)
        val Korean = KeyboardMode(7u)
        val LanguageSet2 = KeyboardMode(8u)
        val LanguageSet2Latin = KeyboardMode(9u)
    }
}

/**
 * This data class matches InvalidCharFlags in skyline/applet/swkbd/software_keyboard_config.h
 */
data class InvalidCharFlags(
    var flags : u32 = 0u
) : ByteBufferSerializable {
    // Access each flag as a bool
    var space : Boolean
        get() : Boolean = (flags and 0b1u shl 1) != 0u
        set(value) {
            flags = flags and (0b1u shl 1).inv() or (if (value) 0b1u shl 1 else 0u)
        }
    var atMark : Boolean
        get() : Boolean = (flags and 0b1u shl 2) != 0u
        set(value) {
            flags = flags and (0b1u shl 2).inv() or (if (value) 0b1u shl 2 else 0u)
        }
    var percent : Boolean
        get() : Boolean = (flags and 0b1u shl 3) != 0u
        set(value) {
            flags = flags and (0b1u shl 3).inv() or (if (value) 0b1u shl 3 else 0u)
        }
    var slash : Boolean
        get() : Boolean = (flags and 0b1u shl 4) != 0u
        set(value) {
            flags = flags and (0b1u shl 4).inv() or (if (value) 0b1u shl 4 else 0u)
        }
    var backSlash : Boolean
        get() : Boolean = (flags and 0b1u shl 5) != 0u
        set(value) {
            flags = flags and (0b1u shl 5).inv() or (if (value) 0b1u shl 5 else 0u)
        }
    var numeric : Boolean
        get() : Boolean = (flags and 0b1u shl 6) != 0u
        set(value) {
            flags = flags and (0b1u shl 6).inv() or (if (value) 0b1u shl 6 else 0u)
        }
    var outsideOfDownloadCode : Boolean
        get() : Boolean = (flags and 0b1u shl 7) != 0u
        set(value) {
            flags = flags and (0b1u shl 7).inv() or (if (value) 0b1u shl 7 else 0u)
        }
    var outsideOfMiiNickName : Boolean
        get() : Boolean = (flags and 0b1u shl 8) != 0u
        set(value) {
            flags = flags and (0b1u shl 8).inv() or (if (value) 0b1u shl 8 else 0u)
        }
}

/**
 * This data class matches InitialCursorPos in skyline/applet/swkbd/software_keyboard_config.h
 */
data class InitialCursorPos(
    var pos : u32 = 0u
) : ByteBufferSerializable {
    companion object {
        val First = InitialCursorPos(0u)
        val Last = InitialCursorPos(1u)
    }
}

/**
 * This data class matches PasswordMode in skyline/applet/swkbd/software_keyboard_config.h
 */
data class PasswordMode(
    var mode : u32 = 0u
) : ByteBufferSerializable {
    companion object {
        val Show = PasswordMode(0u)
        val Hide = PasswordMode(1u)
    }
}

/**
 * This data class matches InputFormMode in skyline/applet/swkbd/software_keyboard_config.h
 */
data class InputFormMode(
    var mode : u32 = 0u
) : ByteBufferSerializable {
    @Suppress("unused")
    companion object {
        val OneLine = InputFormMode(0u)
        val MultiLine = InputFormMode(1u)
        val Separate = InputFormMode(2u)
    }
}

/**
 * This data class matches DictionaryLanguage in skyline/applet/swkbd/software_keyboard_config.h
 */
data class DictionaryLanguage(
    var lang : u16 = 0u
) : ByteBufferSerializable

/**
 * This data class matches DictionaryInfo in skyline/applet/swkbd/software_keyboard_config.h
 */
data class DictionaryInfo(
    var offset : u32 = 0u,
    var size : u16 = 0u,
    var dictionaryLang : DictionaryLanguage = DictionaryLanguage()
) : ByteBufferSerializable

/**
 * This data class matches KeyboardConfigVB in skyline/applet/swkbd/software_keyboard_config.h
 */
@Suppress("ArrayInDataClass")
data class SoftwareKeyboardConfig(
    var keyboardMode : KeyboardMode = KeyboardMode(),
    @param:ByteBufferSerializable.ByteBufferSerializableArray(0x9) val okText : CharArray = CharArray(0x9),
    var leftOptionalSymbolKey : Char = '\u0000',
    var rightOptionalSymbolKey : Char = '\u0000',
    var isPredictionEnabled : bool = false,
    @param:ByteBufferSerializable.ByteBufferSerializablePadding(0x1) val _pad0_ : ByteBufferSerializable.ByteBufferPadding = ByteBufferSerializable.ByteBufferPadding,
    var invalidCharsFlags : InvalidCharFlags = InvalidCharFlags(),
    var initialCursorPos : InitialCursorPos = InitialCursorPos(),
    @param:ByteBufferSerializable.ByteBufferSerializableArray(0x41) val headerText : CharArray = CharArray(0x41),
    @param:ByteBufferSerializable.ByteBufferSerializableArray(0x81) val subText : CharArray = CharArray(0x81),
    @param:ByteBufferSerializable.ByteBufferSerializableArray(0x101) val guideText : CharArray = CharArray(0x101),
    @param:ByteBufferSerializable.ByteBufferSerializablePadding(0x2) val _pad2_ : ByteBufferSerializable.ByteBufferPadding = ByteBufferSerializable.ByteBufferPadding,
    var textMaxLength : u32 = 0u,
    var textMinLength : u32 = 0u,
    var passwordMode : PasswordMode = PasswordMode(),
    var inputFormMode : InputFormMode = InputFormMode(),
    var isUseNewLine : bool = false,
    var isUseUtf8 : bool = false,
    var isUseBlurBackground : bool = false,
    @param:ByteBufferSerializable.ByteBufferSerializablePadding(0x1) val _pad3_ : ByteBufferSerializable.ByteBufferPadding = ByteBufferSerializable.ByteBufferPadding,
    var initialStringOffset : u32 = 0u,
    var initialStringLength : u32 = 0u,
    var userDictionaryOffset : u32 = 0u,
    var userDictionaryNum : u32 = 0u,
    var isUseTextCheck : bool = false,
    @param:ByteBufferSerializable.ByteBufferSerializablePadding(0x3) val reserved0 : ByteBufferSerializable.ByteBufferPadding = ByteBufferSerializable.ByteBufferPadding,
    @param:ByteBufferSerializable.ByteBufferSerializableArray(0x8) val separateTextPos : u32Array = u32Array(0x8),
    @param:ByteBufferSerializable.ByteBufferSerializableArray(0x18) val customizedDicInfoList : Array<DictionaryInfo> = Array(0x18) { DictionaryInfo() },
    var customizedDicCount : u8 = 0u,
    var isCancelButtonDisabled : bool = false,
    @param:ByteBufferSerializable.ByteBufferSerializablePadding(0xD) val reserved1 : ByteBufferSerializable.ByteBufferPadding = ByteBufferSerializable.ByteBufferPadding,
    var trigger : u8 = 0u,
    @param:ByteBufferSerializable.ByteBufferSerializablePadding(0x4) val reserved2 : ByteBufferSerializable.ByteBufferPadding = ByteBufferSerializable.ByteBufferPadding,
) : ByteBufferSerializable {
    fun isValid(codepoint : Int) : Boolean {
        when (keyboardMode) {
            KeyboardMode.Numeric -> {
                if (!(codepoint in '0'.code..'9'.code || codepoint == leftOptionalSymbolKey.code || codepoint == rightOptionalSymbolKey.code))
                    return false
            }
            KeyboardMode.ASCII -> {
                if (codepoint !in 0x00..0x7F)
                    return false
            }
        }
        if (invalidCharsFlags.space && Character.isSpaceChar(codepoint))
            return false
        if (invalidCharsFlags.atMark && codepoint == '@'.code)
            return false
        if (invalidCharsFlags.slash && codepoint == '/'.code)
            return false
        if (invalidCharsFlags.backSlash && codepoint == '\\'.code)
            return false
        if (invalidCharsFlags.numeric && codepoint in '0'.code..'9'.code)
            return false
        if (invalidCharsFlags.outsideOfDownloadCode && codepoint !in DownloadCodeCodepoints)
            return false
        if (invalidCharsFlags.outsideOfMiiNickName && codepoint in OutsideOfMiiNicknameCodepoints)
            return false
        if (!isUseNewLine && codepoint == '\n'.code)
            return false
        return true
    }

    fun isValid(string : String) : Boolean {
        if (string.length !in textMinLength.toInt()..textMaxLength.toInt())
            return false
        for (codepoint in string.codePoints()) {
            if (!isValid(codepoint))
                return false
        }
        return true
    }

    fun isValid(chars : CharSequence) : Boolean {
        if (chars.length !in textMinLength.toInt()..textMaxLength.toInt())
            return false
        for (codepoint in chars.codePoints()) {
            if (!isValid(codepoint))
                return false
        }
        return true
    }
}
