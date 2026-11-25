package com.directdrop.app

import android.content.Context
import android.net.Uri
import androidx.documentfile.provider.DocumentFile
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONArray
import org.json.JSONObject
import java.io.DataInputStream
import java.io.DataOutputStream
import java.net.InetSocketAddress
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.security.MessageDigest
import java.security.SecureRandom
import java.security.cert.X509Certificate
import javax.net.ssl.SSLContext
import javax.net.ssl.SSLSocket
import javax.net.ssl.TrustManager
import javax.net.ssl.X509TrustManager

object Protocol {
    const val MAGIC = 0x44445031 // "DDP1"
    const val VERSION_MAJOR: Short = 1
    const val VERSION_MINOR: Short = 0
    const val CHUNK_SIZE = 256 * 1024

    const val MSG_HELLO: Short = 0x0001
    const val MSG_HELLO_ACK: Short = 0x0002
    const val MSG_PAIR_REQUEST: Short = 0x0010
    const val MSG_PAIR_CHALLENGE: Short = 0x0011
    const val MSG_PAIR_CONFIRM: Short = 0x0012
    const val MSG_MANIFEST: Short = 0x0020
    const val MSG_MANIFEST_ACK: Short = 0x0021
    const val MSG_FILE_START: Short = 0x0030
    const val MSG_FILE_CHUNK: Short = 0x0031
    const val MSG_FILE_END: Short = 0x0032
    const val MSG_FILE_ACK: Short = 0x0033
    const val MSG_RESUME_ACK: Short = 0x0041
    const val MSG_JOB_COMPLETE: Short = 0x0050
}

data class SelectedFile(
    val uri: Uri,
    val relativePath: String,
    val size: Long
)

class ProtocolClient(private val context: Context) {

    fun collectFilesFromTree(treeUri: Uri): List<SelectedFile> {
        val root = DocumentFile.fromTreeUri(context, treeUri) ?: return emptyList()
        val rootName = root.name ?: "Folder"
        val out = mutableListOf<SelectedFile>()
        walk(root, rootName, out)
        return out
    }

    private fun walk(dir: DocumentFile, prefix: String, out: MutableList<SelectedFile>) {
        for (child in dir.listFiles()) {
            val name = child.name ?: continue
            val rel = "$prefix/$name"
            if (child.isDirectory) {
                walk(child, rel, out)
            } else if (child.isFile) {
                out.add(SelectedFile(child.uri, rel, child.length()))
            }
        }
    }

    suspend fun transferFiles(
        host: String,
        port: Int = 47291,
        deviceId: String,
        deviceName: String,
        files: List<SelectedFile>,
        onSasCode: (String) -> Unit,
        onProgress: (verified: Int, total: Int, currentFile: String) -> Unit
    ) = withContext(Dispatchers.IO) {
        val trustAll = arrayOf<TrustManager>(object : X509TrustManager {
            override fun checkClientTrusted(chain: Array<X509Certificate>, authType: String) {}
            override fun checkServerTrusted(chain: Array<X509Certificate>, authType: String) {}
            override fun getAcceptedIssuers(): Array<X509Certificate> = emptyArray()
        })
        val sslCtx = SSLContext.getInstance("TLSv1.3")
        sslCtx.init(null, trustAll, SecureRandom())

        val socket = sslCtx.socketFactory.createSocket() as SSLSocket
        socket.enabledProtocols = arrayOf("TLSv1.3")
        socket.connect(InetSocketAddress(host, port), 10000)
        socket.startHandshake()

        val out = DataOutputStream(socket.outputStream)
        val inp = DataInputStream(socket.inputStream)

        sendJson(out, Protocol.MSG_HELLO, JSONObject().apply {
            put("deviceId", deviceId)
            put("deviceName", deviceName)
        })
        readFrame(inp) // HelloAck

        val nonce = ByteArray(16).also { SecureRandom().nextBytes(it) }.joinToString("") { "%02x".format(it) }
        sendJson(out, Protocol.MSG_PAIR_REQUEST, JSONObject().apply { put("nonce", nonce) })
        val pairResp = readFrame(inp)
        val pairJson = JSONObject(String(pairResp.second))
        onSasCode(pairJson.optString("sas", "------"))
        if (pairResp.first == Protocol.MSG_PAIR_CHALLENGE) {
            val confirm = readFrame(inp)
            if (confirm.first != Protocol.MSG_PAIR_CONFIRM) {
                socket.close()
                throw IllegalStateException("Pairing rejected")
            }
        }

        val manifestFiles = JSONArray()
        for (f in files) {
            manifestFiles.put(JSONObject().apply {
                put("relativePath", f.relativePath)
                put("size", f.size)
            })
        }
        sendJson(out, Protocol.MSG_MANIFEST, JSONObject().apply {
            put("jobId", "job-${System.currentTimeMillis()}")
            put("files", manifestFiles)
        })
        readFrame(inp) // ManifestAck

        var verified = 0
        val buf = ByteArray(Protocol.CHUNK_SIZE)
        for (f in files) {
            onProgress(verified, files.size, f.relativePath)
            val sha = computeSha256(f.uri)
            sendJson(out, Protocol.MSG_FILE_START, JSONObject().apply {
                put("relativePath", f.relativePath)
                put("size", f.size)
                put("sha256", sha)
            })
            val ack = readFrame(inp)
            val ackJson = JSONObject(String(ack.second))
            if (ack.first == Protocol.MSG_FILE_ACK && ackJson.optBoolean("skipped", false)) {
                verified++
                continue
            }
            val startOffset = ackJson.optLong("offset", 0L)

            context.contentResolver.openInputStream(f.uri)?.use { stream ->
                var skipped = 0L
                while (skipped < startOffset) {
                    val s = stream.skip(startOffset - skipped)
                    if (s <= 0) break
                    skipped += s
                }
                var offset = startOffset
                while (true) {
                    val n = stream.read(buf)
                    if (n <= 0) break
                    val payload = ByteBuffer.allocate(8 + n).order(ByteOrder.BIG_ENDIAN)
                    payload.putLong(offset)
                    payload.put(buf, 0, n)
                    sendFrame(out, Protocol.MSG_FILE_CHUNK, payload.array())
                    offset += n
                }
            }
            sendJson(out, Protocol.MSG_FILE_END, JSONObject())
            val endAck = readFrame(inp)
            val endJson = JSONObject(String(endAck.second))
            if (endJson.optBoolean("ok", false)) {
                verified++
            }
        }

        sendJson(out, Protocol.MSG_JOB_COMPLETE, JSONObject())
        out.flush()
        socket.close()
        onProgress(verified, files.size, "Completed")
    }

    private fun computeSha256(uri: Uri): String {
        val md = MessageDigest.getInstance("SHA-256")
        val buf = ByteArray(256 * 1024)
        context.contentResolver.openInputStream(uri)?.use { s ->
            while (true) {
                val n = s.read(buf)
                if (n <= 0) break
                md.update(buf, 0, n)
            }
        }
        return md.digest().joinToString("") { "%02x".format(it) }
    }

    private fun sendJson(out: DataOutputStream, type: Short, obj: JSONObject) {
        sendFrame(out, type, obj.toString().toByteArray(Charsets.UTF_8))
    }

    private fun sendFrame(out: DataOutputStream, type: Short, payload: ByteArray) {
        val hdr = ByteBuffer.allocate(16).order(ByteOrder.BIG_ENDIAN)
        hdr.putInt(Protocol.MAGIC)
        hdr.putShort(Protocol.VERSION_MAJOR)
        hdr.putShort(Protocol.VERSION_MINOR)
        hdr.putShort(type)
        hdr.putShort(0)
        hdr.putInt(payload.size)
        out.write(hdr.array())
        if (payload.isNotEmpty()) out.write(payload)
        out.flush()
    }

    private fun readFrame(inp: DataInputStream): Pair<Short, ByteArray> {
        val hdrBytes = ByteArray(16)
        inp.readFully(hdrBytes)
        val bb = ByteBuffer.wrap(hdrBytes).order(ByteOrder.BIG_ENDIAN)
        val magic = bb.int
        if (magic != Protocol.MAGIC) throw IllegalStateException("Invalid magic")
        bb.short // maj
        bb.short // min
        val type = bb.short
        bb.short // flags
        val len = bb.int
        val payload = ByteArray(len)
        if (len > 0) inp.readFully(payload)
        return type to payload
    }
}
