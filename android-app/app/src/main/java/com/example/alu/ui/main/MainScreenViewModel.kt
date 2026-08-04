package com.example.alu.ui.main

import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.net.Uri
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.example.alu.AluBridge
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import java.io.File
import java.io.FileOutputStream
import android.provider.MediaStore
import android.content.ContentUris

class MainScreenViewModel : ViewModel() {
    private val _uiState = MutableStateFlow(MainScreenUiState())
    val uiState: StateFlow<MainScreenUiState> = _uiState.asStateFlow()

    fun updateWidth(width: Int) {
        _uiState.value = _uiState.value.copy(width = width)
    }

    fun updateHeight(height: Int) {
        _uiState.value = _uiState.value.copy(height = height)
    }

    fun updateQuality(quality: Int) {
        _uiState.value = _uiState.value.copy(quality = quality)
    }

    fun updateFit(fit: Boolean) {
        _uiState.value = _uiState.value.copy(fit = fit)
    }

    fun updateBicubic(bicubic: Boolean) {
        _uiState.value = _uiState.value.copy(bicubic = bicubic)
    }

    fun updateBrightness(brightness: Float) {
        _uiState.value = _uiState.value.copy(brightness = brightness)
    }

    fun updateContrast(contrast: Float) {
        _uiState.value = _uiState.value.copy(contrast = contrast)
    }

    fun updateSpatialFilter(spatialFilter: Float) {
        _uiState.value = _uiState.value.copy(spatialFilter = spatialFilter)
    }

    fun onImageSelected(uri: Uri?) {
        _uiState.value = _uiState.value.copy(selectedImageUri = uri, processedBitmap = null)
    }

    fun loadGallery(context: Context) {
        viewModelScope.launch(Dispatchers.IO) {
            val imageList = mutableListOf<Uri>()
            val projection = arrayOf(MediaStore.Images.Media._ID)
            val sortOrder = "${MediaStore.Images.Media.DATE_ADDED} DESC"
            context.contentResolver.query(
                MediaStore.Images.Media.EXTERNAL_CONTENT_URI,
                projection,
                null,
                null,
                sortOrder
            )?.use { cursor ->
                val idColumn = cursor.getColumnIndexOrThrow(MediaStore.Images.Media._ID)
                while (cursor.moveToNext() && imageList.size < 50) {
                    val id = cursor.getLong(idColumn)
                    val contentUri = ContentUris.withAppendedId(MediaStore.Images.Media.EXTERNAL_CONTENT_URI, id)
                    imageList.add(contentUri)
                }
            }
            _uiState.value = _uiState.value.copy(galleryImages = imageList)
        }
    }

    fun processImage(context: Context) {
        val currentState = _uiState.value
        val uri = currentState.selectedImageUri ?: return

        _uiState.value = currentState.copy(isProcessing = true, error = null)

        viewModelScope.launch(Dispatchers.IO) {
            try {
                // 1. Copy URI to temp input file
                val inStream = context.contentResolver.openInputStream(uri)
                val tempInFile = File(context.cacheDir, "input.jpg")
                val outStream = FileOutputStream(tempInFile)
                inStream?.use { input ->
                    outStream.use { output ->
                        input.copyTo(output)
                    }
                }

                // 2. Define output file
                val tempOutFile = File(context.cacheDir, "output.png")

                // 3. Call JNI
                val fitInt = if (currentState.fit) 1 else 0
                val bicubicInt = if (currentState.bicubic) 1 else 0
                
                val result = AluBridge.process_image(
                    tempInFile.absolutePath,
                    currentState.width,
                    currentState.height,
                    tempOutFile.absolutePath,
                    currentState.quality,
                    fitInt,
                    bicubicInt
                )

                if (result != 0) {
                    _uiState.value = _uiState.value.copy(isProcessing = false, error = "Processing failed with code $result")
                    return@launch
                }

                // 4. Load output bitmap
                val bitmap = BitmapFactory.decodeFile(tempOutFile.absolutePath)
                _uiState.value = _uiState.value.copy(isProcessing = false, processedBitmap = bitmap, error = null)
                
            } catch (e: Exception) {
                e.printStackTrace()
                _uiState.value = _uiState.value.copy(isProcessing = false, error = e.message)
            }
        }
    }
}

data class MainScreenUiState(
    val selectedImageUri: Uri? = null,
    val galleryImages: List<Uri> = emptyList(),
    val processedBitmap: Bitmap? = null,
    val isProcessing: Boolean = false,
    val brightness: Float = 0f,
    val contrast: Float = 1f,
    val spatialFilter: Float = 0f,
    val width: Int = 800,
    val height: Int = 600,
    val quality: Int = 80,
    val fit: Boolean = true,
    val bicubic: Boolean = true,
    val error: String? = null
)
