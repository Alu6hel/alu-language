package com.example.alu;

public class AluBridge {
    static {
        System.loadLibrary("image_processor");
    }

    public static native int process_image(String inFile, int width, int height, String outFile, int quality, int fit, int bicubic);
    public static native int process_hybrid(String lowFile, String highFile, String outFile, int radius);
    public static native int apply_filter(java.nio.ByteBuffer inRgba, java.nio.ByteBuffer outRgba, int w, int h);
    public static native int scan_file(String filePath);
}
