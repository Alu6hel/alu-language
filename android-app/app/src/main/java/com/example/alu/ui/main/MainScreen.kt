package com.example.alu.ui.main

import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.Image
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.viewmodel.compose.viewModel
import androidx.compose.foundation.clickable
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.items
import androidx.compose.ui.layout.ContentScale
import coil.compose.AsyncImage
import com.example.alu.theme.AluTheme

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun MainScreen(
    modifier: Modifier = Modifier,
    viewModel: MainScreenViewModel = viewModel()
) {
    val state by viewModel.uiState.collectAsStateWithLifecycle()
    val context = LocalContext.current
    var showBottomSheet by remember { mutableStateOf(false) }

    LaunchedEffect(Unit) {
        viewModel.loadGallery(context)
    }

    val photoPickerLauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.GetContent(),
        onResult = { uri -> viewModel.onImageSelected(uri) }
    )

    Column(
        modifier = modifier
            .fillMaxSize()
            .padding(16.dp)
            .verticalScroll(rememberScrollState()),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Text("Hero App - Alu Image Processor", style = MaterialTheme.typography.headlineSmall)
        Spacer(modifier = Modifier.height(16.dp))

        Button(onClick = { photoPickerLauncher.launch("image/*") }) {
            Text(if (state.selectedImageUri != null) "Change Image" else "Select Image")
        }

        Spacer(modifier = Modifier.height(16.dp))

        if (state.selectedImageUri != null) {
            Text("Selected Image URI: ${state.selectedImageUri.toString().takeLast(20)}...")
        }

        Spacer(modifier = Modifier.height(16.dp))

        Button(onClick = { showBottomSheet = true }) {
            Text("Open Editing Sliders")
        }

        if (showBottomSheet) {
            ModalBottomSheet(onDismissRequest = { showBottomSheet = false }) {
                Column(modifier = Modifier.padding(16.dp).verticalScroll(rememberScrollState())) {
                    Text("Edit Photo", style = MaterialTheme.typography.titleLarge)
                    Spacer(modifier = Modifier.height(16.dp))

                    Text("Brightness: ${state.brightness}")
                    Slider(
                        value = state.brightness,
                        onValueChange = { viewModel.updateBrightness(it) },
                        valueRange = -1f..1f
                    )

                    Text("Contrast: ${state.contrast}")
                    Slider(
                        value = state.contrast,
                        onValueChange = { viewModel.updateContrast(it) },
                        valueRange = 0f..2f
                    )

                    Text("Spatial Filtering: ${state.spatialFilter}")
                    Slider(
                        value = state.spatialFilter,
                        onValueChange = { viewModel.updateSpatialFilter(it) },
                        valueRange = -1f..1f
                    )
                    Spacer(modifier = Modifier.height(16.dp))
                }
            }
        }

        Spacer(modifier = Modifier.height(16.dp))

        if (state.galleryImages.isNotEmpty()) {
            Text("Local Gallery", style = MaterialTheme.typography.titleMedium)
            LazyVerticalGrid(
                columns = GridCells.Fixed(3),
                modifier = Modifier.height(200.dp).fillMaxWidth()
            ) {
                items(state.galleryImages) { uri ->
                    AsyncImage(
                        model = uri,
                        contentDescription = null,
                        contentScale = ContentScale.Crop,
                        modifier = Modifier
                            .aspectRatio(1f)
                            .padding(2.dp)
                            .clickable { viewModel.onImageSelected(uri) }
                    )
                }
            }
        }
        
        Spacer(modifier = Modifier.height(16.dp))

        // Toggles
        Row(
            verticalAlignment = Alignment.CenterVertically,
            modifier = Modifier.fillMaxWidth()
        ) {
            Checkbox(
                checked = state.fit,
                onCheckedChange = { viewModel.updateFit(it) }
            )
            Text("Fit (Maintain Aspect Ratio)")
        }

        Row(
            verticalAlignment = Alignment.CenterVertically,
            modifier = Modifier.fillMaxWidth()
        ) {
            Checkbox(
                checked = state.bicubic,
                onCheckedChange = { viewModel.updateBicubic(it) }
            )
            Text("Bicubic Interpolation (vs Bilinear)")
        }

        Spacer(modifier = Modifier.height(16.dp))

        Button(
            onClick = { viewModel.processImage(context) },
            enabled = state.selectedImageUri != null && !state.isProcessing
        ) {
            Text(if (state.isProcessing) "Processing..." else "Process Image")
        }

        if (state.error != null) {
            Spacer(modifier = Modifier.height(8.dp))
            Text("Error: ${state.error}", color = MaterialTheme.colorScheme.error)
        }

        Spacer(modifier = Modifier.height(16.dp))

        if (state.processedBitmap != null) {
            Text("Processed Result:", style = MaterialTheme.typography.titleMedium)
            Spacer(modifier = Modifier.height(8.dp))
            Image(
                bitmap = state.processedBitmap!!.asImageBitmap(),
                contentDescription = "Processed Image",
                modifier = Modifier
                    .fillMaxWidth()
                    .heightIn(max = 400.dp)
            )
        }
    }
}
