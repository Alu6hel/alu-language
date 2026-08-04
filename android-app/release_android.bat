@echo off
echo ====================================================
echo [ALU PACKAGER] Android Release Packager V1.0
echo ====================================================

echo [ALU PACKAGER] Generating Release Android App Bundle (.AAB)...
call gradlew :app:bundleRelease

echo [ALU PACKAGER] Generating and Publishing AAR to Maven Local...
call gradlew :alu-sdk:publishToMavenLocal

echo [SUCCESS] Successfully packaged Android App Bundle and published AAR!
