/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2023 Strato Team and Contributors (https://github.com/strato-emu/)
 */

package emu.skyline.utils

import android.content.Context
import android.net.Uri
import android.util.Log
import emu.skyline.di.getSettings

interface SearchLocationHelper {
    companion object {

        /**
         * Returns the list of selected search locations.
         * @return An uris array of selected search locations
         */
        fun getSearchLocations(context : Context) : Array<Uri?> {
            var locations = context.getSettings().searchLocation.split("|");
            var urisArray = arrayOf<Uri?>();

            locations.forEach{
                val array = urisArray.copyOf(urisArray.size + 1)
                array[urisArray.size] = Uri.parse(it);
                urisArray = array;
            }
            return urisArray;
        }

        /**
         * Adds the given search location to the emulation settings
         * @param uri The uri os the selected search location
         * @return The exit status of the installation process
         */
        fun addLocation(context : Context, uri : Uri) : SearchLocationResult {
            var locations = getSearchLocations(context)
            if(locations.contains(uri)){
                return SearchLocationResult.AlreadyAdded
            }
            else{
                context.getSettings().searchLocation = StringBuilder().append(context.getSettings().searchLocation).append(if(locations.isEmpty()) "" else "|").append(uri.toString()).toString()
                context.getSettings().refreshRequired = true;
            }
            return SearchLocationResult.Success
        }

        /**
         * Deletes the given uri from the emulation settings
         * @param uri The input stream to read the driver from
         * @return The exit status of the installation process
         */
        fun deleteLocation(context : Context, uri : Uri) : SearchLocationResult {
            var locations = context.getSettings().searchLocation.split("|");

            var newValue = ""
            for (location in locations) {
                if(location != uri.toString() && location != "")
                    newValue = StringBuilder().append(newValue).append(if(newValue.isEmpty()) "" else "|").append(location).toString()
            }
            context.getSettings().searchLocation = newValue;
            context.getSettings().refreshRequired = true;
            return SearchLocationResult.Deleted
        }
    }
}

enum class SearchLocationResult {
    Success,
    AlreadyAdded,
    Deleted
}
