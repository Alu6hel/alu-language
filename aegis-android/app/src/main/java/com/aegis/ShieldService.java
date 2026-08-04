package com.aegis;

import android.app.Service;
import android.content.Intent;
import android.os.FileObserver;
import android.os.IBinder;
import android.util.Log;

public class ShieldService extends Service {

    private static final String TAG = "AegisShield";
    private FileObserver fileObserver;

    // Load the native Alu library we compiled (libaegis.so)
    static {
        System.loadLibrary("aegis");
    }

    // Native JNI Method exported by our Alu code
    private native int scanFile(String filePath);

    @Override
    public void onCreate() {
        super.onCreate();
        Log.i(TAG, "Aegis Shield Service Initializing...");

        // Watch the Downloads directory for any newly written files (e.g. dropped payloads)
        String targetPath = "/sdcard/Download/";
        fileObserver = new FileObserver(targetPath, FileObserver.CLOSE_WRITE) {
            @Override
            public void onEvent(int event, String path) {
                if (path != null) {
                    String fullPath = targetPath + path;
                    Log.i(TAG, "New file dropped: " + fullPath);
                    
                    // Route to Alu's unhackable threat scanner
                    int threatLevel = scanFile(fullPath);
                    
                    if (threatLevel > 0) {
                        Log.w(TAG, "Aegis: Malware Threat Neutralized at " + fullPath);
                        // In a full implementation, we'd delete the file and notify the OS.
                    } else {
                        Log.i(TAG, "Aegis: File is clean.");
                    }
                }
            }
        };
        fileObserver.startWatching();
        Log.i(TAG, "Aegis Shield FileObserver active.");
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        if (fileObserver != null) {
            fileObserver.stopWatching();
        }
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
}
