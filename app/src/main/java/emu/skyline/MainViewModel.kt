package emu.skyline

import android.app.Application
import android.content.Context
import android.net.Uri
import android.util.Log
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.viewModelScope
import dagger.hilt.android.lifecycle.HiltViewModel
import dagger.hilt.android.qualifiers.ApplicationContext
import emu.skyline.loader.AppEntry
import emu.skyline.utils.fromFile
import emu.skyline.utils.toFile
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import java.io.File
import javax.inject.Inject

sealed class MainState {
    object Loading : MainState()
    class Loaded(val items : ArrayList<AppEntry>) : MainState()
    class Error(val ex : Exception) : MainState()
}

@HiltViewModel
class MainViewModel @Inject constructor(@ApplicationContext context : Context, private val romProvider : RomProvider) : AndroidViewModel(context as Application) {
    companion object {
        private val TAG = MainViewModel::class.java.simpleName
    }

    private var state
        get() = _stateData.value
        set(value) = _stateData.postValue(value)
    private val _stateData = MutableLiveData<MainState>()
    val stateData : LiveData<MainState> = _stateData

    /**
     * This refreshes the contents of the adapter by either trying to load cached adapter data or searches for them to recreate a list
     *
     * @param loadFromFile If this is false then trying to load cached adapter data is skipped entirely
     */
    fun loadRoms(context : Context, loadFromFile : Boolean, searchLocation : Uri, systemLanguage : Int) {
        if (state == MainState.Loading)
            return
        state = MainState.Loading

        val romsFile = File(getApplication<SkylineApplication>().filesDir.canonicalPath + "/roms.bin")

        viewModelScope.launch(Dispatchers.IO) {
            if (loadFromFile && romsFile.exists()) {
                try {
                    state = MainState.Loaded(fromFile(romsFile))
                    checkRomHash(searchLocation, systemLanguage)
                    return@launch
                } catch (e : Exception) {
                    Log.w(TAG, "Ran into exception while loading: ${e.message}")
                }
            }

            state = if (searchLocation.toString().isEmpty()) {
                MainState.Loaded(ArrayList())
            } else {
                try {
                    KeyReader.importFromLocation(context, searchLocation)
                    val romElements = romProvider.loadRoms(searchLocation, systemLanguage)
                    romElements.toFile(romsFile)
                    MainState.Loaded(romElements)
                } catch (e : Exception) {
                    Log.w(TAG, "Ran into exception while saving: ${e.message}")
                    MainState.Error(e)
                }
            }
        }
    }

    /**
     * Tracks whether an auto refresh is already in progress
     */
    private var isAutoRefreshingRoms = false

    /**
     * This checks if the roms have changed since the last time they were loaded and if so it reloads them
     */
    fun checkRomHash(searchLocation : Uri, systemLanguage : Int) {
        // Skip if an auto refresh is already in progress or if the state hasn't already loaded
        if (isAutoRefreshingRoms || state !is MainState.Loaded)
            return
        isAutoRefreshingRoms = true

        viewModelScope.launch(Dispatchers.IO) {
            val currentHash = when (val currentState = state) {
                is MainState.Loaded -> currentState.items.hashCode()
                else -> 0
            }
            val romElements = romProvider.loadRoms(searchLocation, systemLanguage)
            val newHash = romElements.hashCode()
            if (newHash != currentHash)
                state = MainState.Loaded(romElements)

            isAutoRefreshingRoms = false
        }
    }
}
