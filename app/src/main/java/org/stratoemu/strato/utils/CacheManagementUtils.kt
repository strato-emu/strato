/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2024 Strato Team and Contributors (https://github.com/strato-emu/)
 */

package org.stratoemu.strato.utils

import org.stratoemu.strato.StratoApplication
import org.stratoemu.strato.getPublicFilesDir
import java.io.File

class CacheManagementUtils {

    companion object {
        private val cacheFolderRoot by lazy { "${StratoApplication.instance.getPublicFilesDir().canonicalPath}/graphics_pipeline_cache/" }

        fun pipelineCacheExists(titleId: String) : Boolean {
            val cacheFolderPath = cacheFolderRoot + titleId
            val cacheFolderPathExt = "$cacheFolderRoot$titleId.staging"

            return File(cacheFolderPath).exists() && File(cacheFolderPathExt).exists()
        }

        /**
         * Deletes the cache files for a given game.
         */
        fun deleteCacheFile(titleId : String) : Boolean {
            return File("${cacheFolderRoot}$titleId").delete()
                    && File("${cacheFolderRoot}$titleId.staging").delete()
        }
    }
}
