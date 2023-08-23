package org.stratoemu.strato.views

import android.content.Context
import android.util.AttributeSet
import android.util.Rational
import android.view.SurfaceView
import kotlin.math.roundToInt

class FixedRatioSurfaceView @JvmOverloads constructor(context : Context, attrs : AttributeSet? = null, defStyleAttr : Int = 0) : SurfaceView(context, attrs, defStyleAttr) {
    private var aspectRatio : Float = 0f // (width / height), 0f is a special value for stretch

    /**
     * Sets the desired aspect ratio for this view
     * @param ratio the ratio to force the view to, or null to stretch to fit
     */
    fun setAspectRatio(ratio : Rational?) {
        aspectRatio = ratio?.toFloat() ?: 0f
    }

    override fun onMeasure(widthMeasureSpec : Int, heightMeasureSpec : Int) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec)
        val width = MeasureSpec.getSize(widthMeasureSpec)
        val height = MeasureSpec.getSize(heightMeasureSpec)
        if (aspectRatio != 0f) {
            val newWidth : Int
            val newHeight : Int
            if (height * aspectRatio < width) {
                newWidth = (height * aspectRatio).roundToInt()
                newHeight = height
            } else {
                newWidth = width
                newHeight = (width / aspectRatio).roundToInt()
            }
            setMeasuredDimension(newWidth, newHeight)
        } else {
            setMeasuredDimension(width, height)
        }
    }
}
