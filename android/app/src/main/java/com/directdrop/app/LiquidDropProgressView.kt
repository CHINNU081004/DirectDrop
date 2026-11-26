package com.directdrop.app

import android.animation.ValueAnimator
import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.LinearGradient
import android.graphics.Paint
import android.graphics.RadialGradient
import android.graphics.RectF
import android.graphics.Shader
import android.util.AttributeSet
import android.view.View
import android.view.animation.DecelerateInterpolator

class LiquidDropProgressView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null
) : View(context, attrs) {

    private var animatedProgress: Float = 0f
    private var animator: ValueAnimator? = null

    var progress: Int = 0
        set(value) {
            val clamped = value.coerceIn(0, 100)
            field = clamped
            animator?.cancel()
            animator = ValueAnimator.ofFloat(animatedProgress, clamped.toFloat()).apply {
                duration = 220
                interpolator = DecelerateInterpolator()
                addUpdateListener {
                    animatedProgress = it.animatedValue as Float
                    postInvalidateOnAnimation()
                }
                start()
            }
        }

    private val trackPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(40, 255, 255, 255)
        style = Paint.Style.FILL
    }
    private val borderPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(95, 255, 255, 255)
        style = Paint.Style.STROKE
        strokeWidth = 3f
    }
    private val fillPaint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val dropPaint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val trackRect = RectF()
    private val fillRect = RectF()

    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        val h = (36 * resources.displayMetrics.density).toInt()
        setMeasuredDimension(MeasureSpec.getSize(widthMeasureSpec), h)
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        val pad = height * 0.18f
        val r = (height - 2 * pad) / 2f
        trackRect.set(pad, pad, width - pad, height - pad)

        canvas.drawRoundRect(trackRect, r, r, trackPaint)
        canvas.drawRoundRect(trackRect, r, r, borderPaint)

        val frac = (animatedProgress / 100f).coerceIn(0f, 1f)
        if (frac > 0.001f) {
            val fillW = maxOf(r * 2f, trackRect.width() * frac)
            fillRect.set(trackRect.left, trackRect.top, trackRect.left + fillW, trackRect.bottom)
            fillPaint.shader = LinearGradient(
                fillRect.left, 0f, fillRect.right, 0f,
                Color.parseColor("#2962FF"),
                Color.parseColor("#00E5FF"),
                Shader.TileMode.CLAMP
            )
            canvas.drawRoundRect(fillRect, r, r, fillPaint)
        }

        val dropR = r * 1.25f
        val dropX = trackRect.left + (trackRect.width() - r * 2f) * frac + r
        val dropY = trackRect.centerY()
        dropPaint.shader = RadialGradient(
            dropX - dropR * 0.25f, dropY - dropR * 0.25f, dropR * 1.2f,
            intArrayOf(Color.WHITE, Color.parseColor("#00E5FF"), Color.parseColor("#2962FF")),
            floatArrayOf(0f, 0.4f, 1f),
            Shader.TileMode.CLAMP
        )
        canvas.drawCircle(dropX, dropY, dropR, dropPaint)
        canvas.drawCircle(dropX, dropY, dropR, borderPaint)
    }
}
