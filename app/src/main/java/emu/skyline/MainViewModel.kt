package emu.skyline

import android.content.Context
import android.net.Uri
import android.util.Log
import androidx.documentfile.provider.DocumentFile
import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import emu.skyline.data.AppItem
import emu.skyline.data.DataItem
import emu.skyline.data.HeaderItem
import emu.skyline.loader.RomFile
import emu.skyline.loader.RomFormat
import emu.skyline.utils.loadSerializedList
import emu.skyline.utils.serialize
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import java.io.File
import java.io.IOException

sealed class MainState {
    object Loading : MainState()
    class Loaded(val items : List<DataItem>) : MainState()
    class Error(val ex : Exception) : MainState()
}

class MainViewModel : ViewModel() {
    companion object {
        private val TAG = MainViewModel::class.java.simpleName
    }

    private var state
        get() = _stateData.value
        set(value) = _stateData.postValue(value)
    private val _stateData = MutableLiveData<MainState>()
    val stateData : LiveData<MainState> = _stateData

    var searchBarAnimated = false

    /**
     * This adds all files in [directory] with [extension] as an entry using [RomFile] to load metadata
     */
    private fun addEntries(context : Context, extension : String, romFormat : RomFormat, directory : DocumentFile, romElements : ArrayList<DataItem>, found : Boolean = false) : Boolean {
        var foundCurrent = found

        directory.listFiles().forEach { file ->
            if (file.isDirectory) {
                foundCurrent = addEntries(context, extension, romFormat, file, romElements, foundCurrent)
            } else {
                if (extension.equals(file.name?.substringAfterLast("."), ignoreCase = true)) {
                    RomFile(context, romFormat, file.uri).let { romFile ->
                        if (!foundCurrent) romElements.add(HeaderItem(romFormat.name))
                        romElements.add(AppItem(romFile.appEntry))

                        foundCurrent = true
                    }
                }
            }
        }

        return foundCurrent
    }

    /**
     * This refreshes the contents of the adapter by either trying to load cached adapter data or searches for them to recreate a list
     *
     * @param loadFromFile If this is false then trying to load cached adapter data is skipped entirely
     */
    fun loadRoms(context : Context, loadFromFile : Boolean, searchLocation : Uri) {
        if (state == MainState.Loading) return
        state = MainState.Loading

        val romsFile = File(context.filesDir.canonicalPath + "/roms.bin")

        viewModelScope.launch(Dispatchers.IO) {
            if (loadFromFile) {
                try {
                    state = MainState.Loaded(loadSerializedList(romsFile))
                    return@launch
                } catch (e : Exception) {
                    Log.w(TAG, "Ran into exception while loading: ${e.message}")
                }
            }

            try {
                val searchDocument = DocumentFile.fromTreeUri(context, searchLocation)!!

                val romElements = ArrayList<DataItem>()
                addEntries(context, "nsp", RomFormat.NSP, searchDocument, romElements)
                addEntries(context, "xci", RomFormat.XCI, searchDocument, romElements)
                addEntries(context, "nro", RomFormat.NRO, searchDocument, romElements)
                addEntries(context, "nso", RomFormat.NSO, searchDocument, romElements)
                addEntries(context, "nca", RomFormat.NCA, searchDocument, romElements)

                try {
                    romElements.serialize(romsFile)
                } catch (e : IOException) {
                    Log.w(TAG, "Ran into exception while saving: ${e.message}")
                }

                state = MainState.Loaded(romElements)
            } catch (e : Exception) {
                state = MainState.Error(e)
            }
        }
    }
}
