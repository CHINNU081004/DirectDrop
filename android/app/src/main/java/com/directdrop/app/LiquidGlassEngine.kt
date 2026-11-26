package com.directdrop.app

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RadialGradient
import android.graphics.Shader
import android.provider.Settings
import kotlin.math.cos
import kotlin.math.hypot
import kotlin.math.sin

class Spring1D(
    var value: Float = 0f,
    var velocity: Float = 0f,
    private val stiffness: Float = 260f,
    private val damping: Float = 22f
) {
    var target: Float = value

    fun step(dt: Float): Boolean {
        val force = -stiffness * (value - target) - damping * velocity
        velocity += force * dt
        value += velocity * dt
        return kotlin.math.abs(value - target) > 0.001f || kotlin.math.abs(velocity) > 0.001f
    }
}

object MotionPrefs {
    fun isReducedMotion(context: Context): Boolean {
        return try {
            Settings.Global.getFloat(context.contentResolver, Settings.Global.ANIMATOR_DURATION_SCALE, 1f) == 0f
        } catch (_: Exception) {
            false
        }
    }
}

class LiquidRippleState {
    var active = false
    var originX = 0f
    var originY = 0f
    var progress = 0f
    var maxRadius = 0f
    var phase = 0f

    private val ripplePaint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val rimPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeWidth = 3f
    }
    private val path = Path()

    fun trigger(x: Float, y: Float, width: Float, height: Float) {
        originX = x
        originY = y
        progress = 0.02f
        phase = 0f
        maxRadius = hypot(maxOf(x, width - x), maxOf(y, height - y)) * 1.15f
        active = true
    }

    fun step(dt: Float): Boolean {
        if (!active) return false
        progress += dt * 1.9f
        phase += dt * 9f
        if (progress >= 1f) {
            active = false
            progress = 0f
            return false
        }
        return true
    }

    fun draw(canvas: Canvas) {
        if (!active || maxRadius <= 0f) return
        val r = maxRadius * progress
        val alpha = ((1f - progress) * 140f).toInt().coerceIn(0, 255)

        path.reset()
        val steps = 36
        for (i in 0..steps) {
            val angle = (i.toFloat() / steps) * (Math.PI * 2.0).toFloat()
            val wobble = 1f + 0.045f * (1f - progress) * sin(angle * 4f + phase)
            val px = originX + cos(angle) * r * wobble
            val py = originY + sin(angle) * r * wobble
            if (i == 0) path.moveTo(px, py) else path.lineTo(px, py)
        }
        path.close()

        ripplePaint.shader = RadialGradient(
            originX, originY, maxOf(1f, r),
            intArrayOf(Color.argb(alpha / 3, 255, 255, 255), Color.argb(alpha, 0, 229, 255), Color.TRANSPARENT),
            floatArrayOf(0f, 0.72f, 1f),
            Shader.TileMode.CLAMP
        )
        canvas.drawPath(path, ripplePaint)

        rimPaint.color = Color.argb((alpha * 1.2f).toInt().coerceIn(0, 255), 255, 255, 255)
        canvas.drawPath(path, rimPaint)
    }
}
