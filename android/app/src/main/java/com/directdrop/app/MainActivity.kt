package com.directdrop.app

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.net.wifi.p2p.WifiP2pDevice
import android.os.Build
import android.os.Bundle
import android.provider.Settings
import android.view.View
import android.widget.TextView
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.lifecycle.lifecycleScope
import com.google.android.material.button.MaterialButton
import com.google.android.material.progressindicator.LinearProgressIndicator
import kotlinx.coroutines.launch

class MainActivity : AppCompatActivity() {

    private lateinit var statusText: TextView
    private lateinit var sasText: TextView
    private lateinit var peersText: TextView
    private lateinit var selectionText: TextView
    private lateinit var transferDetailText: TextView
    private lateinit var transferProgress: LinearProgressIndicator

    private lateinit var p2pController: WifiP2pController
    private lateinit var protocolClient: ProtocolClient

    private var discoveredPeers: List<WifiP2pDevice> = emptyList()
    private var selectedFiles: List<SelectedFile> = emptyList()
    private var connectedHostIp: String? = null

    private val permLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { results ->
        val denied = results.filterValues { !it }.keys
        if (denied.isNotEmpty()) {
            statusText.text = "Permission required: ${denied.joinToString()}"
        }
    }

    private val filePicker = registerForActivityResult(
        ActivityResultContracts.OpenMultipleDocuments()
    ) { uris ->
        if (uris.isNotEmpty()) {
            val picked = uris.mapNotNull { uri ->
                val doc = androidx.documentfile.provider.DocumentFile.fromSingleUri(this, uri)
                val name = doc?.name ?: "file_${System.currentTimeMillis()}"
                SelectedFile(uri, name, doc?.length() ?: 0L)
            }
            selectedFiles = picked
            val totalMb = selectedFiles.sumOf { it.size } / (1024.0 * 1024.0)
            selectionText.text = "Selected ${selectedFiles.size} file(s) (%.2f MB)".format(totalMb)
        }
    }

    private val folderPicker = registerForActivityResult(
        ActivityResultContracts.OpenDocumentTree()
    ) { uri ->
        if (uri != null) {
            contentResolver.takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION)
            selectedFiles = protocolClient.collectFilesFromTree(uri)
            val totalMb = selectedFiles.sumOf { it.size } / (1024.0 * 1024.0)
            selectionText.text = "Selected ${selectedFiles.size} files (%.2f MB)".format(totalMb)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        enableEdgeToEdge()
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        protocolClient = ProtocolClient(this)
        statusText = findViewById(R.id.statusText)
        sasText = findViewById(R.id.sasText)
        peersText = findViewById(R.id.peersText)
        selectionText = findViewById(R.id.selectionText)
        transferDetailText = findViewById(R.id.transferDetailText)
        transferProgress = findViewById(R.id.transferProgress)

        val btnDiscover: MaterialButton = findViewById(R.id.btnDiscover)
        val btnConnect: MaterialButton = findViewById(R.id.btnConnect)
        val btnSelectFiles: MaterialButton = findViewById(R.id.btnSelectFiles)
        val btnSelectFolder: MaterialButton = findViewById(R.id.btnSelectFolder)
        val btnTransfer: MaterialButton = findViewById(R.id.btnTransfer)

        p2pController = WifiP2pController(
            this,
            onPeersChanged = { peers ->
                discoveredPeers = peers
                peersText.text = if (peers.isEmpty()) {
                    "No peers found yet."
                } else {
                    peers.joinToString("\n") { "• ${it.deviceName} (${it.deviceAddress})" }
                }
            },
            onGroupConnected = { goIp, isGo ->
                connectedHostIp = if (!isGo) goIp else null
                statusText.text = "Wi-Fi Direct Connected (GO=$goIp)"
            },
            onStatus = { msg -> statusText.text = msg }
        )

        requestRuntimePermissions()

        btnDiscover.setOnClickListener {
            if (hasRequiredPermissions()) {
                p2pController.discoverPeers()
            } else {
                requestRuntimePermissions()
            }
        }
        btnConnect.setOnClickListener {
            discoveredPeers.firstOrNull()?.let { p2pController.connectToPeer(it) }
                ?: run { statusText.text = "No peers discovered yet." }
        }
        btnSelectFiles.setOnClickListener { filePicker.launch(arrayOf("*/*")) }
        btnSelectFolder.setOnClickListener { folderPicker.launch(null) }
        btnTransfer.setOnClickListener {
            val host = connectedHostIp ?: run {
                statusText.text = "Connect to laptop via Wi-Fi Direct first."
                return@setOnClickListener
            }
            if (selectedFiles.isEmpty()) {
                statusText.text = "Select a folder first."
                return@setOnClickListener
            }
            val devId = Settings.Secure.getString(contentResolver, Settings.Secure.ANDROID_ID) ?: "android"
            lifecycleScope.launch {
                try {
                    protocolClient.transferFiles(
                        host = host,
                        deviceId = devId,
                        deviceName = "${Build.MANUFACTURER} ${Build.MODEL}",
                        files = selectedFiles,
                        onSasCode = { sas ->
                            runOnUiThread {
                                sasText.visibility = View.VISIBLE
                                sasText.text = "SAS Code: $sas"
                            }
                        },
                        onProgress = { v, t, f ->
                            runOnUiThread {
                                transferProgress.progress = if (t > 0) (v * 100) / t else 0
                                transferDetailText.text = "Verified $v/$t — $f"
                            }
                        }
                    )
                } catch (e: Exception) {
                    statusText.text = "Transfer error: ${e.message}"
                }
            }
        }
    }

    override fun onResume() {
        super.onResume()
        p2pController.register()
    }

    override fun onPause() {
        super.onPause()
        p2pController.unregister()
    }

    private fun requiredPermissions(): Array<String> {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            arrayOf(Manifest.permission.NEARBY_WIFI_DEVICES, Manifest.permission.POST_NOTIFICATIONS)
        } else {
            arrayOf(Manifest.permission.ACCESS_FINE_LOCATION)
        }
    }

    private fun hasRequiredPermissions(): Boolean {
        return requiredPermissions().all {
            ContextCompat.checkSelfPermission(this, it) == PackageManager.PERMISSION_GRANTED
        }
    }

    private fun requestRuntimePermissions() {
        permLauncher.launch(requiredPermissions())
    }
}
