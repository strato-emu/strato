/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

@file:OptIn(ExperimentalUnsignedTypes::class)
@file:Suppress("SERIAL")

package emu.skyline.utils

import android.os.Parcel
import android.os.Parcelable
import java.nio.ByteBuffer
import kotlin.Array
import kotlin.Char
import kotlin.CharArray
import kotlin.DoubleArray
import kotlin.FloatArray
import kotlin.reflect.*
import kotlin.reflect.full.*
import java.lang.reflect.Array as javaArray

typealias bool = Boolean
typealias u8 = UByte
typealias u16 = UShort
typealias u32 = UInt
typealias u64 = ULong
typealias float = Float
typealias double = Double
typealias boolArray = BooleanArray
typealias u8Array = UByteArray
typealias u16Array = UShortArray
typealias u32Array = UIntArray
typealias u64Array = ULongArray
typealias floatArray = FloatArray
typealias doubleArray = DoubleArray

fun stringFromChars(chars : CharArray) : String {
    val stringBuilder = StringBuilder()
    for (char in chars) {
        if (char == '\u0000')
            break
        stringBuilder.append(char)
    }
    return stringBuilder.toString()
}

/**
 * Interface for serializing data classes to and from a ByteBuffer
 */
interface ByteBufferSerializable : Parcelable {
    override fun describeContents() : Int {
        return 0
    }

    override fun writeToParcel(out : Parcel, flags : Int) {
        out.writeString(this.javaClass.name)
        out.writeByteArray(this.writeToByteBuffer(ByteBuffer.allocate(ByteBufferSerializationData.getSerializationData(this::class).bytes)).array())
    }

    class ParcelableCreator : Parcelable.ClassLoaderCreator<ByteBufferSerializable> {
        override fun createFromParcel(parcel : Parcel) : ByteBufferSerializable {
            val kClass = javaClass.classLoader?.loadClass(parcel.readString())!!.kotlin
            val byteArray = ByteArray(ByteBufferSerializationData.getSerializationData(kClass).bytes)
            parcel.readByteArray(byteArray)
            return createFromByteBuffer(kClass, ByteBuffer.wrap(byteArray))
        }

        override fun newArray(size : Int) : Array<ByteBufferSerializable?> {
            return arrayOfNulls(size)
        }

        override fun createFromParcel(parcel : Parcel, loader : ClassLoader) : ByteBufferSerializable {
            val kClass = loader.loadClass(parcel.readString())!!.kotlin
            val byteArray = ByteArray(ByteBufferSerializationData.getSerializationData(kClass).bytes)
            parcel.readByteArray(byteArray)
            return createFromByteBuffer(kClass, ByteBuffer.wrap(byteArray))
        }
    }


    /**
     * Annotations required for arrays to mark the length
     * @param length The length of the array
     */
    annotation class ByteBufferSerializableArray(val length : Int)

    /**
     * Annotations required for padding to mark the number of bytes they use
     * @param bytes The number of bytes the padding uses
     */
    annotation class ByteBufferSerializablePadding(val bytes : Int)


    // Exceptions thrown when serialization fails
    class WrongBufferSizeException(val kClass : KClass<*>, val expected : Int, val given : Int, cause : Throwable = Throwable()) : Exception("Serialization of ${kClass.simpleName} expected $expected bytes, but only $given were given", cause)
    class NotADataClassException(val kClass : KClass<*>, cause : Throwable = Throwable()) : Exception("${kClass.simpleName} is not a data class and can't be serialized", cause)
    class NotByteBufferSerializableException(val kClass : KClass<*>, cause : Throwable = Throwable()) : Exception("${kClass.simpleName} is not Serializable", cause)
    class NoArraySizeException(val param : KParameter, cause : Throwable = Throwable()) : Exception("Array parameter ${param.name} isn't annotated as a ByteBufferSerializableArray", cause)
    class NoPaddingSizeException(val param : KParameter, cause : Throwable = Throwable()) : Exception("Padding parameter ${param.name} isn't annotated as a ByteBufferSerializablePadding", cause)
    class InvalidStateException(val kProperty : KProperty1<*, *>, val expectedLength : Int, val length : Int, cause : Throwable = Throwable()) : Exception("Property ${kProperty.name} expected to hold an array of length $expectedLength, but an array of $length was found", cause)
    class SerializationSizeMismatch(val kClass : KClass<*>, val expected : Int, val used : Int, cause : Throwable = Throwable()) : Exception("Serialization of ${kClass.simpleName} expected to use $expected bytes, but only $used were used", cause)

    object ByteBufferPadding

    /**
     * Used to hold information about the size of the object for later reuse
     * @param elementClass The class of the the elements for array types
     */
    private data class ClassAndSize(val kClass : KClass<*>, val size : Int, val length : Int = 1, val elementClass : KClass<*>? = null)

    /**
     * Holds information about the constructor of the classes and the properties for serialization
     */
    @Suppress("ArrayInDataClass")
    private data class ByteBufferSerializationData(val bytes : Int, val classesAndSizes : Array<ClassAndSize>, val properties : Array<KProperty1<*, *>>) {
        companion object {
            fun getSerializationData(kClass : KClass<*>) : ByteBufferSerializationData {
                val prev = cache[kClass]
                // If the class is already in the cache, return it, otherwise calculate it
                if (prev != null)
                    return prev
                // If the class doesn't implement ByteBufferSerializable, throw an exception
                if (!kClass.isSubclassOf(ByteBufferSerializable::class))
                    throw  NotByteBufferSerializableException(kClass)
                // If the class isn't a data class, throw an exception
                if (!kClass.isData)
                    throw NotADataClassException(kClass)
                // The parameters of the constructor, used to calculate the serialization data
                val constructorParams = kClass.primaryConstructor!!.parameters
                val classProperties = kClass.declaredMemberProperties

                // The class' properties, sorted according to the order they appear in the constructor parameters
                val properties = Array<KProperty1<*, *>>(constructorParams.size) { index -> classProperties.first { property -> property.name == (constructorParams[index].name) && constructorParams[index].name != null } }
                // The number of bytes needed for serialization
                var bytes = 0
                val classesAndSizes = Array(constructorParams.size) { index ->
                    val param = constructorParams[index]
                    val classifier = param.type.classifier!!.starProjectedType.classifier
                    // The length of the array, 1 if the parameter isn't an array
                    var length = 1
                    var elementClass : KClass<*>? = null
                    // Get the size and length of the parameter
                    val size = when (classifier) {
                        bool::class -> {
                            1
                        }
                        u8::class -> {
                            1
                        }
                        u16::class -> {
                            2
                        }
                        Char::class -> {
                            2
                        }
                        u32::class -> {
                            4
                        }
                        u64::class -> {
                            8
                        }
                        float::class -> {
                            4
                        }
                        double::class -> {
                            8
                        }
                        // For array parameters, get the length of the array from the annotation and the class of the array elements
                        boolArray::class -> {
                            length = param.findAnnotation<ByteBufferSerializableArray>()?.length ?: throw(NoArraySizeException(param))
                            1
                        }
                        u8Array::class -> {
                            length = param.findAnnotation<ByteBufferSerializableArray>()?.length ?: throw(NoArraySizeException(param))
                            1
                        }
                        u16Array::class -> {
                            length = param.findAnnotation<ByteBufferSerializableArray>()?.length ?: throw(NoArraySizeException(param))
                            2
                        }
                        CharArray::class -> {
                            length = param.findAnnotation<ByteBufferSerializableArray>()?.length ?: throw(NoArraySizeException(param))
                            2
                        }
                        u32Array::class -> {
                            length = param.findAnnotation<ByteBufferSerializableArray>()?.length ?: throw(NoArraySizeException(param))
                            4
                        }
                        u64Array::class -> {
                            length = param.findAnnotation<ByteBufferSerializableArray>()?.length ?: throw(NoArraySizeException(param))
                            8
                        }
                        floatArray::class -> {
                            length = param.findAnnotation<ByteBufferSerializableArray>()?.length ?: throw(NoArraySizeException(param))
                            4
                        }
                        doubleArray::class -> {
                            length = param.findAnnotation<ByteBufferSerializableArray>()?.length ?: throw(NoArraySizeException(param))
                            8
                        }
                        Array::class -> {
                            length = param.findAnnotation<ByteBufferSerializableArray>()?.length ?: throw(NoArraySizeException(param))
                            elementClass = param.type.arguments[0].type!!.classifier as KClass<*>
                            getSerializationData(elementClass).bytes
                        }
                        // For padding, get the size of the padding from the annotation
                        ByteBufferPadding::class -> {
                            param.findAnnotation<ByteBufferSerializablePadding>()?.bytes ?: throw(NoPaddingSizeException(param))
                        }
                        // Get data recursively for [ByteBufferSerializable] classes, throw an exception otherwise
                        else -> {
                            try {
                                getSerializationData(classifier as KClass<*>).bytes
                            } catch (e : Exception) {
                                throw NotByteBufferSerializableException(kClass, e)
                            }
                        }
                    }
                    // Update the total size of the class
                    bytes += size * length
                    ClassAndSize(classifier as KClass<*>, size, length, elementClass)
                }
                val result = ByteBufferSerializationData(bytes, classesAndSizes, properties)
                // Cache the result
                cache[kClass] = result
                return result
            }

            /*
             * The cache of serialization data
             */
            val cache = HashMap<KClass<*>, ByteBufferSerializationData>()
        }
    }

    companion object {
        @JvmField @Suppress("unused")
        val CREATOR : ParcelableCreator = ParcelableCreator()

        /**
         * Creates an object of the kClass Class from a ByteBuffer
         * @param ignoreRemaining if true, and exception will not be thrown if the buffer size exceeds the bytes needed to create the object.
         * @return the Object created from the ByteBuffer.
         */
        fun createFromByteBuffer(kClass : KClass<*>, buffer : ByteBuffer, ignoreRemaining : Boolean = false) : ByteBufferSerializable {
            val serializationData = ByteBufferSerializationData.getSerializationData(kClass)
            val startPos = buffer.position()
            // Throw an exception if the buffer is too small or if it's too big and [ignoreRemaining] is false
            if (serializationData.bytes > buffer.remaining() || (!ignoreRemaining && buffer.remaining() > serializationData.bytes))
                throw WrongBufferSizeException(kClass, serializationData.bytes, buffer.remaining())
            val classesAndSizes = serializationData.classesAndSizes
            // Fill the constructor parameters with the data from the buffer
            val args = Array(classesAndSizes.size) { index ->
                val size = classesAndSizes[index].size
                val length = classesAndSizes[index].length
                val elementClass = classesAndSizes[index].elementClass
                when (val paramClass = classesAndSizes[index].kClass) {
                    bool::class -> {
                        buffer.get() != 0.toByte()
                    }
                    u8::class -> {
                        buffer.get().toUByte()
                    }
                    u16::class -> {
                        buffer.short.toUShort()
                    }
                    Char::class -> {
                        buffer.char
                    }
                    u32::class -> {
                        buffer.int.toUInt()
                    }
                    u64::class -> {
                        buffer.long.toULong()
                    }
                    float::class -> {
                        buffer.float
                    }
                    double::class -> {
                        buffer.double
                    }
                    boolArray::class -> {
                        val temp = BooleanArray(length)
                        for (i in temp.indices)
                            temp[i] = buffer.get() != 0.toByte()
                        temp
                    }
                    // For primitive Array types, use the bulk get methods
                    u8Array::class -> {
                        val temp = u8Array(length)
                        buffer.get(temp.asByteArray())
                        temp
                    }
                    u16Array::class -> {
                        val temp = u16Array(length)
                        buffer.asShortBuffer().get(temp.asShortArray())
                        buffer.position(buffer.position() + length * size)
                        temp
                    }
                    CharArray::class -> {
                        val temp = CharArray(length)
                        buffer.asCharBuffer().get(temp)
                        buffer.position(buffer.position() + length * size)
                        temp

                    }
                    u32Array::class -> {
                        val temp = u32Array(length)
                        buffer.asIntBuffer().get(temp.asIntArray())
                        buffer.position(buffer.position() + length * size)
                        temp
                    }
                    u64Array::class -> {
                        val temp = u64Array(length)
                        buffer.asLongBuffer().get(temp.asLongArray())
                        buffer.position(buffer.position() + length * size)
                        temp
                    }
                    floatArray::class -> {
                        val temp = floatArray(length)
                        buffer.asFloatBuffer().get(temp)
                        buffer.position(buffer.position() + length * size)
                        temp
                    }
                    doubleArray::class -> {
                        val temp = doubleArray(length)
                        buffer.asDoubleBuffer().get(temp)
                        buffer.position(buffer.position() + length * size)
                        temp
                    }
                    Array::class -> {
                        // Use Java reflection to create the array as I couldn't find a way to do this in Kotlin
                        val array = javaArray.newInstance(elementClass!!.java, length)
                        for (i in IntRange(0, length - 1))
                            javaArray.set(array, i, createFromByteBuffer(elementClass, buffer, true))
                        (Array::class.createType(listOf(KTypeProjection.invariant(elementClass.starProjectedType))).classifier as KClass<*>).cast(array)
                    }
                    // For padding, skip the bytes and use the [ByByteBufferPadding] object
                    ByteBufferPadding::class -> {
                        buffer.position(buffer.position() + size)
                        ByteBufferPadding
                    }
                    else -> {
                        paramClass.cast(createFromByteBuffer(paramClass, buffer, true))
                    }
                }
            }
            // Check that the used bytes are equal to the expected size of the object
            if (buffer.position() - startPos != serializationData.bytes)
                throw SerializationSizeMismatch(kClass, serializationData.bytes, buffer.position() - startPos)
            return kClass.primaryConstructor!!.call(*args) as ByteBufferSerializable
        }
    }

    /**
     * Sets this object's state from a ByteBuffer
     * @param ignoreRemaining if true, an exception will not be thrown if the buffer would have remaining bytes after serialization
     */
    fun setFromByteBuffer(buffer : ByteBuffer, ignoreRemaining : Boolean = false) {
        val serializationData = ByteBufferSerializationData.getSerializationData(this::class)
        val startPos = buffer.position()
        // Throw an exception if the buffer is too small or if it's too big and [ignoreRemaining] is false
        if (serializationData.bytes > buffer.remaining() || (!ignoreRemaining && buffer.remaining() > serializationData.bytes))
            throw WrongBufferSizeException(this::class, serializationData.bytes, buffer.remaining())
        val properties = serializationData.properties
        // Set each of the properties using the data from the buffer, or skip them if they're read only
        serializationData.classesAndSizes.forEachIndexed { index, classAndSize ->
            val property = properties[index]
            val length = classAndSize.length
            val size = classAndSize.size
            val elementClass = classAndSize.elementClass
            when (classAndSize.kClass) {
                bool::class -> {
                    val temp = buffer.get() != 0.toByte()
                    (property as? KMutableProperty1<*, *>)?.setter?.call(this, temp)
                }
                u8::class -> {
                    val temp = buffer.get().toUByte()
                    (property as? KMutableProperty1<*, *>)?.setter?.call(this, temp)
                }
                u16::class -> {
                    val temp = buffer.short.toUShort()
                    (property as? KMutableProperty1<*, *>)?.setter?.call(this, temp)
                }
                Char::class -> {
                    val temp = buffer.char
                    (property as? KMutableProperty1<*, *>)?.setter?.call(this, temp)
                }
                u32::class -> {
                    val temp = buffer.int.toUInt()
                    (property as? KMutableProperty1<*, *>)?.setter?.call(this, temp)
                }
                u64::class -> {
                    val temp = buffer.long.toULong()
                    (property as? KMutableProperty1<*, *>)?.setter?.call(this, temp)
                }
                float::class -> {
                    val temp = buffer.float
                    (property as? KMutableProperty1<*, *>)?.setter?.call(this, temp)
                }
                double::class -> {
                    val temp = buffer.double
                    (property as? KMutableProperty1<*, *>)?.setter?.call(this, temp)
                }
                boolArray::class -> {
                    val temp = property.getter.call(this) as boolArray
                    if (temp.size == length) {
                        for (i in temp.indices)
                            temp[i] = buffer.get() != 0.toByte()
                    } else {
                        throw InvalidStateException(property, length, temp.size)
                    }
                }
                // For primitive arrays, use the bulk get methods
                u8Array::class -> {
                    val temp = property.getter.call(this) as u8Array
                    if (temp.size == length)
                        buffer.get(temp.asByteArray())
                    else
                        throw InvalidStateException(property, length, temp.size)
                }
                u16Array::class -> {
                    val temp = property.getter.call(this) as u16Array
                    if (temp.size == length) {
                        buffer.asShortBuffer().get(temp.asShortArray())
                        buffer.position(buffer.position() + length * size)
                    } else {
                        throw InvalidStateException(property, length, temp.size)
                    }
                }
                CharArray::class -> {
                    val temp = property.getter.call(this) as CharArray
                    if (temp.size == length) {
                        buffer.asCharBuffer().get(temp)
                        buffer.position(buffer.position() + length * size)
                    } else {
                        throw InvalidStateException(property, length, temp.size)
                    }
                }
                u32Array::class -> {
                    val temp = property.getter.call(this) as u32Array
                    if (temp.size == length) {
                        buffer.asIntBuffer().get(temp.asIntArray())
                        buffer.position(buffer.position() + length * size)
                    } else {
                        throw InvalidStateException(property, length, temp.size)
                    }
                }
                u64Array::class -> {
                    val temp = property.getter.call(this) as u64Array
                    if (temp.size == length) {
                        buffer.asLongBuffer().get(temp.asLongArray())
                        buffer.position(buffer.position() + length * size)
                    } else {
                        throw InvalidStateException(property, length, temp.size)
                    }
                }
                floatArray::class -> {
                    val temp = property.getter.call(this) as floatArray
                    if (temp.size == length) {
                        buffer.asFloatBuffer().get(temp)
                        buffer.position(buffer.position() + length * size)
                    } else {
                        throw InvalidStateException(property, length, temp.size)
                    }
                }
                doubleArray::class -> {
                    val temp = property.getter.call(this) as doubleArray
                    if (temp.size == length) {
                        buffer.asDoubleBuffer().get(temp)
                        buffer.position(buffer.position() + length * size)
                    } else {
                        throw InvalidStateException(property, length, temp.size)
                    }
                }
                Array::class -> {
                    val temp = property.getter.call(this) as Array<*>
                    if (temp.size == length)
                        temp.forEach { curr -> (elementClass!!.cast(curr) as ByteBufferSerializable).setFromByteBuffer(buffer, true) }
                    else
                        throw InvalidStateException(property, length, temp.size)
                }
                ByteBufferPadding::class -> {
                    buffer.position(buffer.position() + size)

                }
                else -> {
                    (property.getter.call(this) as ByteBufferSerializable).setFromByteBuffer(buffer, true)
                }
            }
        }
        // Check that the used bytes are equal to the expected size of the object
        if (buffer.position() - startPos != serializationData.bytes)
            throw SerializationSizeMismatch(this::class, serializationData.bytes, buffer.position() - startPos)
    }

    /**
     * Write this object to a ByteBuffer
     * @param ignoreRemaining If true, an exception will not be thrown if the buffer would have remaining bytes after writing the object
     */
    fun writeToByteBuffer(buffer : ByteBuffer, ignoreRemaining : Boolean = false) : ByteBuffer {
        val serializationData = ByteBufferSerializationData.getSerializationData(this::class)
        val startPos = buffer.position()
        // Throw an exception if the buffer is too small or if it's too big and [ignoreRemaining] is false
        if (serializationData.bytes > buffer.remaining() || (!ignoreRemaining && buffer.remaining() > serializationData.bytes))
            throw WrongBufferSizeException(this::class, serializationData.bytes, buffer.remaining())
        val properties = serializationData.properties
        serializationData.classesAndSizes.forEachIndexed { index, classAndSize ->
            val property = properties[index]
            val length = classAndSize.length
            val size = classAndSize.size
            val elementClass = classAndSize.elementClass
            when (classAndSize.kClass) {
                bool::class -> {
                    buffer.put(if (property.getter.call(this) as Boolean) 1 else 0)
                }

                u8::class -> {
                    buffer.put((property.getter.call(this) as u8).toByte())
                }

                u16::class -> {
                    buffer.putShort((property.getter.call(this) as u16).toShort())
                }

                Char::class -> {
                    buffer.putChar(property.getter.call(this) as Char)
                }

                u32::class -> {
                    buffer.putInt((property.getter.call(this) as u32).toInt())
                }

                u64::class -> {
                    buffer.putLong((property.getter.call(this) as u64).toLong())
                }

                float::class -> {
                    buffer.putFloat(property.getter.call(this) as float)
                }

                double::class -> {
                    buffer.putDouble(property.getter.call(this) as double)
                }

                boolArray::class -> {
                    val temp = property.getter.call(this) as boolArray
                    if (temp.size == length)
                        temp.forEach { curr -> buffer.put(if (curr) 1 else 0) }
                    else
                        throw InvalidStateException(property, length, temp.size)
                }

                // For primitive arrays, use the bulk write methods
                u8Array::class -> {
                    val temp = property.getter.call(this) as u8Array
                    if (temp.size == length)
                        buffer.put(temp.asByteArray())
                    else
                        throw InvalidStateException(property, length, temp.size)
                }

                u16Array::class -> {
                    val temp = property.getter.call(this) as u16Array
                    if (temp.size == length) {
                        buffer.asShortBuffer().put(temp.asShortArray())
                        buffer.position(buffer.position() + length * size)
                    } else {
                        throw InvalidStateException(property, length, temp.size)
                    }
                }

                CharArray::class -> {
                    val temp = property.getter.call(this) as CharArray
                    if (temp.size == length) {
                        buffer.asCharBuffer().put(temp)
                        buffer.position(buffer.position() + length * size)
                    } else {
                        throw InvalidStateException(property, length, temp.size)
                    }
                }

                u32Array::class -> {
                    val temp = property.getter.call(this) as u32Array
                    if (temp.size == length) {
                        buffer.asIntBuffer().put(temp.asIntArray())
                        buffer.position(buffer.position() + length * size)
                    } else {
                        throw InvalidStateException(property, length, temp.size)
                    }
                }

                u64Array::class -> {
                    val temp = property.getter.call(this) as u64Array
                    if (temp.size == length) {
                        buffer.asLongBuffer().put(temp.asLongArray())
                        buffer.position(buffer.position() + length * size)
                    } else {
                        throw InvalidStateException(property, length, temp.size)
                    }
                }

                FloatArray::class -> {
                    val temp = property.getter.call(this) as FloatArray
                    if (temp.size == length) {
                        buffer.asFloatBuffer().put(temp)
                        buffer.position(buffer.position() + length * size)
                    } else {
                        throw InvalidStateException(property, length, temp.size)
                    }
                }

                DoubleArray::class -> {
                    val temp = property.getter.call(this) as DoubleArray
                    if (temp.size == length) {
                        buffer.asDoubleBuffer().put(temp)
                        buffer.position(buffer.position() + length * size)
                    } else {
                        throw InvalidStateException(property, length, temp.size)
                    }
                }

                Array::class -> {
                    val temp = property.getter.call(this) as Array<*>
                    if (temp.size == length)
                        temp.forEach { curr -> (elementClass!!.cast(curr) as ByteBufferSerializable).writeToByteBuffer(buffer, true) }
                    else
                        throw InvalidStateException(property, length, temp.size)
                }
                // For padding, just skip the bytes.
                ByteBufferPadding::class -> {
                    buffer.position(buffer.position() + size)
                }
                else -> {
                    (property.getter.call(this) as ByteBufferSerializable).writeToByteBuffer(buffer, true)
                }
            }
        }
        // Check that the written bytes are equal to the expected size of the object.
        if (buffer.position() - startPos != serializationData.bytes)
            throw SerializationSizeMismatch(this::class, serializationData.bytes, buffer.position() - startPos)
        return buffer
    }
}
