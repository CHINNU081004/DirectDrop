package com.directdrop.app

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.net.wifi.WifiManager
import android.os.Build
import androidx.core.content.ContextCompat
import java.net.Inet4Address
import java.net.InetSocketAddress
import java.net.NetworkInterface
import java.net.Socket

enum class CapState(val label: String) {
    SUPPORTED("Supported"),
    UNSUPPORTED("Unsupported"),
    PERMISSION_REQUIRED("Permission required"),
    UNKNOWN("Unknown")
}

enum class ActiveTransport {
    WIFI_DIRECT,
    ROUTER_LAN
}

enum class ReachabilityState(val label: String) {
    NOT_TESTED("Not verified yet"),
    REACHABLE("Reachable"),
    UNREACHABLE("Unreachable"),
    CLIENT_ISOLATION_SUSPECTED("Blocked / client isolation suspected")
}

data class WifiBandCapabilities(
    val band24: CapState,
    val band5: CapState,
    val band6: CapState
)

data class WifiDirectCapabilities(
    val available: CapState,
    val band24: CapState,
    val band5: CapState,
    val band6: CapState = CapState.UNSUPPORTED // Android WifiP2pConfig exposes Auto/2.4/5 only
)

data class RouterLanCapabilities(
    val available: CapState,
    val activeFrequencyMhz: Int,
    val linkSpeedMbps: Int,
    val localIps: List<String>
)

data class LocalBandCapabilities(
    val apiLevel: Int,
    val cpuArch: String,
    val wifi: WifiBandCapabilities,
    val wifiDirect: WifiDirectCapabilities,
    val routerLan: RouterLanCapabilities
)

data class PeerCapabilities(
    val name: String = "Peer Device",
    val known: Boolean = false,
    val wifi: WifiBandCapabilities = WifiBandCapabilities(CapState.UNKNOWN, CapState.UNKNOWN, CapState.UNKNOWN),
    val wifiDirectAvailable: CapState = CapState.UNKNOWN,
    val routerLanAvailable: CapState = CapState.UNKNOWN,
    val activeFrequencyMhz: Int = 0
)

data class CompatibilityRow(val label: String, val localVal: String, val peerVal: String)

data class TransferCompatibilityReport(
    val recommendationHeadline: String,
    val recommendationReason: String,
    val activeBandLabel: String,
    val expectedPerformance: String,
    val missingDiagnostics: List<String>,
    val rows: List<CompatibilityRow>
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
        val hasP2pFeature = context.packageManager.hasSystemFeature(PackageManager.FEATURE_WIFI_DIRECT)
        val p2pPerm = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            ContextCompat.checkSelfPermission(context, Manifest.permission.NEARBY_WIFI_DEVICES) == PackageManager.PERMISSION_GRANTED
        } else {
            ContextCompat.checkSelfPermission(context, Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED
        }

        val sup5 = if (wm.is5GHzBandSupported) CapState.SUPPORTED else CapState.UNSUPPORTED
        val sup6 = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            if (wm.is6GHzBandSupported) CapState.SUPPORTED else CapState.UNSUPPORTED
        } else {
            CapState.UNKNOWN
        }

        @Suppress("DEPRECATION")
        val info = try { wm.connectionInfo } catch (_: Exception) { null }
        val freq = info?.frequency ?: 0
        val linkMbps = info?.linkSpeed ?: -1

        val ips = mutableListOf<String>()
        try {
            val ifaces = NetworkInterface.getNetworkInterfaces()
            while (ifaces != null && ifaces.hasMoreElements()) {
                val nif = ifaces.nextElement()
                if (!nif.isUp || nif.isLoopback) continue
                val addrs = nif.inetAddresses
                while (addrs.hasMoreElements()) {
                    val a = addrs.nextElement()
                    if (a is Inet4Address && !a.isLoopbackAddress) {
                        a.hostAddress?.let { ips.add(it) }
                    }
                }
            }
        } catch (_: Exception) {}

        val p2pAvail = when {
            !hasP2pFeature -> CapState.UNSUPPORTED
            !p2pPerm -> CapState.PERMISSION_REQUIRED
            else -> CapState.SUPPORTED
        }

        val wifiCaps = WifiBandCapabilities(CapState.SUPPORTED, sup5, sup6)
        val p2pCaps = WifiDirectCapabilities(
            available = p2pAvail,
            band24 = if (p2pAvail == CapState.SUPPORTED) CapState.SUPPORTED else p2pAvail,
            band5 = if (p2pAvail == CapState.SUPPORTED) sup5 else p2pAvail,
            band6 = CapState.UNSUPPORTED
        )
        val lanCaps = RouterLanCapabilities(
            available = if (ips.isNotEmpty()) CapState.SUPPORTED else CapState.UNSUPPORTED,
            activeFrequencyMhz = if (freq > 0) freq else 0,
            linkSpeedMbps = if (linkMbps > 0) linkMbps else 0,
            localIps = ips
        )
        return LocalBandCapabilities(
            apiLevel = Build.VERSION.SDK_INT,
            cpuArch = Build.SUPPORTED_ABIS.firstOrNull() ?: "unknown",
            wifi = wifiCaps,
            wifiDirect = p2pCaps,
            routerLan = lanCaps
        )
    }

    fun activeBandLabel(mhz: Int): String = when (mhz) {
        in 2400..2500 -> "2.4 GHz ($mhz MHz)"
        in 5150..5900 -> "5 GHz ($mhz MHz)"
        in 5925..7125 -> "6 GHz ($mhz MHz)"
        else -> "Unknown / not reported"
    }

    fun probeTcpReachability(ip: String, port: Int, timeoutMs: Int = 1500): ReachabilityState {
        return try {
            Socket().use { sock ->
                sock.connect(InetSocketAddress(ip, port), timeoutMs)
            }
            ReachabilityState.REACHABLE
        } catch (_: java.net.SocketTimeoutException) {
            ReachabilityState.CLIENT_ISOLATION_SUSPECTED
        } catch (_: Exception) {
            ReachabilityState.UNREACHABLE
        }
    }

    fun compareAndRecommend(
        local: LocalBandCapabilities,
        peer: PeerCapabilities,
        transport: ActiveTransport,
        requestedBand: LiquidBandSelectorView.Band,
        lanReachability: ReachabilityState
    ): TransferCompatibilityReport {
        val activeLabel = activeBandLabel(local.routerLan.activeFrequencyMhz)
        val expectedPerf = if (transport == ActiveTransport.ROUTER_LAN && local.routerLan.linkSpeedMbps > 0) {
            val low = maxOf(1.0, (local.routerLan.linkSpeedMbps * 0.40) / 8.0)
            val high = maxOf(low, (local.routerLan.linkSpeedMbps * 0.65) / 8.0)
            "Link rate: ${local.routerLan.linkSpeedMbps} Mbps • Est. TCP: ~%.1f–%.1f MB/s (depends on router, signal, & storage)".format(low, high)
        } else {
            "Expected speed unknown until link rate is reported or transfer starts measuring."
        }

        fun peerStr(s: CapState) = if (peer.known) s.label else "Unknown"
        val rows = listOf(
            CompatibilityRow("2.4 GHz HW", local.wifi.band24.label, peerStr(peer.wifi.band24)),
            CompatibilityRow("5 GHz HW", local.wifi.band5.label, peerStr(peer.wifi.band5)),
            CompatibilityRow("6 GHz HW", local.wifi.band6.label, peerStr(peer.wifi.band6)),
            CompatibilityRow("Wi-Fi Direct", local.wifiDirect.available.label, peerStr(peer.wifiDirectAvailable)),
            CompatibilityRow("Router/LAN", local.routerLan.available.label, lanReachability.label),
            CompatibilityRow("Active Freq", activeLabel, if (peer.activeFrequencyMhz > 0) activeBandLabel(peer.activeFrequencyMhz) else "Unknown")
        )

        val missing = mutableListOf<String>()
        if (local.wifiDirect.available == CapState.PERMISSION_REQUIRED) {
            missing.add("Nearby Wi-Fi / Location permission is required for Wi-Fi Direct discovery.")
        }
        if (local.wifi.band6 == CapState.SUPPORTED && local.wifiDirect.band6 != CapState.SUPPORTED) {
            missing.add("Your device supports 6 GHz Wi-Fi, but 6 GHz Wi-Fi Direct is not exposed by Android WifiP2pConfig.")
        } else if (local.wifi.band6 == CapState.UNSUPPORTED) {
            missing.add("Your device does not report 6 GHz Wi-Fi support.")
        }
        if (peer.known && peer.wifi.band6 != CapState.SUPPORTED && requestedBand == LiquidBandSelectorView.Band.GHZ_6) {
            missing.add("The other device does not confirm 6 GHz support; a dual/tri-band router may still bridge endpoints over LAN.")
        }
        if (lanReachability == ReachabilityState.CLIENT_ISOLATION_SUSPECTED || lanReachability == ReachabilityState.UNREACHABLE) {
            missing.add("LAN peer is unreachable (check firewall, subnet, or router AP client isolation).")
        }

        val (headline, reason) = when {
            lanReachability == ReachabilityState.REACHABLE ->
                "Recommended: Through Wi-Fi Router (LAN)" to
                    "Local LAN path verified (TLS 1.3 + SAS). Active OS link: $activeLabel. Dual/tri-band routers can bridge different endpoint bands."
            transport == ActiveTransport.WIFI_DIRECT && local.wifiDirect.available == CapState.SUPPORTED -> {
                if (requestedBand == LiquidBandSelectorView.Band.GHZ_6) {
                    "6 GHz Wi-Fi Direct unavailable — use Router/LAN or 5 GHz/Auto" to
                        "Android WifiP2pConfig exposes Auto/2.4/5 GHz only. Switch to Through Wi-Fi Router on a 6 GHz AP or select 5 GHz / Auto."
                } else {
                    "Recommended: Direct (Wi-Fi Direct • ${requestedBand.label})" to
                        "Direct peer-to-peer link without requiring a shared router."
                }
            }
            local.routerLan.available == CapState.SUPPORTED ->
                "Recommended: Through Wi-Fi Router (LAN)" to
                    "Enter peer LAN IP and tap Verify LAN to confirm local reachability."
            else ->
                "Auto mode — check Wi-Fi or permissions" to
                    "Neither Wi-Fi Direct nor an active LAN IP was confirmed."
        }

        return TransferCompatibilityReport(headline, reason, activeLabel, expectedPerf, missing, rows)
    }

    fun buildLearnMoreSections(local: LocalBandCapabilities, report: TransferCompatibilityReport): List<Pair<String, String>> {
        val mismatchText = if (local.wifi.band6 == CapState.SUPPORTED && local.wifiDirect.band6 != CapState.SUPPORTED) {
            "This device supports 6 GHz Wi-Fi, but its current OS or Wi-Fi Direct implementation does not expose a usable 6 GHz Wi-Fi Direct transfer mode. You may still transfer files over a compatible 6 GHz router if both devices join the same local network.\n\nLayers: Wi-Fi HW [2.4G=${local.wifi.band24.label}, 5G=${local.wifi.band5.label}, 6G=${local.wifi.band6.label}] • Wi-Fi Direct=${local.wifiDirect.available.label} • Active Link=${report.activeBandLabel}"
        } else {
            "Wi-Fi hardware support, OS driver exposure, Wi-Fi Direct group-band control, Router/LAN reachability, and the active link frequency are independent layers.\n\nCurrent: Wi-Fi HW [2.4G=${local.wifi.band24.label}, 5G=${local.wifi.band5.label}, 6G=${local.wifi.band6.label}] • Wi-Fi Direct=${local.wifiDirect.available.label} • Active Link=${report.activeBandLabel}"
        }

        val missingText = if (report.missingDiagnostics.isEmpty()) {
            "No capability gaps detected for the current configuration (${report.recommendationHeadline})."
        } else {
            report.missingDiagnostics.joinToString("\n• ", prefix = "• ")
        }

        return listOf(
            "1. Understanding Wi-Fi Bands" to
                "• 2.4 GHz: Better range and wall penetration, often more congestion; practical for older devices and distant rooms.\n• 5 GHz: Higher potential throughput and channel capacity than congested 2.4 GHz, shorter range through walls.\n• 6 GHz: Used by compatible Wi-Fi 6E/7 equipment; shorter range through obstacles; availability depends on hardware, OS, router, and regulations. No fixed speed is promised.",
            "2. Why 6 GHz Wi-Fi Might Work While Wi-Fi Direct Does Not" to mismatchText,
            "3. Why the Other Device Matters" to
                "If one device supports 6 GHz and the other does not, both cannot use 6 GHz directly. Over a dual/tri-band router, however, each device can connect on a different band while bridging local traffic.",
            "4. Why a Powerful Router Can Improve Transfer Speeds" to
                "Better radios/antennas, wider channels, lower congestion, and multi-stream MIMO can improve speed—but the slower endpoint, signal strength, storage speed, and CPU still limit throughput. A 6 GHz router cannot add 6 GHz support to an incompatible device.",
            "5. What Is Missing on This Device?" to missingText
        )
    }
}
