/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.provider

import android.database.Cursor
import android.database.MatrixCursor
import android.os.CancellationSignal
import android.os.ParcelFileDescriptor
import android.provider.DocumentsContract
import android.provider.DocumentsProvider
import android.webkit.MimeTypeMap
import emu.skyline.R
import emu.skyline.SkylineApplication
import emu.skyline.getPublicFilesDir
import java.io.*

class DocumentsProvider : DocumentsProvider() {
    private val baseDirectory = File(SkylineApplication.instance.getPublicFilesDir().canonicalPath)

    companion object {
        private val DEFAULT_ROOT_PROJECTION : Array<String> = arrayOf(
            DocumentsContract.Root.COLUMN_ROOT_ID,
            DocumentsContract.Root.COLUMN_MIME_TYPES,
            DocumentsContract.Root.COLUMN_FLAGS,
            DocumentsContract.Root.COLUMN_ICON,
            DocumentsContract.Root.COLUMN_TITLE,
            DocumentsContract.Root.COLUMN_SUMMARY,
            DocumentsContract.Root.COLUMN_DOCUMENT_ID,
            DocumentsContract.Root.COLUMN_AVAILABLE_BYTES
        )

        private val DEFAULT_DOCUMENT_PROJECTION : Array<String> = arrayOf(
            DocumentsContract.Document.COLUMN_DOCUMENT_ID,
            DocumentsContract.Document.COLUMN_MIME_TYPE,
            DocumentsContract.Document.COLUMN_DISPLAY_NAME,
            DocumentsContract.Document.COLUMN_LAST_MODIFIED,
            DocumentsContract.Document.COLUMN_FLAGS,
            DocumentsContract.Document.COLUMN_SIZE
        )
    }

    override fun onCreate() : Boolean {
        return true
    }

    /**
     * @return The [File] that corresponds to the document ID supplied by [getDocumentId]
     */
    private fun getFile(documentId : String) : File {
        val file = File(documentId)
        if (!file.exists()) throw FileNotFoundException(file.absolutePath + " not found")
        return file
    }

    /**
     * @return A unique ID for the provided [File]
     */
    private fun getDocumentId(file : File) : Any? {
        return file.absolutePath
    }

    override fun queryRoots(projection : Array<out String>?) : Cursor {
        val cursor = MatrixCursor(projection ?: DEFAULT_ROOT_PROJECTION)
        val applicationTitle = SkylineApplication.instance.applicationInfo.loadLabel(SkylineApplication.instance.packageManager).toString()
        val baseDirectoryDocumentId = getDocumentId(baseDirectory)

        cursor.newRow().apply {
            add(DocumentsContract.Root.COLUMN_ROOT_ID, baseDirectoryDocumentId)
            add(DocumentsContract.Root.COLUMN_SUMMARY, null)
            add(DocumentsContract.Root.COLUMN_FLAGS, DocumentsContract.Root.FLAG_SUPPORTS_CREATE or DocumentsContract.Root.FLAG_SUPPORTS_IS_CHILD)
            add(DocumentsContract.Root.COLUMN_TITLE, applicationTitle)
            add(DocumentsContract.Root.COLUMN_DOCUMENT_ID, baseDirectoryDocumentId)
            add(DocumentsContract.Root.COLUMN_MIME_TYPES, "*/*")
            add(DocumentsContract.Root.COLUMN_AVAILABLE_BYTES, baseDirectory.freeSpace)
            add(DocumentsContract.Root.COLUMN_ICON, R.drawable.logo_skyline)
        }

        return cursor
    }

    override fun queryDocument(documentId : String?, projection : Array<out String>?) : Cursor {
        val cursor = MatrixCursor(projection ?: DEFAULT_DOCUMENT_PROJECTION)
        return includeFile(cursor, documentId, null)
    }

    override fun isChildDocument(parentDocumentId : String?, documentId : String?) : Boolean {
        return documentId?.startsWith(parentDocumentId!!) ?: false
    }

    override fun createDocument(parentDocumentId : String?, mimeType : String?, displayName : String) : String? {
        var newFile = File(parentDocumentId, displayName)
        var noConflictId = 1 // Makes sure two files don't have the same name by adding a number to the end
        while (newFile.exists())
            newFile = File(parentDocumentId, "$displayName (${noConflictId++})")

        try {
            if (DocumentsContract.Document.MIME_TYPE_DIR == mimeType) {
                if (!newFile.mkdir())
                    throw IOException("Failed to create directory")
            } else {
                if (!newFile.createNewFile())
                    throw IOException("Failed to create file")
            }
        } catch (e : IOException) {
            throw FileNotFoundException("Couldn't create document '${newFile.path}': ${e.message}")
        }

        return newFile.path
    }

    override fun deleteDocument(documentId : String?) {
        val file = getFile(documentId!!)
        if (!file.delete())
            throw FileNotFoundException("Couldn't delete document with ID '$documentId'")
    }

    override fun removeDocument(documentId : String, parentDocumentId : String?) {
        val parent = getFile(parentDocumentId!!)
        val file = getFile(documentId)

        var doesFileParentMatch = false
        val fileParent = file.parentFile

        if (fileParent == null || fileParent.equals(parent))
            doesFileParentMatch = true
        if (parent == file || doesFileParentMatch) {
            if (!file.delete())
                throw FileNotFoundException("Couldn't delete document with ID '$documentId'")
        } else {
            throw FileNotFoundException("Couldn't delete document with ID '$documentId'")
        }
    }

    override fun renameDocument(documentId : String?, displayName : String?) : String? {
        if (displayName == null)
            throw FileNotFoundException("Couldn't rename document '$documentId' as the new name is null")

        val sourceFile = getFile(documentId!!)
        val sourceParentFile = sourceFile.parentFile ?: throw FileNotFoundException("Couldn't rename document '$documentId' as it has no parent")
        val destFile = File(sourceParentFile.path, displayName)

        try {
            if (!sourceFile.renameTo(destFile))
                throw FileNotFoundException("Couldn't rename document from '${sourceFile.name}' to '${destFile.name}'")
        } catch (e : Exception) {
            throw FileNotFoundException("Couldn't rename document from '${sourceFile.name}' to '${destFile.name}': ${e.message}")
        }
        return getDocumentId(destFile).toString()
    }

    private fun copyDocument(
        sourceDocumentId : String, sourceParentDocumentId : String,
        targetParentDocumentId : String?
    ) : String? {
        if (!isChildDocument(sourceParentDocumentId, sourceDocumentId))
            throw FileNotFoundException("Couldn't copy document '$sourceDocumentId' as its parent is not '$sourceParentDocumentId'")

        return copyDocument(sourceDocumentId, targetParentDocumentId)
    }

    override fun copyDocument(sourceDocumentId : String, targetParentDocumentId : String?) : String? {
        val parent = getFile(targetParentDocumentId!!)
        val oldFile = getFile(sourceDocumentId)
        val newFile = File(parent.path, oldFile.name)
        try {
            if (!(newFile.createNewFile() && newFile.setWritable(true) && newFile.setReadable(true)))
                throw IOException("Couldn't create new file")

            FileInputStream(oldFile).use { inStream ->
                FileOutputStream(newFile).use { outStream ->
                    inStream.copyTo(outStream)
                }
            }
        } catch (e : IOException) {
            throw FileNotFoundException("Couldn't copy document '$sourceDocumentId': ${e.message}")
        }

        return getDocumentId(newFile).toString()
    }

    override fun moveDocument(
        sourceDocumentId : String, sourceParentDocumentId : String?,
        targetParentDocumentId : String?
    ) : String? {
        try {
            val newDocumentId = copyDocument(
                sourceDocumentId, sourceParentDocumentId!!,
                targetParentDocumentId
            )
            removeDocument(sourceDocumentId, sourceParentDocumentId)
            return newDocumentId
        } catch (e : FileNotFoundException) {
            throw FileNotFoundException("Couldn't move document '$sourceDocumentId'")
        }
    }

    private fun includeFile(cursor : MatrixCursor, documentId : String?, file : File?) : MatrixCursor {
        val localDocumentId = documentId ?: file?.let { getDocumentId(it) } as String?
        val localFile = file ?: getFile(documentId.toString())

        var flags = 0
        if (localFile.isDirectory && localFile.canWrite()) {
            flags = DocumentsContract.Document.FLAG_DIR_SUPPORTS_CREATE
        } else if (localFile.canWrite()) {
            flags = DocumentsContract.Document.FLAG_SUPPORTS_WRITE
            flags = flags or DocumentsContract.Document.FLAG_SUPPORTS_DELETE

            flags = flags or DocumentsContract.Document.FLAG_SUPPORTS_REMOVE
            flags = flags or DocumentsContract.Document.FLAG_SUPPORTS_MOVE
            flags = flags or DocumentsContract.Document.FLAG_SUPPORTS_COPY
            flags = flags or DocumentsContract.Document.FLAG_SUPPORTS_RENAME
        }

        val displayName = localFile.name
        val mimeType = getTypeForFile(localFile)

        cursor.newRow().apply {
            add(DocumentsContract.Document.COLUMN_DOCUMENT_ID, localDocumentId)
            add(DocumentsContract.Document.COLUMN_DISPLAY_NAME, displayName)
            add(DocumentsContract.Document.COLUMN_SIZE, localFile.length())
            add(DocumentsContract.Document.COLUMN_MIME_TYPE, mimeType)
            add(DocumentsContract.Document.COLUMN_LAST_MODIFIED, localFile.lastModified())
            add(DocumentsContract.Document.COLUMN_FLAGS, flags)
        }

        return cursor
    }

    private fun getTypeForFile(file : File) : Any? {
        return if (file.isDirectory)
            DocumentsContract.Document.MIME_TYPE_DIR
        else
            getTypeForName(file.name)
    }

    private fun getTypeForName(name : String) : Any? {
        val lastDot = name.lastIndexOf('.')
        if (lastDot >= 0) {
            val extension = name.substring(lastDot + 1)
            val mime = MimeTypeMap.getSingleton().getMimeTypeFromExtension(extension)
            if (mime != null)
                return mime
        }
        return "application/octect-stream"
    }

    override fun queryChildDocuments(parentDocumentId : String?, projection : Array<out String>?, sortOrder : String?) : Cursor {
        var cursor = MatrixCursor(projection ?: DEFAULT_DOCUMENT_PROJECTION)

        val parent = getFile(parentDocumentId!!)
        for (file in parent.listFiles()!!)
            cursor = includeFile(cursor, null, file)

        return cursor
    }

    override fun openDocument(documentId : String?, mode : String?, signal : CancellationSignal?) : ParcelFileDescriptor {
        val file = documentId?.let { getFile(it) }
        val accessMode = ParcelFileDescriptor.parseMode(mode)
        return ParcelFileDescriptor.open(file, accessMode)
    }
}
