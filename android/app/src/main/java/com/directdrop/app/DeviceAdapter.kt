package com.directdrop.app

import android.net.wifi.p2p.WifiP2pDevice
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView

class DeviceAdapter(
    private val onDeviceSelected: (WifiP2pDevice) -> Unit
) : RecyclerView.Adapter<DeviceAdapter.VH>() {

    private var devices: List<WifiP2pDevice> = emptyList()
    private var selectedAddress: String? = null

    fun submitList(list: List<WifiP2pDevice>) {
        devices = list
        notifyDataSetChanged()
    }

    class VH(v: View) : RecyclerView.ViewHolder(v) {
        val card: LiquidDeviceCardView = v.findViewById(R.id.deviceCard)
        val name: TextView = v.findViewById(R.id.deviceNameText)
        val detail: TextView = v.findViewById(R.id.deviceDetailText)
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): VH {
        val v = LayoutInflater.from(parent.context).inflate(R.layout.item_device_card, parent, false)
        return VH(v)
    }

    override fun getItemCount(): Int = devices.size

    override fun onBindViewHolder(holder: VH, position: Int) {
        val dev = devices[position]
        val statusLabel = when (dev.status) {
            WifiP2pDevice.CONNECTED -> "Connected"
            WifiP2pDevice.INVITED -> "Connecting..."
            WifiP2pDevice.AVAILABLE -> "Available"
            else -> "Nearby"
        }
        holder.name.text = dev.deviceName.ifBlank { "Wi-Fi Direct Device" }
        holder.detail.text = "${dev.deviceAddress} • $statusLabel"
        holder.card.isSelectedCard = (dev.deviceAddress == selectedAddress)
        holder.card.onCardSelectedWithOrigin = { _, _ ->
            selectedAddress = dev.deviceAddress
            notifyDataSetChanged()
            onDeviceSelected(dev)
        }
    }
}
