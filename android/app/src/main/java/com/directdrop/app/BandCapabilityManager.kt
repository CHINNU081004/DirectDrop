package com.directdrop.app

import android.content.Context
import android.content.pm.PackageManager
import android.net.wifi.WifiManager
import android.os.Build

enum class CapabilityState {
    SUPPORTED_AND_AVAILABLE,
    SUPPORTED_BUT_UNAVAILABLE,
    UNSUPPORTED,
    PERMISSION_REQUIRED,
    UNKNOWN
}

enum class ActiveTransport {
    WIFI_DIRECT,
    SECURE_LAN_TLS
}

data class LocalBandCapabilities(
    val apiLevel: Int,
    val cpuArch: String,
    val supports24GHz: Boolean,
    val supports5GHz: Boolean,
    val supports6GHz: Boolean,
    val supportsWifiDirect: Boolean,
    val p2pBandControlSupports6GHz: Boolean = false, // Android WifiP2pConfig exposes Auto/2.4/5 only
    val activeFrequencyMhz: Int = 0
)

data class BandEvaluation(
    val requested: LiquidBandSelectorView.Band,
    val state: CapabilityState,
    val statusHeadline: String,
    val explanation: String,
    val activeBandLabel: String
)

object BandCapabilityManager {
    private const val PREFS_NAME = "directdrop_prefs"
    private const val KEY_BAND = "band_preference"

    fun loadSavedBand(context: Context): LiquidBandSelectorView.Band {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        val idx = prefs.getInt(KEY_BAND, 0).coerceIn(0, LiquidBandSelectorView.Band.entries.lastIndex)
        return LiquidBandSelectorView.Band.entries[idx]
    }

    fun saveBand(context: Context, band: LiquidBandSelectorView.Band) {
        context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
            .edit()
            .putInt(KEY_BAND, band.ordinal)
            .apply()
    }

    fun probe(context: Context): LocalBandCapabilities {
        val wm = context.applicationContext.getSystemService(Context.WIFI_SERVICE) as WifiManager
        val hasP2p = context.packageManager.hasSystemFeature(PackageManager.FEATURE_WIFI_DIRECT)
        val sup5 = wm.is5GHzBandSupported
        val sup6 = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) wm.is6GHzBandSupported else false
        @Suppress("DEPRECATION")
        val freq = try { wm.connectionInfo?.frequency ?: 0 } catch (_: Exception) { 0 }
        return LocalBandCapabilities(
            apiLevel = Build.VERSION.SDK_INT,
            cpuArch = Build.SUPPORTED_ABIS.firstOrNull() ?: "unknown",
            supports24GHz = true,
            supports5GHz = sup5,
            supports6GHz = sup6,
            supportsWifiDirect = hasP2p,
            p2pBandControlSupports6GHz = false,
            activeFrequencyMhz = if (freq > 0) freq else 0
        )
    }

    fun activeBandLabel(mhz: Int): String = when (mhz) {
        in 2400..2500 -> "2.4 GHz ($mhz MHz)"
        in 5150..5900 -> "5 GHz ($mhz MHz)"
        in 5925..7125 -> "6 GHz ($mhz MHz)"
        else -> "Band information unavailable"
    }

    fun evaluate(
        requested: LiquidBandSelectorView.Band,
        cap: LocalBandCapabilities,
        transport: ActiveTransport
    ): BandEvaluation {
        val activeLabel = activeBandLabel(cap.activeFrequencyMhz)
        return when (requested) {
            LiquidBandSelectorView.Band.AUTO -> BandEvaluation(
                requested = requested,
                state = CapabilityState.SUPPORTED_AND_AVAILABLE,
                statusHeadline = "Auto — choosing the best available connection",
                explanation = if (transport == ActiveTransport.WIFI_DIRECT) {
                    if (cap.supports5GHz) "Prefers 5 GHz for speed when supported; 2.4 GHz fallback for range."
                    else "Using 2.4 GHz (5 GHz not reported by local adapter)."
                } else {
                    "Secure LAN uses the active Wi-Fi AP band ($activeLabel)."
                },
                activeBandLabel = activeLabel
            )
            LiquidBandSelectorView.Band.GHZ_24 -> BandEvaluation(
                requested = requested,
                state = CapabilityState.SUPPORTED_AND_AVAILABLE,
                statusHeadline = "Manual preference: 2.4 GHz",
                explanation = if (transport == ActiveTransport.WIFI_DIRECT) {
                    "Requests 2.4 GHz Wi-Fi Direct group band for wall penetration & range."
                } else {
                    "LAN cannot force router band; active OS link reports: $activeLabel."
                },
                activeBandLabel = activeLabel
            )
            LiquidBandSelectorView.Band.GHZ_5 -> {
                if (!cap.supports5GHz) {
                    BandEvaluation(
                        requested = requested,
                        state = CapabilityState.UNSUPPORTED,
                        statusHeadline = "5 GHz unavailable — choose Auto or another band",
                        explanation = "Adapter did not report 5 GHz support (manual preference preserved).",
                        activeBandLabel = activeLabel
                    )
                } else {
                    BandEvaluation(
                        requested = requested,
                        state = CapabilityState.SUPPORTED_AND_AVAILABLE,
                        statusHeadline = "Manual preference: 5 GHz",
                        explanation = if (transport == ActiveTransport.WIFI_DIRECT) {
                            "Requests 5 GHz Wi-Fi Direct group band via Android WifiP2pConfig."
                        } else {
                            "LAN cannot force router band; active OS link reports: $activeLabel."
                        },
                        activeBandLabel = activeLabel
                    )
                }
            }
            LiquidBandSelectorView.Band.GHZ_6 -> {
                if (!cap.supports6GHz) {
                    BandEvaluation(
                        requested = requested,
                        state = CapabilityState.UNSUPPORTED,
                        statusHeadline = "6 GHz unavailable — choose Auto or another band",
                        explanation = "Local adapter did not report 6 GHz (Wi-Fi 6E) capability (manual preference preserved).",
                        activeBandLabel = activeLabel
                    )
                } else if (transport == ActiveTransport.WIFI_DIRECT && !cap.p2pBandControlSupports6GHz) {
                    BandEvaluation(
                        requested = requested,
                        state = CapabilityState.SUPPORTED_BUT_UNAVAILABLE,
                        statusHeadline = "6 GHz supported, but unavailable for Wi-Fi Direct",
                        explanation = "Android WifiP2pConfig exposes Auto/2.4/5 GHz only. Connect via Secure LAN on a 6 GHz router or pick 5 GHz / Auto.",
                        activeBandLabel = activeLabel
                    )
                } else {
                    BandEvaluation(
                        requested = requested,
                        state = CapabilityState.SUPPORTED_AND_AVAILABLE,
                        statusHeadline = "Manual preference: 6 GHz",
                        explanation = "Secure LAN cannot force router band; active OS link reports: $activeLabel.",
                        activeBandLabel = activeLabel
                    )
                }
            }
        }
    }
}
