package com.directdrop.app

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.net.wifi.p2p.WifiP2pConfig
import android.net.wifi.p2p.WifiP2pDevice
import android.os.Build
import android.os.Bundle
import android.os.Environment
import android.provider.Settings
import android.view.View
import android.widget.TextView
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.lifecycle.lifecycleScope
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.button.MaterialButton
import com.google.android.material.button.MaterialButtonToggleGroup
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File

class MainActivity : AppCompatActivity() {

    private lateinit var homePage: View
    private lateinit var selectPage: View
    private lateinit var scanPage: View
    private lateinit var transferScreen: View
    private lateinit var learnMorePage: View

    private lateinit var statusText: TextView
    private lateinit var sasText: TextView
    private lateinit var selectionText: TextView
    private lateinit var browserPathText: TextView
    private lateinit var emptyBrowserText: TextView
    private lateinit var recentHistoryText: TextView
    private lateinit var transferScreenSummary: TextView
    private lateinit var transferFileListText: TextView
    private lateinit var transferScreenProgress: LiquidDropProgressView
    private lateinit var radarScanView: RadarScanView
    private lateinit var btnFolderUp: MaterialButton

    private lateinit var p2pController: WifiP2pController
    private lateinit var browserAdapter: BrowserAdapter
    private lateinit var deviceAdapter: DeviceAdapter
    private var selectedDevice: WifiP2pDevice? = null

    private var discoveredPeers: List<WifiP2pDevice> = emptyList()
    private var currentEntries: List<BrowserEntry> = emptyList()
    private var sortMode: Int = 0 // 0=Name, 1=Size, 2=Type
    private var activeCategoryId: String = "files"
    private val categoryScrollStates = mutableMapOf<String, android.os.Parcelable?>()
    private lateinit var browserRecyclerView: RecyclerView
    private val selectedMap = linkedMapOf<String, SelectedFile>()
    private val selectedIds = mutableSetOf<String>()
    private var currentDir: File = Environment.getExternalStorageDirectory()
    private var connectedHostIp: String? = null

    private val permLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { results ->
        val denied = results.filterValues { !it }.keys
        if (denied.isNotEmpty()) {
            statusText.text = "Permission required: ${denied.joinToString()}"
        }
    }

    private fun updateSelectionCount() {
        selectionText.text = selectedMap.size.toString()
    }

    private fun showPage(target: View) {
        listOf(homePage, selectPage, scanPage, transferScreen, learnMorePage).forEach { page ->
            if (page == target) {
                page.alpha = 0f
                page.visibility = View.VISIBLE
                page.animate().alpha(1f).setDuration(220).start()
            } else {
                page.visibility = View.GONE
            }
        }
    }

    private fun applySortAndShow(list: List<BrowserEntry>) {
        currentEntries = list
        val sorted = when (sortMode) {
            1 -> list.sortedWith(compareBy<BrowserEntry>({ !it.isDirectory }, { -(it.selectedFile?.size ?: 0L) }))
            2 -> list.sortedWith(compareBy<BrowserEntry>({ !it.isDirectory }, { it.title.substringAfterLast('.', "").lowercase() }, { it.title.lowercase() }))
            else -> list.sortedWith(compareBy<BrowserEntry>({ !it.isDirectory }, { it.title.lowercase() }))
        }
        val query = findViewById<android.widget.EditText>(R.id.searchInput).text?.toString()?.trim().orEmpty()
        val filtered = if (query.isEmpty()) sorted else sorted.filter { it.title.contains(query, ignoreCase = true) }
        emptyBrowserText.visibility = if (filtered.isEmpty()) View.VISIBLE else View.GONE
        browserAdapter.submitList(filtered)
        categoryScrollStates[activeCategoryId]?.let { state ->
            browserRecyclerView.layoutManager?.onRestoreInstanceState(state)
        }
    }

    private fun loadDirectory(dir: File) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R && !Environment.isExternalStorageManager()) {
            val intent = Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION).apply {
                data = Uri.parse("package:$packageName")
            }
            startActivity(intent)
            return
        }
        currentDir = dir
        val root = Environment.getExternalStorageDirectory()
        val rel = dir.absolutePath.removePrefix(root.absolutePath).trimStart('/')
        browserPathText.text = if (rel.isEmpty()) "Internal Storage" else rel
        btnFolderUp.visibility = if (dir.absolutePath != root.absolutePath) View.VISIBLE else View.GONE
        lifecycleScope.launch(Dispatchers.IO) {
            val list = InAppFileManager.listDirectory(dir)
            withContext(Dispatchers.Main) { applySortAndShow(list) }
        }
    }

    private fun loadMediaCategory(title: String, loader: () -> List<SelectedFile>) {
        btnFolderUp.visibility = View.GONE
        browserPathText.text = title
        lifecycleScope.launch(Dispatchers.IO) {
            val entries = loader().map { sf ->
                BrowserEntry(
                    id = sf.uri.toString(),
                    title = sf.relativePath.substringAfterLast('/'),
                    subtitle = "%.2f MB".format(sf.size / (1024.0 * 1024.0)),
                    isDirectory = false,
                    selectedFile = sf
                )
            }
            withContext(Dispatchers.Main) { applySortAndShow(entries) }
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
        InAppFileManager.ensureHiddenIndexDir(this)

        homePage = findViewById(R.id.homePage)
        selectPage = findViewById(R.id.selectPage)
        scanPage = findViewById(R.id.scanPage)
        transferScreen = findViewById(R.id.transferScreen)
        learnMorePage = findViewById(R.id.learnMorePage)

        statusText = findViewById(R.id.statusText)
        sasText = findViewById(R.id.sasText)
        selectionText = findViewById(R.id.selectionText)
        browserPathText = findViewById(R.id.browserPathText)
        emptyBrowserText = findViewById(R.id.emptyBrowserText)
        recentHistoryText = findViewById(R.id.recentHistoryText)
        transferScreenSummary = findViewById(R.id.transferScreenSummary)
        transferFileListText = findViewById(R.id.transferFileListText)
        transferScreenProgress = findViewById(R.id.transferScreenProgress)
        radarScanView = findViewById(R.id.radarScanView)
        btnFolderUp = findViewById(R.id.btnFolderUp)

        val recycler: RecyclerView = findViewById(R.id.browserRecyclerView)
        browserRecyclerView = recycler
        recycler.layoutManager = LinearLayoutManager(this)
        browserAdapter = BrowserAdapter(
            context = this,
            scope = lifecycleScope,
            selectedIds = selectedIds,
            onFolderClick = { entry -> entry.file?.let { loadDirectory(it) } },
            onToggleItem = { entry, checked ->
                if (entry.isDirectory && entry.file != null) {
                    val files = InAppFileManager.collectDirectoryFiles(entry.file)
                    if (checked) {
                        files.forEach { selectedMap[it.relativePath] = it }
                    } else {
                        files.forEach { selectedMap.remove(it.relativePath) }
                    }
                } else if (entry.selectedFile != null) {
                    if (checked) selectedMap[entry.id] = entry.selectedFile
                    else selectedMap.remove(entry.id)
                }
                updateSelectionCount()
            }
        )
        recycler.adapter = browserAdapter

        val btnHomeSend: MaterialButton = findViewById(R.id.btnHomeSend)
        val btnHomeReceive: MaterialButton = findViewById(R.id.btnHomeReceive)
        val btnBackFromSelect: MaterialButton = findViewById(R.id.btnBackFromSelect)
        val btnProceedToScan: MaterialButton = findViewById(R.id.btnProceedToScan)
        val btnBackFromScan: MaterialButton = findViewById(R.id.btnBackFromScan)
        val btnBackToSetup: MaterialButton = findViewById(R.id.btnBackToSetup)

        val btnDiscover: MaterialButton = findViewById(R.id.btnDiscover)
        val btnConnect: MaterialButton = findViewById(R.id.btnConnect)
        val transportSelector: LiquidSegmentedSelectorView = findViewById(R.id.transportSelector)
        val bandSelector: LiquidBandSelectorView = findViewById(R.id.bandSelector)
        val bandStatusText: TextView = findViewById(R.id.bandStatusText)
        val bandWhyText: TextView = findViewById(R.id.bandWhyText)
        val expectedPerfText: TextView = findViewById(R.id.expectedPerfText)
        val compatComparisonText: TextView = findViewById(R.id.compatComparisonText)
        val lanIpInput: android.widget.EditText = findViewById(R.id.lanIpInput)
        val btnVerifyLan: MaterialButton = findViewById(R.id.btnVerifyLan)
        val btnConnectLan: MaterialButton = findViewById(R.id.btnConnectLan)
        val btnLearnMore: MaterialButton = findViewById(R.id.btnLearnMore)
        val btnBackFromLearnMore: MaterialButton = findViewById(R.id.btnBackFromLearnMore)
        val learnMoreBodyText: TextView = findViewById(R.id.learnMoreBodyText)

        var activeTransport = ActiveTransport.WIFI_DIRECT
        var lanReachability = ReachabilityState.NOT_TESTED
        var currentPeerCaps = PeerCapabilities()

        transportSelector.setSegments(
            listOf(
                LiquidSegmentedSelectorView.Segment("p2p", "Direct (P2P)"),
                LiquidSegmentedSelectorView.Segment("lan", "Through Router")
            ),
            initialIndex = 0
        )

        fun refreshBandTelemetry(band: LiquidBandSelectorView.Band) {
            val cap = BandCapabilityManager.probe(this)
            val report = BandCapabilityManager.compareAndRecommend(cap, currentPeerCaps, activeTransport, band, lanReachability)
            bandStatusText.text = "${report.recommendationHeadline} • Active: ${report.activeBandLabel}"
            bandWhyText.text = "Why this option? ${report.recommendationReason}"
            expectedPerfText.text = "Expected performance: ${report.expectedPerformance}"
            compatComparisonText.text = report.rows.joinToString("\n") {
                "${it.label.padEnd(13)} | You: ${it.localVal.padEnd(11)} | Peer: ${it.peerVal}"
            }
            learnMoreBodyText.text = BandCapabilityManager.buildLearnMoreSections(cap, report)
                .joinToString("\n\n") { (title, body) -> "$title\n$body" }
        }

        transportSelector.onSegmentSelected = { _, seg ->
            activeTransport = if (seg.id == "p2p") ActiveTransport.WIFI_DIRECT else ActiveTransport.ROUTER_LAN
            refreshBandTelemetry(bandSelector.selectedBand)
        }

        btnLearnMore.setOnClickListener {
            refreshBandTelemetry(bandSelector.selectedBand)
            showPage(learnMorePage)
        }
        btnBackFromLearnMore.setOnClickListener { showPage(scanPage) }

        val savedBand = BandCapabilityManager.loadSavedBand(this)
        bandSelector.setBand(savedBand, notify = false)
        refreshBandTelemetry(savedBand)
        bandSelector.onBandChanged = { band ->
            BandCapabilityManager.saveBand(this, band)
            refreshBandTelemetry(band)
        }

        btnVerifyLan.setOnClickListener {
            val ip = lanIpInput.text?.toString()?.trim().orEmpty()
            if (ip.isEmpty()) {
                statusText.text = "Enter peer LAN IP first to verify reachability."
                return@setOnClickListener
            }
            statusText.text = "Checking LAN reachability to $ip:47291..."
            lifecycleScope.launch(Dispatchers.IO) {
                val res = BandCapabilityManager.probeTcpReachability(ip, 47291)
                withContext(Dispatchers.Main) {
                    lanReachability = res
                    statusText.text = "LAN check ($ip:47291): ${res.label}"
                    refreshBandTelemetry(bandSelector.selectedBand)
                }
            }
        }

        btnConnectLan.setOnClickListener {
            val ip = lanIpInput.text?.toString()?.trim().orEmpty()
            if (ip.isEmpty()) {
                statusText.text = "Enter desktop LAN IP (e.g. 192.168.1.42) for TLS 1.3 fallback."
                return@setOnClickListener
            }
            connectedHostIp = ip
            activeTransport = ActiveTransport.ROUTER_LAN
            transportSelector.setSelectedIndex(1, notify = false)
            statusText.text = "Router/LAN target: $ip:47291 (TLS 1.3 + SAS)"
            lifecycleScope.launch(Dispatchers.IO) {
                val res = BandCapabilityManager.probeTcpReachability(ip, 47291)
                withContext(Dispatchers.Main) {
                    lanReachability = res
                    refreshBandTelemetry(bandSelector.selectedBand)
                }
            }
        }

        val deviceRecycler: RecyclerView = findViewById(R.id.deviceRecyclerView)
        deviceRecycler.layoutManager = LinearLayoutManager(this)
        deviceAdapter = DeviceAdapter { dev ->
            selectedDevice = dev
            activeTransport = ActiveTransport.WIFI_DIRECT
            if (bandSelector.selectedBand == LiquidBandSelectorView.Band.GHZ_6) {
                refreshBandTelemetry(bandSelector.selectedBand)
                statusText.text = "6 GHz unavailable for Wi-Fi Direct; choose Auto, 5 GHz, or Use LAN."
                return@DeviceAdapter
            }
            val band = when (bandSelector.selectedBand) {
                LiquidBandSelectorView.Band.GHZ_24 -> WifiP2pConfig.GROUP_OWNER_BAND_2GHZ
                LiquidBandSelectorView.Band.GHZ_5 -> WifiP2pConfig.GROUP_OWNER_BAND_5GHZ
                else -> WifiP2pConfig.GROUP_OWNER_BAND_AUTO
            }
            p2pController.connectToPeer(dev, band)
        }
        deviceRecycler.adapter = deviceAdapter
        val categorySelector: LiquidSegmentedSelectorView = findViewById(R.id.categorySelector)
        val btnSort: MaterialButton = findViewById(R.id.btnSort)
        val btnSelectAll: MaterialButton = findViewById(R.id.btnSelectAll)
        val btnViewMode: MaterialButton = findViewById(R.id.btnViewMode)
        val searchInput: android.widget.EditText = findViewById(R.id.searchInput)
        val homeTabSelector: LiquidSegmentedSelectorView = findViewById(R.id.homeTabSelector)
        val shareActionsContainer: View = findViewById(R.id.shareActionsContainer)
        val btnTransfer: MaterialButton = findViewById(R.id.btnTransfer)

        homeTabSelector.setSegments(
            listOf(
                LiquidSegmentedSelectorView.Segment("share", "Share"),
                LiquidSegmentedSelectorView.Segment("history", "History")
            ),
            initialIndex = 0
        )
        homeTabSelector.onSegmentSelected = { _, seg ->
            shareActionsContainer.visibility = if (seg.id == "share") View.VISIBLE else View.GONE
        }

        categorySelector.setSegments(
            listOf(
                LiquidSegmentedSelectorView.Segment("files", "Files"),
                LiquidSegmentedSelectorView.Segment("images", "Images"),
                LiquidSegmentedSelectorView.Segment("videos", "Videos"),
                LiquidSegmentedSelectorView.Segment("audio", "Audio"),
                LiquidSegmentedSelectorView.Segment("apps", "Apps")
            ),
            initialIndex = 0
        )

        btnViewMode.setOnClickListener {
            browserAdapter.isGridMode = !browserAdapter.isGridMode
            if (browserAdapter.isGridMode) {
                btnViewMode.text = "List"
                recycler.layoutManager = androidx.recyclerview.widget.GridLayoutManager(this, 3)
            } else {
                btnViewMode.text = "Grid"
                recycler.layoutManager = LinearLayoutManager(this)
            }
            recycler.adapter = browserAdapter
        }

        searchInput.addTextChangedListener(object : android.text.TextWatcher {
            override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) {}
            override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) {
                applySortAndShow(currentEntries)
            }
            override fun afterTextChanged(s: android.text.Editable?) {}
        })

        btnSelectAll.setOnClickListener {
            val nonDirs = currentEntries.filter { !it.isDirectory && it.selectedFile != null }
            val allSelected = nonDirs.isNotEmpty() && nonDirs.all { selectedIds.contains(it.id) }
            if (allSelected) {
                nonDirs.forEach {
                    selectedIds.remove(it.id)
                    selectedMap.remove(it.id)
                }
            } else {
                nonDirs.forEach {
                    selectedIds.add(it.id)
                    selectedMap[it.id] = it.selectedFile!!
                }
            }
            updateSelectionCount()
            applySortAndShow(currentEntries)
        }

        btnSort.setOnClickListener {
            sortMode = (sortMode + 1) % 3
            btnSort.text = when (sortMode) {
                1 -> "Sort: Size"
                2 -> "Sort: Type"
                else -> "Sort: Name"
            }
            applySortAndShow(currentEntries)
        }

        btnHomeSend.setOnClickListener {
            showPage(selectPage)
            categorySelector.setSelectedIndex(0, notify = false)
            activeCategoryId = "files"
            loadDirectory(currentDir)
        }
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
        btnFolderUp.setOnClickListener {
            currentDir.parentFile?.let { loadDirectory(it) }
        }
        btnProceedToScan.setOnClickListener {
            if (selectedMap.isEmpty()) {
                browserPathText.text = "Select at least 1 item first"
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

        var categoryLoadSeq = 0L
        categorySelector.onSegmentSelected = { _, seg ->
            categoryScrollStates[activeCategoryId] = browserRecyclerView.layoutManager?.onSaveInstanceState()
            activeCategoryId = seg.id
            val seq = ++categoryLoadSeq
            emptyBrowserText.visibility = View.GONE
            browserAdapter.submitList(emptyList())
            when (seg.id) {
                "files" -> loadDirectory(currentDir)
                "images" -> {
                    btnFolderUp.visibility = View.GONE
                    browserPathText.text = "Images"
                    lifecycleScope.launch(Dispatchers.IO) {
                        val entries = MediaBrowser.queryImages(this@MainActivity, 200).map { sf ->
                            BrowserEntry(
                                id = sf.uri.toString(),
                                title = sf.relativePath.substringAfterLast('/'),
                                subtitle = "%.2f MB".format(sf.size / (1024.0 * 1024.0)),
                                isDirectory = false,
                                selectedFile = sf
                            )
                        }
                        withContext(Dispatchers.Main) { if (seq == categoryLoadSeq) applySortAndShow(entries) }
                    }
                }
                "videos" -> {
                    btnFolderUp.visibility = View.GONE
                    browserPathText.text = "Videos"
                    lifecycleScope.launch(Dispatchers.IO) {
                        val entries = MediaBrowser.queryVideos(this@MainActivity, 200).map { sf ->
                            BrowserEntry(
                                id = sf.uri.toString(),
                                title = sf.relativePath.substringAfterLast('/'),
                                subtitle = "%.2f MB".format(sf.size / (1024.0 * 1024.0)),
                                isDirectory = false,
                                selectedFile = sf
                            )
                        }
                        withContext(Dispatchers.Main) { if (seq == categoryLoadSeq) applySortAndShow(entries) }
                    }
                }
                "audio" -> {
                    btnFolderUp.visibility = View.GONE
                    browserPathText.text = "Audio"
                    lifecycleScope.launch(Dispatchers.IO) {
                        val entries = MediaBrowser.queryAudio(this@MainActivity, 200).map { sf ->
                            BrowserEntry(
                                id = sf.uri.toString(),
                                title = sf.relativePath.substringAfterLast('/'),
                                subtitle = "%.2f MB".format(sf.size / (1024.0 * 1024.0)),
                                isDirectory = false,
                                selectedFile = sf
                            )
                        }
                        withContext(Dispatchers.Main) { if (seq == categoryLoadSeq) applySortAndShow(entries) }
                    }
                }
                "apps" -> {
                    btnFolderUp.visibility = View.GONE
                    browserPathText.text = "Installed Apps"
                    lifecycleScope.launch(Dispatchers.IO) {
                        val apps = InAppFileManager.listInstalledAppsWithIcons(this@MainActivity)
                        withContext(Dispatchers.Main) { if (seq == categoryLoadSeq) applySortAndShow(apps) }
                    }
                }
            }
        }

        p2pController = WifiP2pController(
            this,
            onPeersChanged = { peers ->
                discoveredPeers = peers
                if (peers.isNotEmpty()) radarScanView.setScanning(false)
                deviceAdapter.submitList(peers)
                statusText.text = if (peers.isEmpty()) "No devices found yet." else "Found ${peers.size} device(s) — tap to connect"
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
            if (bandSelector.selectedBand == LiquidBandSelectorView.Band.GHZ_6) {
                statusText.text = "6 GHz unavailable for Wi-Fi Direct; choose Auto, 5 GHz, or Use LAN."
                return@setOnClickListener
            }
            val band = when (bandSelector.selectedBand) {
                LiquidBandSelectorView.Band.GHZ_24 -> WifiP2pConfig.GROUP_OWNER_BAND_2GHZ
                LiquidBandSelectorView.Band.GHZ_5 -> WifiP2pConfig.GROUP_OWNER_BAND_5GHZ
                else -> WifiP2pConfig.GROUP_OWNER_BAND_AUTO
            }
            val target = selectedDevice ?: discoveredPeers.firstOrNull()
            target?.let { p2pController.connectToPeer(it, band) }
                ?: run { statusText.text = "No peers discovered yet. Use LAN IP fallback if needed." }
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
                val items = selectedMap.values.toList()
                transferFileListText.text = items.mapIndexed { idx, f ->
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
            val files = selectedMap.values.toList()
            if (files.isEmpty()) {
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
                files = files
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
            arrayOf(Manifest.permission.ACCESS_FINE_LOCATION, Manifest.permission.READ_EXTERNAL_STORAGE)
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
