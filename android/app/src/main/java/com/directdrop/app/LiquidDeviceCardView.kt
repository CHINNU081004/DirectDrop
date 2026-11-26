package com.directdrop.app

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RadialGradient
import android.graphics.RectF
import android.graphics.Shader
import android.util.AttributeSet
import android.view.MotionEvent
import android.widget.FrameLayout

class LiquidDeviceCardView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null
) : FrameLayout(context, attrs) {

    var isSelectedCard: Boolean = false
        set(value) {
            field = value
            invalidate()
        }
    var onCardSelectedWithOrigin: ((x: Float, y: Float) -> Unit)? = null

    private val ripple = LiquidRippleState()
    private val pressSpring = Spring1D(0f, stiffness = 340f, damping = 22f)
    private var pointerX = 0f
    private var pointerY = 0f
    private var pointerInside = false
    private var animating = false
    private var lastNanos = 0L

    private val bgPaint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val borderPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeWidth = 2.5f
    }
    private val highlightPaint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val rect = RectF()

    init {
        setWillNotDraw(false)
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        pointerX = event.x
        pointerY = event.y
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                pointerInside = true
                pressSpring.target = 1f
                if (!MotionPrefs.isReducedMotion(context)) {
                    ripple.trigger(event.x, event.y, width.toFloat(), height.toFloat())
                }
                startLoop()
                return true
            }
            MotionEvent.ACTION_MOVE -> {
                invalidate()
                return true
            }
            MotionEvent.ACTION_UP -> {
                pointerInside = false
                pressSpring.target = 0f
                startLoop()
                onCardSelectedWithOrigin?.invoke(event.x, event.y)
                return true
            }
            MotionEvent.ACTION_CANCEL -> {
                pointerInside = false
                pressSpring.target = 0f
                startLoop()
                return true
            }
        }
        return super.onTouchEvent(event)
    }

    private fun startLoop() {
        if (animating) return
        animating = true
        lastNanos = System.nanoTime()
        postOnAnimation(loopRunnable)
    }

    private val loopRunnable = object : Runnable {
        override fun run() {
            val now = System.nanoTime()
            val dt = ((now - lastNanos) / 1_000_000_000f).coerceIn(0.001f, 0.033f)
            lastNanos = now
            val a = pressSpring.step(dt)
            val b = ripple.step(dt)
            scaleX = 1f - pressSpring.value * 0.025f
            scaleY = 1f - pressSpring.value * 0.025f
            invalidate()
            if (a || b) postOnAnimation(this) else animating = false
        }
    }

    override fun onDraw(canvas: Canvas) {
        rect.set(2f, 2f, width - 2f, height - 2f)
        val r = 44f
        bgPaint.color = if (isSelectedCard) Color.argb(65, 0, 229, 255) else Color.argb(30, 255, 255, 255)
        canvas.drawRoundRect(rect, r, r, bgPaint)

        if (pointerInside) {
            highlightPaint.shader = RadialGradient(
                pointerX, pointerY, maxOf(width, height) * 0.6f,
                intArrayOf(Color.argb(75, 255, 255, 255), Color.TRANSPARENT),
                floatArrayOf(0f, 1f),
                Shader.TileMode.CLAMP
            )
            canvas.drawRoundRect(rect, r, r, highlightPaint)
        }

        ripple.draw(canvas)

        borderPaint.color = if (isSelectedCard) Color.argb(200, 0, 229, 255) else Color.argb(85, 255, 255, 255)
        canvas.drawRoundRect(rect, r, r, borderPaint)
        super.onDraw(canvas)
    }
}
