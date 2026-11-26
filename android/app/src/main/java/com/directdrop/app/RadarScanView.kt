package com.directdrop.app

import android.animation.ValueAnimator
import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.util.AttributeSet
import android.view.View
import android.view.animation.LinearInterpolator

class RadarScanView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null
) : View(context, attrs) {

    private var phase: Float = 0f
    private var scanning: Boolean = false
    private val animator = ValueAnimator.ofFloat(0f, 1f).apply {
        duration = 1400
        repeatCount = ValueAnimator.INFINITE
        interpolator = LinearInterpolator()
        addUpdateListener {
            phase = it.animatedValue as Float
            postInvalidateOnAnimation()
        }
    }

    private val dotPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.parseColor("#00E5FF")
        style = Paint.Style.FILL
    }
    private val ringPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeWidth = 4f
    }

    fun setScanning(active: Boolean) {
        scanning = active
        if (active && !animator.isStarted) animator.start()
        else if (!active) animator.cancel()
        invalidate()
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        val cx = width / 2f
        val cy = height / 2f
        val maxR = minOf(cx, cy) - 6f

        dotPaint.color = if (scanning) Color.parseColor("#00E5FF") else Color.argb(150, 130, 177, 255)
        canvas.drawCircle(cx, cy, 10f, dotPaint)

        if (scanning) {
            for (i in 0 until 3) {
                var t = phase + i / 3f
                if (t >= 1f) t -= 1f
                val r = 12f + (maxR - 12f) * t
                ringPaint.color = Color.argb((210 * (1f - t)).toInt(), 0, 229, 255)
                canvas.drawCircle(cx, cy, r, ringPaint)
            }
        } else {
            ringPaint.color = Color.argb(65, 255, 255, 255)
            canvas.drawCircle(cx, cy, maxR * 0.65f, ringPaint)
        }
    }
}
