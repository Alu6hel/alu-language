# Outstanding Tasks

## Goal 4: Platform-Specific UI Hooks
- [x] Android: Expand `jni_bridge.cpp` generator to allow ALU to call into Java APIs (UI queries, Toast).
- [x] iOS: Generate `alu_ios_bridge.m` exposing `UIAlertController` and `UIScreen` values.
- [x] Create native definitions in `std/os/android/ui.alu` and `std/os/ios/ui.alu`.
- [x] Validate cross-compilation pipeline with UI hooks.
