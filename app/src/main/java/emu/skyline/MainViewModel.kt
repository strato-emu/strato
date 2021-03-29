package emu.skyline

import android.app.Application
import android.content.Context
import android.net.Uri
import android.util.Log
import androidx.lifecycle.*
import dagger.hilt.android.lifecycle.HiltViewModel
import dagger.hilt.android.qualifiers.ApplicationContext
import emu.skyline.loader.AppEntry
import emu.skyline.loader.RomFormat
import emu.skyline.utils.fromFile
import emu.skyline.utils.toFile
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import java.io.File
import java.util.*
import javax.inject.Inject
import kotlin.collections.HashMap

sealed class MainState {
    object Loading : MainState()
    class Loaded(val items : HashMap<RomFormat, ArrayList<AppEntry>>) : MainState()
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

    var searchBarAnimated = false

    /**
     * This refreshes the contents of the adapter by either trying to load cached adapter data or searches for them to recreate a list
     *
     * @param loadFromFile If this is false then trying to load cached adapter data is skipped entirely
     */
    fun loadRoms(loadFromFile : Boolean, searchLocation : Uri) {
        if (state == MainState.Loading) return
        state = MainState.Loading

        val romsFile = File(getApplication<SkylineApplication>().filesDir.canonicalPath + "/roms.bin")

        viewModelScope.launch(Dispatchers.IO) {
            if (loadFromFile && romsFile.exists()) {
                try {
                    state = MainState.Loaded(fromFile(romsFile))
                    return@launch
                } catch (e : Exception) {
                    Log.w(TAG, "Ran into exception while loading: ${e.message}")
                }
            }

            state = if (searchLocation.toString().isEmpty()) {
                @Suppress("ReplaceWithEnumMap")
                MainState.Loaded(HashMap())
            } else {
                try {
                    val romElements = romProvider.loadRoms(searchLocation)
                    romElements.toFile(romsFile)
                    MainState.Loaded(romElements)
                } catch (e : Exception) {
                    Log.w(TAG, "Ran into exception while saving: ${e.message}")
                    MainState.Error(e)
                }
            }
        }
    }
}
