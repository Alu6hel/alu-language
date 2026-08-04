package com.example.alu

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.ui.Modifier
import androidx.work.PeriodicWorkRequestBuilder
import androidx.work.WorkManager
import com.example.alu.theme.AluTheme
import com.example.alu.ui.main.MainScreen
import java.util.concurrent.TimeUnit

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        
        // Schedule deep background scan via WorkManager (e.g., every 24 hours)
        val deepScanWork = PeriodicWorkRequestBuilder<AegisShieldWorker>(24, TimeUnit.HOURS)
            .build()
        WorkManager.getInstance(this).enqueue(deepScanWork)

        setContent {
            AluTheme {
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = MaterialTheme.colorScheme.background
                ) {
                    MainScreen()
                }
            }
        }
    }
}
