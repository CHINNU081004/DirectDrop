package com.directdrop.app

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.LinearGradient
import android.graphics.Paint
import android.graphics.RadialGradient
import android.graphics.RectF
import android.graphics.Shader
import android.util.AttributeSet
import android.view.MotionEvent
import android.view.View
import kotlin.math.abs

class LiquidBandSelectorView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null
) : View(context, attrs) {

    enum class Band(val label: String) {
        AUTO("Auto"),
        GHZ_24("2.4 GHz"),
        GHZ_5("5 GHz"),
        GHZ_6("6 GHz")
    }

    var selectedBand: Band = Band.AUTO
        private set
    var onBandChanged: ((Band) -> Unit)? = null

    private val thumbPos = Spring1D(0f, stiffness = 300f, damping = 24f)
    private val pressSpring = Spring1D(0f, stiffness = 360f, damping = 22f)
    private val stretchSpring = Spring1D(0f, stiffness = 280f, damping = 18f)

    private var pointerX = 0f
    private var pointerY = 0f
    private var lastDragX = 0f
    private var isDragging = false
    private var animating = false
    private var lastFrameNanos = 0L

    private val trackPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(36, 255, 255, 255)
        style = Paint.Style.FILL
    }
    private val borderPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(95, 255, 255, 255)
        style = Paint.Style.STROKE
        strokeWidth = 2.5f
    }
    private val thumbPaint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val highlightPaint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val textPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        textAlign = Paint.Align.CENTER
        isFakeBoldText = true
    }
    private val trackRect = RectF()
    private val thumbRect = RectF()

    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        val h = (48 * resources.displayMetrics.density).toInt()
        setMeasuredDimension(MeasureSpec.getSize(widthMeasureSpec), h)
    }

    fun setBand(band: Band, notify: Boolean = false) {
        selectedBand = band
        if (MotionPrefs.isReducedMotion(context)) {
            thumbPos.value = band.ordinal.toFloat()
            thumbPos.target = band.ordinal.toFloat()
            thumbPos.velocity = 0f
            invalidate()
        } else {
            thumbPos.target = band.ordinal.toFloat()
            startLoop()
        }
        if (notify) onBandChanged?.invoke(band)
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        val count = Band.entries.size
        val segW = width / count.toFloat()
        pointerX = event.x
        pointerY = event.y
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                parent?.requestDisallowInterceptTouchEvent(true)
                isDragging = true
                lastDragX = event.x
                pressSpring.target = 1f
                startLoop()
                return true
            }
            MotionEvent.ACTION_MOVE -> {
                val dx = event.x - lastDragX
                lastDragX = event.x
                val norm = ((event.x / maxOf(1f, width.toFloat())) * count - 0.5f).coerceIn(0f, (count - 1).toFloat())
                thumbPos.target = norm
                stretchSpring.target = (dx / 18f).coerceIn(-0.35f, 0.35f)
                startLoop()
                return true
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                isDragging = false
                pressSpring.target = 0f
                stretchSpring.target = 0f
                val idx = (event.x / maxOf(1f, segW)).toInt().coerceIn(0, count - 1)
                setBand(Band.entries[idx], notify = true)
                return true
            }
        }
        return super.onTouchEvent(event)
    }

    private fun startLoop() {
        if (animating) return
        animating = true
        lastFrameNanos = System.nanoTime()
        postOnAnimation(frameRunnable)
    }

    private val frameRunnable = object : Runnable {
        override fun run() {
            val now = System.nanoTime()
            val dt = ((now - lastFrameNanos) / 1_000_000_000f).coerceIn(0.001f, 0.033f)
            lastFrameNanos = now

            val a = thumbPos.step(dt)
            val b = pressSpring.step(dt)
            val c = stretchSpring.step(dt)
            invalidate()

            if (a || b || c || isDragging) {
                postOnAnimation(this)
            } else {
                animating = false
            }
        }
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        val pad = 4f
        trackRect.set(pad, pad, width - pad, height - pad)
        val r = trackRect.height() / 2f
        canvas.drawRoundRect(trackRect, r, r, trackPaint)
        canvas.drawRoundRect(trackRect, r, r, borderPaint)

        val count = Band.entries.size
        val segW = trackRect.width() / count.toFloat()
        val stretchPx = abs(stretchSpring.value) * segW * 0.25f
        val compressY = pressSpring.value * 3.5f
        val left = trackRect.left + thumbPos.value * segW + 4f - if (stretchSpring.value < 0) stretchPx else 0f
        val right = left + segW - 8f + stretchPx
        thumbRect.set(
            left.coerceAtLeast(trackRect.left + 3f),
            trackRect.top + 3f + compressY,
            right.coerceAtMost(trackRect.right - 3f),
            trackRect.bottom - 3f - compressY
        )

        val tr = thumbRect.height() / 2f
        TransparentGlassPainter.drawGlassLens(canvas, thumbRect, tr, 0f, 0f, 1f)

        textPaint.textSize = 13f * resources.displayMetrics.density
        val textY = trackRect.centerY() - (textPaint.descent() + textPaint.ascent()) / 2f
        Band.entries.forEachIndexed { idx, band ->
            textPaint.color = if (idx == selectedBand.ordinal) Color.WHITE else Color.argb(190, 227, 242, 253)
            canvas.drawText(band.label, trackRect.left + segW * (idx + 0.5f), textY, textPaint)
        }
    }
}
