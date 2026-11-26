package com.directdrop.app

import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.net.wifi.WifiManager
import android.os.Build
import android.os.IBinder
import android.os.PowerManager
import androidx.core.app.NotificationCompat
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch

data class TransferUiState(
    val running: Boolean = false,
    val verified: Int = 0,
    val total: Int = 0,
    val currentFile: String = "Idle",
    val sasCode: String? = null,
    val error: String? = null
)

class TransferService : Service() {

    companion object {
        private const val CHANNEL_ID = "directdrop_transfer"
        private const val NOTIF_ID = 1001

        val state = MutableStateFlow(TransferUiState())
        var pendingFiles: List<SelectedFile> = emptyList()

        fun startTransfer(context: Context, host: String, deviceId: String, deviceName: String, files: List<SelectedFile>) {
            pendingFiles = files
            val intent = Intent(context, TransferService::class.java).apply {
                putExtra("host", host)
                putExtra("deviceId", deviceId)
                putExtra("deviceName", deviceName)
            }
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                context.startForegroundService(intent)
            } else {
                context.startService(intent)
            }
        }
    }

    private val serviceScope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
    private var transferJob: Job? = null
    private var wakeLock: PowerManager.WakeLock? = null
    private var wifiLock: WifiManager.WifiLock? = null

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onCreate() {
        super.onCreate()
        createChannel()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        val host = intent?.getStringExtra("host") ?: return START_NOT_STICKY
        val deviceId = intent.getStringExtra("deviceId") ?: "android"
        val deviceName = intent.getStringExtra("deviceName") ?: "Android"
        val files = pendingFiles

        val notif = buildNotification("Starting transfer (${files.size} items)...", 0, 100)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            startForeground(NOTIF_ID, notif, ServiceInfo.FOREGROUND_SERVICE_TYPE_DATA_SYNC)
        } else {
            startForeground(NOTIF_ID, notif)
        }

        acquireLocks()
        transferJob?.cancel()
        transferJob = serviceScope.launch {
            state.value = TransferUiState(running = true, total = files.size, currentFile = "Connecting...")
            try {
                val client = ProtocolClient(applicationContext)
                client.transferFiles(
                    host = host,
                    deviceId = deviceId,
                    deviceName = deviceName,
                    files = files,
                    onSasCode = { sas ->
                        state.value = state.value.copy(sasCode = sas)
                    },
                    onProgress = { verified, total, current ->
                        state.value = state.value.copy(verified = verified, total = total, currentFile = current)
                        val pct = if (total > 0) (verified * 100) / total else 0
                        val nm = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
                        nm.notify(NOTIF_ID, buildNotification("Sending $verified/$total: $current", pct, 100))
                    }
                )
                state.value = state.value.copy(running = false, currentFile = "Completed")
            } catch (e: Exception) {
                state.value = state.value.copy(running = false, error = e.message ?: "Transfer failed")
            } finally {
                releaseLocks()
                stopForeground(STOP_FOREGROUND_DETACH)
                stopSelf()
            }
        }

        return START_NOT_STICKY
    }

    private fun acquireLocks() {
        val pm = getSystemService(Context.POWER_SERVICE) as PowerManager
        wakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "DirectDrop::TransferWakeLock").apply {
            acquire(60 * 60 * 1000L)
        }
        val wm = applicationContext.getSystemService(Context.WIFI_SERVICE) as WifiManager
        @Suppress("DEPRECATION")
        wifiLock = wm.createWifiLock(WifiManager.WIFI_MODE_FULL_HIGH_PERF, "DirectDrop::WifiLock").apply {
            acquire()
        }
    }

    private fun releaseLocks() {
        try { wakeLock?.let { if (it.isHeld) it.release() } } catch (_: Exception) {}
        try { wifiLock?.let { if (it.isHeld) it.release() } } catch (_: Exception) {}
    }

    private fun createChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val ch = NotificationChannel(CHANNEL_ID, "DirectDrop Transfers", NotificationManager.IMPORTANCE_LOW)
            val nm = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
            nm.createNotificationChannel(ch)
        }
    }

    private fun buildNotification(text: String, progress: Int, max: Int) =
        NotificationCompat.Builder(this, CHANNEL_ID)
            .setSmallIcon(R.drawable.ic_launcher)
            .setContentTitle("DirectDrop")
            .setContentText(text)
            .setOnlyAlertOnce(true)
            .setOngoing(true)
            .setProgress(max, progress, false)
            .build()

    override fun onDestroy() {
        releaseLocks()
        serviceScope.cancel()
        super.onDestroy()
    }
}
