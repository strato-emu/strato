/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package skyline.strato.data

import kotlinx.serialization.Serializable
import kotlinx.serialization.SerializationException
import kotlinx.serialization.json.*
import java.io.File

data class GpuDriverMetadata(
    val name : String,
    val author : String,
    val packageVersion : String,
    val vendor : String,
    val driverVersion : String,
    val minApi : Int,
    val description : String,
    val libraryName : String,
) {
    private constructor(metadataV1 : GpuDriverMetadataV1) : this(
        name = metadataV1.name,
        author = metadataV1.author,
        packageVersion = metadataV1.packageVersion,
        vendor = metadataV1.vendor,
        driverVersion = metadataV1.driverVersion,
        minApi = metadataV1.minApi,
        description = metadataV1.description,
        libraryName = metadataV1.libraryName,
    )

    val label get() = "${name}-v${packageVersion}"

    companion object {
        private const val SCHEMA_VERSION_V1 = 1

        fun deserialize(metadataFile : File) : GpuDriverMetadata {
            val metadataJson = Json.parseToJsonElement(metadataFile.readText())

            return when (metadataJson.jsonObject["schemaVersion"]?.jsonPrimitive?.intOrNull) {
                SCHEMA_VERSION_V1 -> GpuDriverMetadata(Json.decodeFromJsonElement<GpuDriverMetadataV1>(metadataJson))
                else -> throw SerializationException("Unsupported metadata version")
            }
        }
    }
}

@Serializable
private data class GpuDriverMetadataV1(
    val schemaVersion : Int,
    val name : String,
    val author : String,
    val packageVersion : String,
    val vendor : String,
    val driverVersion : String,
    val minApi : Int,
    val description : String,
    val libraryName : String,
)
