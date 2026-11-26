package com.directdrop.app

import android.content.ContentUris
import android.content.Context
import android.content.pm.ApplicationInfo
import android.net.Uri
import android.provider.MediaStore
import java.io.File

object MediaBrowser {

    fun queryImages(context: Context, limit: Int = 50): List<SelectedFile> =
        queryMedia(context, MediaStore.Images.Media.EXTERNAL_CONTENT_URI, "Pictures", limit)

    fun queryVideos(context: Context, limit: Int = 50): List<SelectedFile> =
        queryMedia(context, MediaStore.Video.Media.EXTERNAL_CONTENT_URI, "Movies", limit)

    fun queryAudio(context: Context, limit: Int = 50): List<SelectedFile> =
        queryMedia(context, MediaStore.Audio.Media.EXTERNAL_CONTENT_URI, "Music", limit)

    private fun queryMedia(context: Context, collection: Uri, folder: String, limit: Int): List<SelectedFile> {
        val out = mutableListOf<SelectedFile>()
        val projection = arrayOf(
            MediaStore.MediaColumns._ID,
            MediaStore.MediaColumns.DISPLAY_NAME,
            MediaStore.MediaColumns.SIZE
        )
        val sort = "${MediaStore.MediaColumns.DATE_ADDED} DESC"
        context.contentResolver.query(collection, projection, null, null, sort)?.use { cursor ->
            val idCol = cursor.getColumnIndexOrThrow(MediaStore.MediaColumns._ID)
            val nameCol = cursor.getColumnIndexOrThrow(MediaStore.MediaColumns.DISPLAY_NAME)
            val sizeCol = cursor.getColumnIndexOrThrow(MediaStore.MediaColumns.SIZE)
            while (cursor.moveToNext() && out.size < limit) {
                val id = cursor.getLong(idCol)
                val name = cursor.getString(nameCol) ?: "media_$id"
                val size = cursor.getLong(sizeCol)
                val uri = ContentUris.withAppendedId(collection, id)
                out.add(SelectedFile(uri, "$folder/$name", size))
            }
        }
        return out
    }

    fun listInstalledUserApps(context: Context, limit: Int = 50): List<SelectedFile> {
        val pm = context.packageManager
        val out = mutableListOf<SelectedFile>()
        val apps = pm.getInstalledApplications(0)
        for (app in apps) {
            if ((app.flags and ApplicationInfo.FLAG_SYSTEM) != 0) continue
            val apkFile = File(app.sourceDir ?: continue)
            if (!apkFile.exists()) continue
            val label = pm.getApplicationLabel(app).toString().replace(Regex("[^a-zA-Z0-9._-]"), "_")
            out.add(SelectedFile(Uri.fromFile(apkFile), "Apps/$label.apk", apkFile.length()))
            if (out.size >= limit) break
        }
        return out
    }
}
