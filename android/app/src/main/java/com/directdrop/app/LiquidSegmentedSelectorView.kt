package com.directdrop.app

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RectF
import android.util.AttributeSet
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.View
import kotlin.math.abs
import kotlin.math.max
import kotlin.math.min

/**
 * Reusable movable liquid-glass segmented selector for Android.
 * Supports variable segment widths, horizontal scrolling on narrow screens,
 * spring-animated translucent lens motion, rapid taps, keyboard navigation,
 * and reduced-motion accessibility preferences.
 */
class LiquidSegmentedSelectorView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null
) : View(context, attrs) {

    data class Segment(val id: String, val label: String)

    private var segments: List<Segment> = emptyList()
    var selectedIndex: Int = 0
        private set
    var onSegmentSelected: ((Int, Segment) -> Unit)? = null

    private val lensX = Spring1D(0f, stiffness = 310f, damping = 25f)
    private val lensW = Spring1D(80f, stiffness = 310f, damping = 25f)
    private val pressSpring = Spring1D(0f, stiffness = 360f, damping = 24f)
    private val stretchSpring = Spring1D(0f, stiffness = 280f, damping = 20f)

    private val segLefts = mutableListOf<Float>()
    private val segWidths = mutableListOf<Float>()
    private var contentWidth = 0f
    private var scrollOffset = 0f

    private var downX = 0f
    private var lastX = 0f
    private var isDragging = false
    private var movedBeyondSlop = false
    private var animating = false
    private var lastFrameNanos = 0L

    private val trackPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(26, 255, 255, 255)
        style = Paint.Style.FILL
    }
    private val borderPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(82, 255, 255, 255)
        style = Paint.Style.STROKE
        strokeWidth = 2.2f
    }
    private val textPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        textAlign = Paint.Align.CENTER
        isFakeBoldText = true
    }

    private val trackRect = RectF()
    private val thumbRect = RectF()

    init {
        isFocusable = true
        isClickable = true
    }

    fun setSegments(items: List<Segment>, initialIndex: Int = 0) {
        segments = items
        selectedIndex = if (items.isEmpty()) 0 else initialIndex.coerceIn(0, items.lastIndex)
        recomputeLayout()
        snapLensToSelected()
        updateContentDescription()
        invalidate()
    }

    fun setSelectedIndex(index: Int, notify: Boolean = false) {
        if (segments.isEmpty()) return
        val clamped = index.coerceIn(0, segments.lastIndex)
        val changed = clamped != selectedIndex
        selectedIndex = clamped
        updateContentDescription()

        if (segLefts.size == segments.size) {
            val targetLeft = segLefts[clamped]
            val targetW = segWidths[clamped]
            val dx = targetLeft - lensX.value
            if (MotionPrefs.isReducedMotion(context)) {
                lensX.value = targetLeft
                lensX.target = targetLeft
                lensX.velocity = 0f
                lensW.value = targetW
                lensW.target = targetW
                lensW.velocity = 0f
                invalidate()
            } else {
                lensX.target = targetLeft
                lensW.target = targetW
                stretchSpring.value = (dx / 120f).coerceIn(-0.28f, 0.28f)
                stretchSpring.target = 0f
                startLoop()
            }
            ensureSegmentVisible(clamped)
        }
        if (changed && notify) {
            onSegmentSelected?.invoke(clamped, segments[clamped])
        }
    }

    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        val h = (46 * resources.displayMetrics.density).toInt()
        setMeasuredDimension(MeasureSpec.getSize(widthMeasureSpec), h)
    }

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
        super.onSizeChanged(w, h, oldw, oldh)
        recomputeLayout()
        snapLensToSelected()
    }

    private fun recomputeLayout() {
        segLefts.clear()
        segWidths.clear()
        if (segments.isEmpty() || width <= 0) return

        val d = resources.displayMetrics.density
        textPaint.textSize = 13f * d
        val pad = 4f * d
        val availW = max(1f, width - pad * 2f)
        val minSegPad = 24f * d

        val rawWidths = segments.map { seg ->
            max(58f * d, textPaint.measureText(seg.label) + minSegPad)
        }
        val rawSum = rawWidths.sum()
        val finalWidths = if (rawSum <= availW) {
            val extra = (availW - rawSum) / segments.size
            rawWidths.map { it + extra }
        } else {
            rawWidths
        }

        var x = pad
        finalWidths.forEach { w ->
            segLefts.add(x)
            segWidths.add(w)
            x += w
        }
        contentWidth = x + pad
        scrollOffset = scrollOffset.coerceIn(0f, max(0f, contentWidth - width))
    }

    private fun snapLensToSelected() {
        if (segLefts.size != segments.size || segments.isEmpty()) return
        val idx = selectedIndex.coerceIn(0, segments.lastIndex)
        lensX.value = segLefts[idx]
        lensX.target = segLefts[idx]
        lensX.velocity = 0f
        lensW.value = segWidths[idx]
        lensW.target = segWidths[idx]
        lensW.velocity = 0f
        ensureSegmentVisible(idx)
    }

    private fun ensureSegmentVisible(idx: Int) {
        if (idx !in segLefts.indices || width <= 0) return
        val maxScroll = max(0f, contentWidth - width)
        if (maxScroll <= 0f) {
            scrollOffset = 0f
            return
        }
        val left = segLefts[idx]
        val right = left + segWidths[idx]
        val margin = 16f * resources.displayMetrics.density
        if (left - margin < scrollOffset) {
            scrollOffset = max(0f, left - margin)
        } else if (right + margin > scrollOffset + width) {
            scrollOffset = min(maxScroll, right + margin - width)
        }
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        if (segments.isEmpty()) return super.onTouchEvent(event)
        val d = resources.displayMetrics.density
        val slop = 10f * d
        val maxScroll = max(0f, contentWidth - width)

        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                parent?.requestDisallowInterceptTouchEvent(true)
                downX = event.x
                lastX = event.x
                isDragging = true
                movedBeyondSlop = false
                pressSpring.target = 1f
                if (!MotionPrefs.isReducedMotion(context)) startLoop() else invalidate()
                return true
            }
            MotionEvent.ACTION_MOVE -> {
                val dx = event.x - lastX
                lastX = event.x
                if (abs(event.x - downX) > slop) movedBeyondSlop = true
                if (maxScroll > 0f && movedBeyondSlop) {
                    scrollOffset = (scrollOffset - dx).coerceIn(0f, maxScroll)
                    invalidate()
                } else {
                    val contentX = event.x + scrollOffset
                    val idx = indexAtX(contentX)
                    if (idx in segLefts.indices && !MotionPrefs.isReducedMotion(context)) {
                        lensX.target = segLefts[idx]
                        lensW.target = segWidths[idx]
                        stretchSpring.target = (dx / 18f).coerceIn(-0.30f, 0.30f)
                        startLoop()
                    }
                }
                return true
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                isDragging = false
                pressSpring.target = 0f
                stretchSpring.target = 0f
                if (!movedBeyondSlop || maxScroll <= 0f) {
                    val idx = indexAtX(event.x + scrollOffset)
                    setSelectedIndex(idx, notify = true)
                    performClick()
                } else if (!MotionPrefs.isReducedMotion(context)) {
                    startLoop()
                }
                return true
            }
        }
        return super.onTouchEvent(event)
    }

    override fun performClick(): Boolean {
        super.performClick()
        return true
    }

    override fun onKeyDown(keyCode: Int, event: KeyEvent?): Boolean {
        if (segments.isNotEmpty()) {
            when (keyCode) {
                KeyEvent.KEYCODE_DPAD_LEFT -> {
                    setSelectedIndex(max(0, selectedIndex - 1), notify = true)
                    return true
                }
                KeyEvent.KEYCODE_DPAD_RIGHT -> {
                    setSelectedIndex(min(segments.lastIndex, selectedIndex + 1), notify = true)
                    return true
                }
            }
        }
        return super.onKeyDown(keyCode, event)
    }

    private fun indexAtX(contentX: Float): Int {
        for (i in segLefts.indices) {
            if (contentX >= segLefts[i] && contentX <= segLefts[i] + segWidths[i]) return i
        }
        return if (contentX < contentWidth / 2f) 0 else max(0, segments.lastIndex)
    }

    private fun updateContentDescription() {
        if (selectedIndex in segments.indices) {
            contentDescription = "${segments[selectedIndex].label}, ${selectedIndex + 1} of ${segments.size}"
        }
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

            val a = lensX.step(dt)
            val b = lensW.step(dt)
            val c = pressSpring.step(dt)
            val d = stretchSpring.step(dt)
            invalidate()

            if (a || b || c || d || isDragging) {
                postOnAnimation(this)
            } else {
                animating = false
            }
        }
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        if (segments.isEmpty()) return
        val d = resources.displayMetrics.density
        val pad = 4f

        trackRect.set(pad, pad, width - pad, height - pad)
        val r = trackRect.height() / 2f
        canvas.drawRoundRect(trackRect, r, r, trackPaint)
        canvas.drawRoundRect(trackRect, r, r, borderPaint)

        canvas.save()
        canvas.clipRect(trackRect)
        canvas.translate(-scrollOffset, 0f)

        if (segLefts.size == segments.size) {
            val stretchPx = abs(stretchSpring.value) * lensW.value * 0.22f
            val compressY = pressSpring.value * 2.8f
            val left = lensX.value + 3f - if (stretchSpring.value < 0f) stretchPx else 0f
            val right = lensX.value + lensW.value - 3f + if (stretchSpring.value > 0f) stretchPx else 0f
            thumbRect.set(
                max(pad + 2f, left),
                trackRect.top + 3f + compressY,
                min(max(width.toFloat(), contentWidth) - pad - 2f, right),
                trackRect.bottom - 3f - compressY
            )
            if (thumbRect.width() > 8f && thumbRect.height() > 8f) {
                TransparentGlassPainter.drawGlassLens(canvas, thumbRect, thumbRect.height() / 2f, 0f, 0f, 1f)
            }
        }

        textPaint.textSize = 13f * d
        val textY = trackRect.centerY() - (textPaint.descent() + textPaint.ascent()) / 2f
        segments.forEachIndexed { idx, seg ->
            textPaint.color = if (idx == selectedIndex) Color.WHITE else Color.argb(195, 226, 232, 240)
            val cx = segLefts[idx] + segWidths[idx] * 0.5f
            canvas.drawText(seg.label, cx, textY, textPaint)
        }

        canvas.restore()
    }
}
