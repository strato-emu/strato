package emu.skyline

import android.content.Context
import android.net.Uri
import android.util.Log
import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import dagger.hilt.android.lifecycle.HiltViewModel
import emu.skyline.loader.AppEntry
import emu.skyline.loader.RomFormat
import emu.skyline.utils.fromFile
import emu.skyline.utils.toFile
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import java.io.File
import java.io.IOException
import javax.inject.Inject

sealed class MainState {
    object Loading : MainState()
    class Loaded(val items : HashMap<RomFormat, ArrayList<AppEntry>>) : MainState()
    class Error(val ex : Exception) : MainState()
}

@HiltViewModel
class MainViewModel @Inject constructor(private val romProvider : RomProvider) : ViewModel() {
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
                    state = MainState.Loaded(fromFile(romsFile))
                    return@launch
                } catch (e : Exception) {
                    Log.w(TAG, "Ran into exception while loading: ${e.message}")
                }
            }

            val romElements = romProvider.loadRoms(searchLocation)
            try {
                romElements.toFile(romsFile)
            } catch (e : IOException) {
                Log.w(TAG, "Ran into exception while saving: ${e.message}")
            }

            state = MainState.Loaded(romElements)
        }
    }
}
