package com.directdrop.app

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.net.wifi.p2p.WifiP2pConfig
import android.net.wifi.p2p.WifiP2pDevice
import android.os.Build
import android.os.Bundle
import android.provider.Settings
import android.view.View
import android.widget.TextView
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.documentfile.provider.DocumentFile
import androidx.lifecycle.lifecycleScope
import com.google.android.material.button.MaterialButton
import com.google.android.material.button.MaterialButtonToggleGroup
import kotlinx.coroutines.launch

class MainActivity : AppCompatActivity() {

    private lateinit var homePage: View
    private lateinit var selectPage: View
    private lateinit var scanPage: View
    private lateinit var transferScreen: View

    private lateinit var statusText: TextView
    private lateinit var sasText: TextView
    private lateinit var peersText: TextView
    private lateinit var selectionText: TextView
    private lateinit var recentHistoryText: TextView
    private lateinit var transferScreenSummary: TextView
    private lateinit var transferFileListText: TextView
    private lateinit var transferScreenProgress: LiquidDropProgressView
    private lateinit var radarScanView: RadarScanView

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
            selectedFiles = uris.mapNotNull { uri ->
                val doc = DocumentFile.fromSingleUri(this, uri)
                val name = doc?.name ?: "file_${System.currentTimeMillis()}"
                SelectedFile(uri, name, doc?.length() ?: 0L)
            }
            updateSelectionSummary()
        }
    }

    private val folderPicker = registerForActivityResult(
        ActivityResultContracts.OpenDocumentTree()
    ) { uri ->
        if (uri != null) {
            contentResolver.takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION)
            selectedFiles = protocolClient.collectFilesFromTree(uri)
            updateSelectionSummary()
        }
    }

    private fun updateSelectionSummary() {
        val totalMb = selectedFiles.sumOf { it.size } / (1024.0 * 1024.0)
        val preview = selectedFiles.take(8).joinToString("\n") {
            "• ${it.relativePath} (%.2f MB)".format(it.size / (1024.0 * 1024.0))
        }
        val extra = if (selectedFiles.size > 8) "\n...and ${selectedFiles.size - 8} more" else ""
        selectionText.text = "Selected ${selectedFiles.size} item(s) — %.2f MB\n\n$preview$extra".format(totalMb)
    }

    private fun showPage(target: View) {
        listOf(homePage, selectPage, scanPage, transferScreen).forEach { page ->
            if (page == target) {
                page.alpha = 0f
                page.visibility = View.VISIBLE
                page.animate().alpha(1f).setDuration(220).start()
            } else {
                page.visibility = View.GONE
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        enableEdgeToEdge()
        super.onCreate(savedInstanceState)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            display?.supportedModes?.maxByOrNull { it.refreshRate }?.let { mode ->
                window.attributes = window.attributes.apply { preferredDisplayModeId = mode.modeId }
            }
        }
        setContentView(R.layout.activity_main)

        protocolClient = ProtocolClient(this)
        homePage = findViewById(R.id.homePage)
        selectPage = findViewById(R.id.selectPage)
        scanPage = findViewById(R.id.scanPage)
        transferScreen = findViewById(R.id.transferScreen)

        statusText = findViewById(R.id.statusText)
        sasText = findViewById(R.id.sasText)
        peersText = findViewById(R.id.peersText)
        selectionText = findViewById(R.id.selectionText)
        recentHistoryText = findViewById(R.id.recentHistoryText)
        transferScreenSummary = findViewById(R.id.transferScreenSummary)
        transferFileListText = findViewById(R.id.transferFileListText)
        transferScreenProgress = findViewById(R.id.transferScreenProgress)
        radarScanView = findViewById(R.id.radarScanView)

        val btnHomeSend: MaterialButton = findViewById(R.id.btnHomeSend)
        val btnHomeReceive: MaterialButton = findViewById(R.id.btnHomeReceive)
        val btnBackFromSelect: MaterialButton = findViewById(R.id.btnBackFromSelect)
        val btnProceedToScan: MaterialButton = findViewById(R.id.btnProceedToScan)
        val btnBackFromScan: MaterialButton = findViewById(R.id.btnBackFromScan)
        val btnBackToSetup: MaterialButton = findViewById(R.id.btnBackToSetup)

        val btnDiscover: MaterialButton = findViewById(R.id.btnDiscover)
        val btnConnect: MaterialButton = findViewById(R.id.btnConnect)
        val bandToggleGroup: MaterialButtonToggleGroup = findViewById(R.id.bandToggleGroup)
        val btnSelectFiles: MaterialButton = findViewById(R.id.btnSelectFiles)
        val btnSelectFolder: MaterialButton = findViewById(R.id.btnSelectFolder)
        val btnPhotos: MaterialButton = findViewById(R.id.btnPhotos)
        val btnVideos: MaterialButton = findViewById(R.id.btnVideos)
        val btnAudio: MaterialButton = findViewById(R.id.btnAudio)
        val btnApps: MaterialButton = findViewById(R.id.btnApps)
        val btnTransfer: MaterialButton = findViewById(R.id.btnTransfer)

        btnHomeSend.setOnClickListener { showPage(selectPage) }
        btnHomeReceive.setOnClickListener {
            showPage(scanPage)
            if (hasRequiredPermissions()) {
                radarScanView.setScanning(true)
                p2pController.discoverPeers()
            } else {
                requestRuntimePermissions()
            }
        }
        btnBackFromSelect.setOnClickListener { showPage(homePage) }
        btnProceedToScan.setOnClickListener {
            if (selectedFiles.isEmpty()) {
                selectionText.text = "Please select at least one file, folder, photo, video, audio, or app first."
                return@setOnClickListener
            }
            showPage(scanPage)
            if (hasRequiredPermissions()) {
                radarScanView.setScanning(true)
                p2pController.discoverPeers()
            }
        }
        btnBackFromScan.setOnClickListener { showPage(selectPage) }
        btnBackToSetup.setOnClickListener { showPage(homePage) }

        p2pController = WifiP2pController(
            this,
            onPeersChanged = { peers ->
                discoveredPeers = peers
                if (peers.isNotEmpty()) radarScanView.setScanning(false)
                peersText.text = if (peers.isEmpty()) {
                    "No peers found yet."
                } else {
                    peers.joinToString("\n") { "• ${it.deviceName} (${it.deviceAddress})" }
                }
            },
            onGroupConnected = { goIp, isGo ->
                radarScanView.setScanning(false)
                connectedHostIp = if (!isGo) goIp else null
                statusText.text = "Wi-Fi Direct Connected (GO=$goIp)"
            },
            onStatus = { msg -> statusText.text = msg }
        )

        requestRuntimePermissions()

        btnDiscover.setOnClickListener {
            if (hasRequiredPermissions()) {
                radarScanView.setScanning(true)
                p2pController.discoverPeers()
            } else {
                requestRuntimePermissions()
            }
        }
        btnConnect.setOnClickListener {
            val band = when (bandToggleGroup.checkedButtonId) {
                R.id.btnBand24 -> WifiP2pConfig.GROUP_OWNER_BAND_2GHZ
                R.id.btnBand5 -> WifiP2pConfig.GROUP_OWNER_BAND_5GHZ
                else -> WifiP2pConfig.GROUP_OWNER_BAND_AUTO
            }
            discoveredPeers.firstOrNull()?.let { p2pController.connectToPeer(it, band) }
                ?: run { statusText.text = "No peers discovered yet." }
        }

        btnSelectFiles.setOnClickListener { filePicker.launch(arrayOf("*/*")) }
        btnSelectFolder.setOnClickListener { folderPicker.launch(null) }
        btnPhotos.setOnClickListener { filePicker.launch(arrayOf("image/*")) }
        btnVideos.setOnClickListener { filePicker.launch(arrayOf("video/*")) }
        btnAudio.setOnClickListener { filePicker.launch(arrayOf("audio/*")) }
        btnApps.setOnClickListener {
            val allApps = MediaBrowser.listInstalledUserApps(this, limit = 200)
            if (allApps.isEmpty()) {
                selectionText.text = "No user apps found."
                return@setOnClickListener
            }
            val names = allApps.map { it.relativePath.removePrefix("Apps/").removeSuffix(".apk") }.toTypedArray()
            val checked = BooleanArray(allApps.size)
            AlertDialog.Builder(this)
                .setTitle("Select Apps to Extract & Send")
                .setMultiChoiceItems(names, checked) { _, which, isChecked ->
                    checked[which] = isChecked
                }
                .setPositiveButton("Select") { _, _ ->
                    selectedFiles = allApps.filterIndexed { idx, _ -> checked[idx] }
                    updateSelectionSummary()
                }
                .setNegativeButton("Cancel", null)
                .show()
        }

        lifecycleScope.launch {
            TransferService.state.collect { st ->
                if (st.sasCode != null) {
                    sasText.visibility = View.VISIBLE
                    sasText.text = "SAS Code: ${st.sasCode}"
                }
                val pct = if (st.total > 0) (st.verified * 100) / st.total else 0
                transferScreenProgress.progress = pct
                val summary = if (st.error != null) {
                    "Error: ${st.error}"
                } else {
                    "Verified ${st.verified}/${st.total} — ${st.currentFile}"
                }
                transferScreenSummary.text = summary
                if (st.total > 0) {
                    recentHistoryText.text = "Last job: ${st.verified}/${st.total} verified (${st.currentFile})"
                }
                transferFileListText.text = selectedFiles.mapIndexed { idx, f ->
                    val mb = f.size / (1024.0 * 1024.0)
                    val mark = if (idx < st.verified) "✓" else if (idx == st.verified && st.running) "⏳" else "•"
                    "$mark ${f.relativePath} (%.2f MB)".format(mb)
                }.joinToString("\n")
            }
        }

        btnTransfer.setOnClickListener {
            val host = connectedHostIp ?: run {
                statusText.text = "Connect to laptop via Wi-Fi Direct first."
                return@setOnClickListener
            }
            if (selectedFiles.isEmpty()) {
                statusText.text = "Select items first."
                return@setOnClickListener
            }
            showPage(transferScreen)
            val devId = Settings.Secure.getString(contentResolver, Settings.Secure.ANDROID_ID) ?: "android"
            TransferService.startTransfer(
                this,
                host = host,
                deviceId = devId,
                deviceName = "${Build.MANUFACTURER} ${Build.MODEL}",
                files = selectedFiles
            )
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
            arrayOf(
                Manifest.permission.NEARBY_WIFI_DEVICES,
                Manifest.permission.POST_NOTIFICATIONS,
                Manifest.permission.READ_MEDIA_IMAGES,
                Manifest.permission.READ_MEDIA_VIDEO,
                Manifest.permission.READ_MEDIA_AUDIO
            )
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
