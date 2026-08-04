package com.example.alu

import android.content.Context
import android.os.Environment
import android.util.Log
import androidx.work.CoroutineWorker
import androidx.work.WorkerParameters
import com.example.alu.sdk.AluImageProcessor
import java.io.File

class AegisShieldWorker(appContext: Context, workerParams: WorkerParameters) :
    CoroutineWorker(appContext, workerParams) {

    override suspend fun doWork(): Result {
        Log.i("AegisShieldWorker", "Starting background deep scan...")

        // Scan the Downloads directory
        val downloadDir = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS)
        if (downloadDir.exists() && downloadDir.isDirectory) {
            val files = downloadDir.listFiles()
            if (files != null) {
                for (file in files) {
                    if (file.isFile) {
                        Log.d("AegisShieldWorker", "Scanning: ${file.absolutePath}")
                        val isThreat = AluImageProcessor.scanFile(file.absolutePath)
                        if (isThreat) {
                            Log.w("AegisShieldWorker", "!!! MALWARE DETECTED IN ${file.absolutePath} !!!")
                            // Future: send notification or quarantine file
                        }
                    }
                }
            }
        }
        
        // If a specific package path was passed in (e.g. from BroadcastReceiver)
        val packagePath = inputData.getString("package_path")
        if (packagePath != null) {
            Log.d("AegisShieldWorker", "Targeted scan of package: $packagePath")
            val isThreat = AluImageProcessor.scanFile(packagePath)
            if (isThreat) {
                Log.w("AegisShieldWorker", "!!! THREAT PACKAGE DETECTED: $packagePath !!!")
            }
        }

        Log.i("AegisShieldWorker", "Background deep scan completed.")
        return Result.success()
    }
}
