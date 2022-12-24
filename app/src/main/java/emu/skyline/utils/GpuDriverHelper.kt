/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.utils

import android.content.Context
import android.os.Build
import android.util.Log
import emu.skyline.R
import emu.skyline.data.GpuDriverMetadata
import emu.skyline.getPublicFilesDir
import kotlinx.serialization.SerializationException
import java.io.File
import java.io.IOException
import java.io.InputStream

private const val GPU_DRIVER_DIRECTORY = "gpu_drivers"
private const val GPU_DRIVER_FILE_REDIRECT_DIR = "gpu/vk_file_redirect"
private const val GPU_DRIVER_INSTALL_TEMP_DIR = "driver_temp"
private const val GPU_DRIVER_META_FILE = "meta.json"
private const val TAG = "GPUDriverHelper"

interface GpuDriverHelper {
    companion object {
        /**
         * Returns information about the system GPU driver.
         * @return An array containing the driver vendor and the driver version, in this order, or `null` if an error occurred
         */
        private external fun getSystemDriverInfo() : Array<String>?

        /**
         * Queries the driver for custom driver loading support.
         * @return `true` if the device supports loading custom drivers, `false` otherwise
         */
        external fun supportsCustomDriverLoading() : Boolean

        /**
         * Queries the driver for manual max clock forcing support
         */
        external fun supportsForceMaxGpuClocks() : Boolean

        /**
         * Calls into the driver to force the GPU to run at the maximum possible clock speed
         * @param force Whether to enable or disable the forced clocks
         */
        external fun forceMaxGpuClocks(enable : Boolean)

        /**
         * Returns the list of installed gpu drivers.
         * @return A map from the folder the driver is installed to the metadata of the driver
         */
        fun getInstalledDrivers(context : Context) : Map<File, GpuDriverMetadata> {
            val gpuDriverDir = getDriversDirectory(context)

            // A map between the driver location and its metadata
            val driverMap = mutableMapOf<File, GpuDriverMetadata>()

            gpuDriverDir.listFiles()?.forEach { entry ->
                // Delete any files that aren't a directory
                if (!entry.isDirectory) {
                    entry.delete()
                    return@forEach
                }

                val metadataFile = File(entry.canonicalPath, GPU_DRIVER_META_FILE)
                // Delete entries without metadata
                if (!metadataFile.exists()) {
                    entry.delete()
                    return@forEach
                }

                try {
                    driverMap[entry] = GpuDriverMetadata.deserialize(metadataFile)
                } catch (e : SerializationException) {
                    Log.w(TAG, "Failed to load gpu driver metadata for ${entry.name}, skipping\n${e.message}")
                }
            }

            return driverMap
        }

        /**
         * Fetches metadata about the system driver.
         * @return A [GpuDriverMetadata] object containing data about the system driver
         */
        fun getSystemDriverMetadata(context : Context) : GpuDriverMetadata {
            val systemDriverInfo = getSystemDriverInfo()

            return GpuDriverMetadata(
                name = context.getString(R.string.system_driver),
                author = "",
                packageVersion = "",
                vendor = systemDriverInfo?.get(0) ?: "",
                driverVersion = systemDriverInfo?.get(1) ?: "",
                minApi = 0,
                description = context.getString(R.string.system_driver_desc),
                libraryName = ""
            )
        }

        /**
         * Installs the given driver to the emulator's drivers directory.
         * @param stream The input stream to read the driver from
         * @return The exit status of the installation process
         */
        fun installDriver(context : Context, stream : InputStream) : GpuDriverInstallResult {
            val installTempDir = File(context.cacheDir.canonicalPath, GPU_DRIVER_INSTALL_TEMP_DIR).apply {
                deleteRecursively()
            }

            try {
                ZipUtils.unzip(stream, installTempDir)
            } catch (e : Exception) {
                e.printStackTrace()
                installTempDir.deleteRecursively()
                return GpuDriverInstallResult.InvalidArchive
            }

            return installUnpackedDriver(context, installTempDir)
        }

        /**
         * Installs the given driver to the emulator's drivers directory.
         * @param file The file to read the driver from
         * @return The exit status of the installation process
         */
        fun installDriver(context : Context, file : File) : GpuDriverInstallResult {
            val installTempDir = File(context.cacheDir.canonicalPath, GPU_DRIVER_INSTALL_TEMP_DIR).apply {
                deleteRecursively()
            }

            try {
                ZipUtils.unzip(file, installTempDir)
            } catch (e : Exception) {
                e.printStackTrace()
                installTempDir.deleteRecursively()
                return GpuDriverInstallResult.InvalidArchive
            }

            return installUnpackedDriver(context, installTempDir)
        }

        /**
         * Installs the given unpacked driver to the emulator's drivers directory.
         * @param unpackDir The location of the unpacked driver
         * @return The exit status of the installation process
         */
        private fun installUnpackedDriver(context : Context, unpackDir : File) : GpuDriverInstallResult {
            val cleanup = {
                unpackDir.deleteRecursively()
            }

            // Check that the metadata file exists
            val metadataFile = File(unpackDir, GPU_DRIVER_META_FILE)
            if (!metadataFile.isFile) {
                cleanup()
                return GpuDriverInstallResult.MissingMetadata
            }

            // Check that the driver metadata is valid
            val driverMetadata = try {
                GpuDriverMetadata.deserialize(metadataFile)
            } catch (e : SerializationException) {
                cleanup()
                return GpuDriverInstallResult.InvalidMetadata
            }

            // Check that the device satisfies the driver's minimum Android version requirements
            if (Build.VERSION.SDK_INT < driverMetadata.minApi) {
                cleanup()
                return GpuDriverInstallResult.UnsupportedAndroidVersion
            }

            // Check that the driver is not already installed
            val installedDrivers = getInstalledDrivers(context)
            val finalInstallDir = File(getDriversDirectory(context), driverMetadata.label)
            if (installedDrivers[finalInstallDir] != null) {
                cleanup()
                return GpuDriverInstallResult.AlreadyInstalled
            }

            // Move the driver files to the final location
            if (!unpackDir.renameTo(finalInstallDir)) {
                cleanup()
                throw IOException("Failed to create directory ${finalInstallDir.name}")
            }

            return GpuDriverInstallResult.Success
        }

        /**
         * Retrieves the library name from the driver metadata for the given driver.
         */
        fun getLibraryName(context : Context, driverLabel : String) : String {
            val driverDir = File(getDriversDirectory(context), driverLabel)
            val metadataFile = File(driverDir, GPU_DRIVER_META_FILE)
            return try {
                GpuDriverMetadata.deserialize(metadataFile).libraryName
            } catch (e : SerializationException) {
                Log.w(TAG, "Failed to load library name for driver ${driverLabel}, driver may not exist or have invalid metadata")
                ""
            }
        }

        fun ensureFileRedirectDir(context : Context) {
            File(context.getPublicFilesDir(), GPU_DRIVER_FILE_REDIRECT_DIR).apply {
                if (!isDirectory) {
                    delete()
                    mkdirs()
                }
            }
        }

        /**
         * Returns the drivers' folder from
         */
        private fun getDriversDirectory(context : Context) = File(context.filesDir.canonicalPath, GPU_DRIVER_DIRECTORY).apply {
            // Create the directory if it doesn't exist
            if (!isDirectory) {
                delete()
                mkdirs()
            }
        }
    }
}

enum class GpuDriverInstallResult {
    Success,
    InvalidArchive,
    MissingMetadata,
    InvalidMetadata,
    UnsupportedAndroidVersion,
    AlreadyInstalled,
}
