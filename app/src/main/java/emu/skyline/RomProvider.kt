package emu.skyline

import android.annotation.SuppressLint
import android.content.Context
import android.net.Uri
import androidx.documentfile.provider.DocumentFile
import dagger.hilt.android.qualifiers.ApplicationContext
import emu.skyline.loader.AppEntry
import emu.skyline.loader.RomFile
import emu.skyline.loader.RomFormat
import emu.skyline.loader.RomFormat.*
import javax.inject.Inject
import javax.inject.Singleton

@Singleton
class RomProvider @Inject constructor(@ApplicationContext private val context : Context) {
    /**
     * This adds all files in [directory] with [extension] as an entry using [RomFile] to load metadata
     */
    @SuppressLint("DefaultLocale")
    private fun addEntries(fileFormats : Map<String, RomFormat>, directory : DocumentFile, entries : HashMap<RomFormat, ArrayList<AppEntry>>) {
        directory.listFiles().forEach { file ->
            if (file.isDirectory) {
                addEntries(fileFormats, file, entries)
            } else {
                fileFormats[file.name?.substringAfterLast(".")?.toLowerCase()]?.let { romFormat->
                    entries.getOrPut(romFormat, { arrayListOf() }).add(RomFile(context, romFormat, file.uri).appEntry)
                }
            }
        }
    }

    fun loadRoms(searchLocation : Uri) = DocumentFile.fromTreeUri(context, searchLocation)!!.let { documentFile ->
        hashMapOf<RomFormat, ArrayList<AppEntry>>().apply {
            addEntries(mapOf("nro" to NRO, "nso" to NSO, "nca" to NCA, "nsp" to NSP, "xci" to XCI), documentFile, this)
        }
    }
}
