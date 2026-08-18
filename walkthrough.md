# ALU Platform-Specific UI Hooks Implementation

The goal was to enable seamless interactions with the host OS UI layer for cross-platform targets (Android and iOS) from within the ALU language, avoiding heavy reflection while maintaining high usability.

## What was Changed

1. **JNI Generator (`cpp_frontend/main.cpp`)**
    - Rewrote the `ANativeActivity_onCreate` handler emitted by the ALU compiler to explicitly cache the `JavaVM` and a global reference to the active `ANativeActivity` (`g_activity`).
    - Injected C-bindings directly into the `jni_bridge.cpp` generation:
        - `alu_os_get_screen_width()`: Attaches to the thread, retrieves `DisplayMetrics`, and queries pixel widths from Android framework.
        - `alu_os_show_toast(char* msg)`: Interacts with the `android.widget.Toast` Java class.

2. **iOS XCFramework Generator (`cpp_frontend/main.cpp`)**
    - Enhanced the `--create-xcframework` build step to generate an `alu_ios_bridge.m` file containing Objective-C implementations of the standard hooks.
    - `alu_os_get_screen_width()`: Bridges to `UIScreen.mainScreen`.
    - `alu_os_show_toast()`: Uses an asynchronous, timed-dismissal `UIAlertController` directly on the `rootViewController`.

3. **Standard Library Additions**
    - Created `std/os/android/ui.alu` and `std/os/ios/ui.alu` acting as standard library headers that export these `extern` routines to ALU code.

## Verification

We wrote a sample script (`test_ui.alu`) importing the OS hooks and successfully:
- Compiled for `aarch64-linux-android` (`--target=android`). Confirmed generation of `jni_bridge.cpp` properly hooking `main()`.
- Compiled the iOS Simulator and Device targets (`--create-xcframework`), confirming `alu_ios_bridge.m` generates safely.

All cross-platform logic is now safely decoupled and automated in the ALU compiler.
