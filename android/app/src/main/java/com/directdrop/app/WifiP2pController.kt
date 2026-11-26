package com.directdrop.app

import android.annotation.SuppressLint
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.net.wifi.p2p.WifiP2pConfig
import android.net.wifi.p2p.WifiP2pDevice
import android.net.wifi.p2p.WifiP2pManager
import android.os.Build

class WifiP2pController(
    private val context: Context,
    private val onPeersChanged: (List<WifiP2pDevice>) -> Unit,
    private val onGroupConnected: (groupOwnerIp: String, isGroupOwner: Boolean) -> Unit,
    private val onStatus: (String) -> Unit
) {
    private val manager = context.getSystemService(Context.WIFI_P2P_SERVICE) as WifiP2pManager
    private val wifiManager = context.applicationContext.getSystemService(Context.WIFI_SERVICE) as android.net.wifi.WifiManager
    private val channel = manager.initialize(context, context.mainLooper, null)

    fun is5GHzSupported(): Boolean = wifiManager.is5GHzBandSupported
    fun is6GHzSupported(): Boolean =
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) wifiManager.is6GHzBandSupported else false

    private val receiver = object : BroadcastReceiver() {
        @SuppressLint("MissingPermission")
        override fun onReceive(ctx: Context, intent: Intent) {
            when (intent.action) {
                WifiP2pManager.WIFI_P2P_PEERS_CHANGED_ACTION -> {
                    manager.requestPeers(channel) { list ->
                        onPeersChanged(list.deviceList.toList())
                    }
                }
                WifiP2pManager.WIFI_P2P_CONNECTION_CHANGED_ACTION -> {
                    manager.requestConnectionInfo(channel) { info ->
                        if (info.groupFormed && info.groupOwnerAddress != null) {
                            onGroupConnected(
                                info.groupOwnerAddress.hostAddress ?: "",
                                info.isGroupOwner
                            )
                        }
                    }
                }
            }
        }
    }

    fun register() {
        val filter = IntentFilter().apply {
            addAction(WifiP2pManager.WIFI_P2P_STATE_CHANGED_ACTION)
            addAction(WifiP2pManager.WIFI_P2P_PEERS_CHANGED_ACTION)
            addAction(WifiP2pManager.WIFI_P2P_CONNECTION_CHANGED_ACTION)
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            context.registerReceiver(receiver, filter, Context.RECEIVER_NOT_EXPORTED)
        } else {
            @Suppress("UnspecifiedRegisterReceiverFlag")
            context.registerReceiver(receiver, filter)
        }
    }

    fun unregister() {
        try { context.unregisterReceiver(receiver) } catch (_: Exception) {}
    }

    @SuppressLint("MissingPermission")
    fun discoverPeers() {
        manager.stopPeerDiscovery(channel, object : WifiP2pManager.ActionListener {
            override fun onSuccess() = startDiscoveryInternal()
            override fun onFailure(reason: Int) = startDiscoveryInternal()
        })
    }

    @SuppressLint("MissingPermission")
    private fun startDiscoveryInternal() {
        manager.discoverPeers(channel, object : WifiP2pManager.ActionListener {
            override fun onSuccess() = onStatus("Searching for Wi-Fi Direct peers...")
            override fun onFailure(reason: Int) {
                val msg = when (reason) {
                    WifiP2pManager.BUSY -> "Wi-Fi Direct busy (code 2). Ensure Wi-Fi is ON and wait a moment."
                    WifiP2pManager.P2P_UNSUPPORTED -> "Wi-Fi Direct unsupported (code 1)."
                    else -> "Discovery failed (code $reason). Check Wi-Fi & Nearby Devices permission."
                }
                onStatus(msg)
            }
        })
    }

    @SuppressLint("MissingPermission")
    fun connectToPeer(device: WifiP2pDevice, band: Int = WifiP2pConfig.GROUP_OWNER_BAND_AUTO) {
        val effectiveBand = when {
            band == WifiP2pConfig.GROUP_OWNER_BAND_5GHZ && !is5GHzSupported() -> {
                onStatus("5 GHz unsupported on local adapter — choose Auto or 2.4 GHz.")
                return
            }
            band == WifiP2pConfig.GROUP_OWNER_BAND_AUTO && is5GHzSupported() -> WifiP2pConfig.GROUP_OWNER_BAND_5GHZ
            else -> band
        }
        val config = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            WifiP2pConfig.Builder()
                .setDeviceAddress(android.net.MacAddress.fromString(device.deviceAddress))
                .setGroupOperatingBand(effectiveBand)
                .build()
        } else {
            WifiP2pConfig().apply { deviceAddress = device.deviceAddress }
        }
        val bandLabel = when (effectiveBand) {
            WifiP2pConfig.GROUP_OWNER_BAND_2GHZ -> "2.4 GHz (Range)"
            WifiP2pConfig.GROUP_OWNER_BAND_5GHZ -> "5 GHz (Speed)"
            else -> "Auto"
        }
        manager.connect(channel, config, object : WifiP2pManager.ActionListener {
            override fun onSuccess() = onStatus("Negotiating $bandLabel group with ${device.deviceName}...")
            override fun onFailure(reason: Int) {
                if (effectiveBand == WifiP2pConfig.GROUP_OWNER_BAND_5GHZ && band == WifiP2pConfig.GROUP_OWNER_BAND_AUTO) {
                    onStatus("5 GHz unavailable; falling back to 2.4 GHz...")
                    connectToPeer(device, WifiP2pConfig.GROUP_OWNER_BAND_2GHZ)
                } else {
                    onStatus("Connect failed (code $reason)")
                }
            }
        })
    }
}
