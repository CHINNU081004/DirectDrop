package com.directdrop.app

import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.media.ThumbnailUtils
import android.os.Build
import android.util.LruCache
import android.util.Size
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.CheckBox
import android.widget.ImageView
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

class BrowserAdapter(
    private val context: Context,
    private val scope: CoroutineScope,
    private val selectedIds: MutableSet<String>,
    private val onFolderClick: (BrowserEntry) -> Unit,
    private val onToggleItem: (BrowserEntry, Boolean) -> Unit
) : RecyclerView.Adapter<BrowserAdapter.VH>() {

    private var entries: List<BrowserEntry> = emptyList()
    private val thumbCache = object : LruCache<String, Bitmap>(120) {}
    var isGridMode: Boolean = false

    fun submitList(list: List<BrowserEntry>) {
        entries = list
        notifyDataSetChanged()
    }

    override fun getItemViewType(position: Int): Int = if (isGridMode) 1 else 0

    class VH(v: View) : RecyclerView.ViewHolder(v) {
        val icon: ImageView = v.findViewById(R.id.entryIcon)
        val title: TextView = v.findViewById(R.id.entryTitle)
        val subtitle: TextView = v.findViewById(R.id.entrySubtitle)
        val check: CheckBox = v.findViewById(R.id.entryCheck)
        var thumbJob: Job? = null
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): VH {
        val layout = if (viewType == 1) R.layout.item_browser_grid else R.layout.item_browser_entry
        val v = LayoutInflater.from(parent.context).inflate(layout, parent, false)
        return VH(v)
    }

    override fun getItemCount(): Int = entries.size

    override fun onBindViewHolder(holder: VH, position: Int) {
        val item = entries[position]
        holder.thumbJob?.cancel()
        holder.title.text = item.title
        holder.subtitle.text = item.subtitle

        val cached = thumbCache.get(item.id)
        if (cached != null) {
            holder.icon.setImageBitmap(cached)
        } else if (item.icon != null) {
            holder.icon.setImageDrawable(item.icon)
        } else if (item.isDirectory) {
            holder.icon.setImageResource(android.R.drawable.ic_menu_agenda)
        } else {
            holder.icon.setImageResource(android.R.drawable.ic_menu_gallery)
            val sf = item.selectedFile
            if (sf != null && isMediaFile(item.title)) {
                holder.thumbJob = scope.launch(Dispatchers.IO) {
                    val bmp = loadThumb(item, sf)
                    if (bmp != null) {
                        thumbCache.put(item.id, bmp)
                        withContext(Dispatchers.Main) {
                            if (holder.bindingAdapterPosition == position) {
                                holder.icon.setImageBitmap(bmp)
                            }
                        }
                    }
                }
            }
        }

        holder.check.setOnCheckedChangeListener(null)
        holder.check.isChecked = selectedIds.contains(item.id)
        holder.check.setOnCheckedChangeListener { _, isChecked ->
            if (isChecked) selectedIds.add(item.id) else selectedIds.remove(item.id)
            onToggleItem(item, isChecked)
        }

        holder.itemView.setOnClickListener {
            if (item.isDirectory) {
                onFolderClick(item)
            } else {
                holder.check.isChecked = !holder.check.isChecked
            }
        }
    }

    private fun isMediaFile(name: String): Boolean {
        val lower = name.lowercase()
        return lower.endsWith(".jpg") || lower.endsWith(".jpeg") || lower.endsWith(".png") ||
            lower.endsWith(".webp") || lower.endsWith(".mp4") || lower.endsWith(".mkv") || lower.endsWith(".mov")
    }

    private fun loadThumb(item: BrowserEntry, sf: SelectedFile): Bitmap? {
        return try {
            if (sf.uri.scheme == "content" && Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                context.contentResolver.loadThumbnail(sf.uri, Size(160, 160), null)
            } else if (item.file != null) {
                val lower = item.file.name.lowercase()
                if (lower.endsWith(".mp4") || lower.endsWith(".mkv") || lower.endsWith(".mov")) {
                    @Suppress("DEPRECATION")
                    ThumbnailUtils.createVideoThumbnail(item.file.absolutePath, android.provider.MediaStore.Images.Thumbnails.MINI_KIND)
                } else {
                    val opts = BitmapFactory.Options().apply { inSampleSize = 8 }
                    BitmapFactory.decodeFile(item.file.absolutePath, opts)
                }
            } else null
        } catch (_: Exception) {
            null
        }
    }
}
