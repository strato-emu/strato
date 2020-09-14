/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.preference

import android.content.Intent

/**
 * Launches document picker to select a folder
 */
class FolderActivity : DocumentActivity() {
    override val actionIntent = Intent(Intent.ACTION_OPEN_DOCUMENT_TREE)
}
