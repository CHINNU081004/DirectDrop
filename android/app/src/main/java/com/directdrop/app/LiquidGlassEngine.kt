package com.directdrop.app

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.LinearGradient
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RadialGradient
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

/**
 * Draws a predominantly transparent, colorless dimensional glass lens
 * with a subtle neutral interior, depth shadow, directional rim lighting,
 * and a small specular highlight that tracks pointer/touch coordinates.
 */
object TransparentGlassPainter {
    private val shadowPaint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val fillPaint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val rimPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeWidth = 2.2f
    }
    private val glintPaint = Paint(Paint.ANTI_ALIAS_FLAG)

    fun drawGlassLens(
        canvas: Canvas,
        rect: RectF,
        cornerRadius: Float,
        lightX: Float,
        lightY: Float,
        alphaScale: Float = 1f
    ) {
        val a = alphaScale.coerceIn(0f, 1f)
        if (a <= 0.01f) return

        // 1. Subtle depth shadow underneath
        shadowPaint.color = Color.argb((42 * a).toInt(), 0, 0, 0)
        canvas.drawRoundRect(
            rect.left, rect.top + 3f, rect.right, rect.bottom + 4f,
            cornerRadius, cornerRadius, shadowPaint
        )

        // 2. Predominantly transparent neutral glass interior (background stays visible)
        fillPaint.shader = LinearGradient(
            rect.left, rect.top, rect.right, rect.bottom,
            Color.argb((34 * a).toInt(), 255, 255, 255),
            Color.argb((14 * a).toInt(), 255, 255, 255),
            Shader.TileMode.CLAMP
        )
        canvas.drawRoundRect(rect, cornerRadius, cornerRadius, fillPaint)

        // 3. Directional luminous glass rim
        rimPaint.shader = LinearGradient(
            lightX - rect.width() * 0.4f, lightY - rect.height() * 0.4f,
            rect.right, rect.bottom,
            intArrayOf(
                Color.argb((210 * a).toInt(), 255, 255, 255),
                Color.argb((70 * a).toInt(), 255, 255, 255),
                Color.argb((130 * a).toInt(), 255, 255, 255)
            ),
            floatArrayOf(0f, 0.55f, 1f),
            Shader.TileMode.CLAMP
        )
        canvas.drawRoundRect(rect, cornerRadius, cornerRadius, rimPaint)

        // 4. Small, natural specular glint following interaction light
        val glintR = maxOf(16f, minOf(rect.width(), rect.height()) * 0.55f)
        val gx = lightX.coerceIn(rect.left + 8f, rect.right - 8f)
        val gy = (rect.top + rect.height() * 0.28f).coerceIn(rect.top + 4f, rect.bottom - 4f)
        glintPaint.shader = RadialGradient(
            gx, gy, glintR,
            intArrayOf(Color.argb((125 * a).toInt(), 255, 255, 255), Color.TRANSPARENT),
            floatArrayOf(0f, 1f),
            Shader.TileMode.CLAMP
        )
        canvas.drawRoundRect(rect, cornerRadius, cornerRadius, glintPaint)
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
