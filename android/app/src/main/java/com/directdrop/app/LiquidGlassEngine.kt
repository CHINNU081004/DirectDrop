package com.directdrop.app

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.LinearGradient
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RectF
import android.graphics.Shader
import android.provider.Settings
import kotlin.math.abs
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
        return abs(value - target) > 0.001f || abs(velocity) > 0.001f
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

object LiquidGlassTokens {
    val surfaceNeutral: Int = Color.argb(28, 255, 255, 255)
    val surfaceElevated: Int = Color.argb(44, 255, 255, 255)
    val rimStroke: Int = Color.argb(95, 255, 255, 255)
    val textPrimary: Int = Color.WHITE
    val textSecondary: Int = Color.argb(200, 226, 232, 240)
}

/**
 * Draws a predominantly transparent, colorless dimensional glass lens
 * with a subtle neutral interior, depth shadow, and a stable, restrained
 * edge reflection (no moving interior hotspot).
 */
object TransparentGlassPainter {
    private val shadowPaint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val fillPaint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val sheenPaint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val rimPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeWidth = 1.8f
    }

    fun drawGlassLens(
        canvas: Canvas,
        rect: RectF,
        cornerRadius: Float,
        @Suppress("UNUSED_PARAMETER") lightX: Float = 0f,
        @Suppress("UNUSED_PARAMETER") lightY: Float = 0f,
        alphaScale: Float = 1f
    ) {
        val a = alphaScale.coerceIn(0f, 1f)
        if (a <= 0.01f) return

        // 1. Subtle depth shadow underneath
        shadowPaint.color = Color.argb((38 * a).toInt(), 0, 0, 0)
        canvas.drawRoundRect(
            rect.left, rect.top + 2.5f, rect.right, rect.bottom + 3.5f,
            cornerRadius, cornerRadius, shadowPaint
        )

        // 2. Predominantly transparent neutral glass interior (background stays visible)
        fillPaint.shader = LinearGradient(
            rect.left, rect.top, rect.right, rect.bottom,
            Color.argb((26 * a).toInt(), 255, 255, 255),
            Color.argb((10 * a).toInt(), 255, 255, 255),
            Shader.TileMode.CLAMP
        )
        canvas.drawRoundRect(rect, cornerRadius, cornerRadius, fillPaint)

        // 3. Subtle fixed top-edge inner sheen (no bright moving spot)
        sheenPaint.shader = LinearGradient(
            rect.left, rect.top, rect.left, rect.top + rect.height() * 0.45f,
            Color.argb((32 * a).toInt(), 255, 255, 255),
            Color.TRANSPARENT,
            Shader.TileMode.CLAMP
        )
        canvas.drawRoundRect(rect, cornerRadius, cornerRadius, sheenPaint)

        // 4. Restrained, stable directional rim highlight
        rimPaint.shader = LinearGradient(
            rect.left, rect.top, rect.right, rect.bottom,
            intArrayOf(
                Color.argb((150 * a).toInt(), 255, 255, 255),
                Color.argb((52 * a).toInt(), 255, 255, 255),
                Color.argb((100 * a).toInt(), 255, 255, 255)
            ),
            floatArrayOf(0f, 0.5f, 1f),
            Shader.TileMode.CLAMP
        )
        canvas.drawRoundRect(rect, cornerRadius, cornerRadius, rimPaint)
    }
}

class LiquidRippleState {
    var active = false
    var originX = 0f
    var originY = 0f
    var progress = 0f
    var maxRadius = 0f
    var phase = 0f

    private val innerCausticPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeWidth = 5f
    }
    private val rimPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeWidth = 2.2f
    }
    private val path = Path()

    fun trigger(x: Float, y: Float, width: Float, height: Float) {
        originX = x
        originY = y
        progress = 0.02f
        phase = 0f
        maxRadius = hypot(maxOf(x, width - x), maxOf(y, height - y)) * 1.12f
        active = true
    }

    fun step(dt: Float): Boolean {
        if (!active) return false
        progress += dt * 1.85f
        phase += dt * 7.5f
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
        val fade = (1f - progress)

        path.reset()
        val steps = 40
        for (i in 0..steps) {
            val angle = (i.toFloat() / steps) * (Math.PI * 2.0).toFloat()
            val wobble = 1f + 0.035f * fade * sin(angle * 4f + phase)
            val px = originX + cos(angle) * r * wobble
            val py = originY + sin(angle) * r * wobble
            if (i == 0) path.moveTo(px, py) else path.lineTo(px, py)
        }
        path.close()

        // Transparent interior — only a soft refracted caustic band + crisp glass rim
        innerCausticPaint.color = Color.argb((42 * fade).toInt().coerceIn(0, 255), 255, 255, 255)
        canvas.drawPath(path, innerCausticPaint)

        rimPaint.color = Color.argb((185 * fade).toInt().coerceIn(0, 255), 255, 255, 255)
        canvas.drawPath(path, rimPaint)
    }
}
