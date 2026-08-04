package com.example.alu

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.util.Log
import androidx.work.Data
import androidx.work.OneTimeWorkRequestBuilder
import androidx.work.WorkManager

class AegisPackageReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        if (intent.action == Intent.ACTION_PACKAGE_ADDED || intent.action == Intent.ACTION_PACKAGE_REPLACED) {
            val packageName = intent.data?.schemeSpecificPart
            Log.d("AegisPackageReceiver", "Package installed/updated: $packageName")
            
            // To scan the actual APK, we would need to get the sourceDir from PackageManager.
            // For now, we just pass the packageName string to the WorkManager job.
            
            val workData = Data.Builder()
                .putString("package_path", packageName) // Dummy path or app ID
                .build()
                
            val scanWork = OneTimeWorkRequestBuilder<AegisShieldWorker>()
                .setInputData(workData)
                .build()
                
            WorkManager.getInstance(context).enqueue(scanWork)
            Log.i("AegisPackageReceiver", "Enqueued OneTimeWorkRequest for targeted scan.")
        }
    }
}
