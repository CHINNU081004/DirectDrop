package com.directdrop.app

import android.content.Context
import android.content.pm.ApplicationInfo
import android.graphics.drawable.Drawable
import android.net.Uri
import android.os.Environment
import java.io.File

data class BrowserEntry(
    val id: String,
    val title: String,
    val subtitle: String,
    val isDirectory: Boolean,
    val file: File? = null,
    val selectedFile: SelectedFile? = null,
    val icon: Drawable? = null
)

object InAppFileManager {

    fun ensureHiddenIndexDir(context: Context): File {
        val dir = File(context.filesDir, ".filemanager")
        if (!dir.exists()) dir.mkdirs()
        val nomedia = File(dir, ".nomedia")
        if (!nomedia.exists()) nomedia.createNewFile()
        return dir
    }

    fun listDirectory(dir: File): List<BrowserEntry> {
        val root = Environment.getExternalStorageDirectory()
        val children = dir.listFiles() ?: return emptyList()
        return children
            .filter { !it.name.startsWith(".") }
            .sortedWith(compareBy<File>({ !it.isDirectory }, { it.name.lowercase() }))
            .map { f ->
                val rel = f.absolutePath.removePrefix(root.absolutePath).trimStart('/')
                if (f.isDirectory) {
                    val count = f.listFiles()?.size ?: 0
                    BrowserEntry(
                        id = f.absolutePath,
                        title = f.name,
                        subtitle = "$count items",
                        isDirectory = true,
                        file = f
                    )
                } else {
                    val mb = f.length() / (1024.0 * 1024.0)
                    BrowserEntry(
                        id = f.absolutePath,
                        title = f.name,
                        subtitle = "%.2f MB".format(mb),
                        isDirectory = false,
                        file = f,
                        selectedFile = SelectedFile(Uri.fromFile(f), rel, f.length())
                    )
                }
            }
    }

    fun collectDirectoryFiles(dir: File): List<SelectedFile> {
        val root = Environment.getExternalStorageDirectory()
        val out = mutableListOf<SelectedFile>()
        dir.walkTopDown().filter { it.isFile && !it.name.startsWith(".") }.forEach { f ->
            val rel = f.absolutePath.removePrefix(root.absolutePath).trimStart('/')
            out.add(SelectedFile(Uri.fromFile(f), rel, f.length()))
        }
        return out
    }

    fun listInstalledAppsWithIcons(context: Context): List<BrowserEntry> {
        ensureHiddenIndexDir(context)
        val pm = context.packageManager
        return pm.getInstalledApplications(0)
            .filter { (it.flags and ApplicationInfo.FLAG_SYSTEM) == 0 && it.sourceDir != null }
            .mapNotNull { app ->
                val apk = File(app.sourceDir)
                if (!apk.exists()) return@mapNotNull null
                val label = pm.getApplicationLabel(app).toString()
                val safeName = label.replace(Regex("[^a-zA-Z0-9._-]"), "_")
                val mb = apk.length() / (1024.0 * 1024.0)
                BrowserEntry(
                    id = app.packageName,
                    title = label,
                    subtitle = "%.2f MB • %s".format(mb, app.packageName),
                    isDirectory = false,
                    file = apk,
                    selectedFile = SelectedFile(Uri.fromFile(apk), "Apps/$safeName.apk", apk.length()),
                    icon = pm.getApplicationIcon(app)
                )
            }
            .sortedBy { it.title.lowercase() }
    }
}
