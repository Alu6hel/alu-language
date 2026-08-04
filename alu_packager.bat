@echo off
echo ====================================================
echo [ALU PACKAGER] Android Mobile Shield Packager V1.0
echo ====================================================

if "%~1"=="" (
    echo Usage: alu_packager.bat ^<source_file.alu^>
    exit /b 1
)

set SOURCE_FILE=%~1
set BASE_NAME=%~n1

echo [ALU PACKAGER] Compiling %SOURCE_FILE% for Android target...
.\alu.exe --target-android %SOURCE_FILE%

if errorlevel 1 (
    echo [ERROR] Compilation failed.
    exit /b 1
)

echo [ALU PACKAGER] Creating APK directory structure...
if exist build_apk rmdir /s /q build_apk
mkdir build_apk\lib\armeabi-v7a
mkdir build_apk\META-INF

echo [ALU PACKAGER] Moving compiled .so to APK lib directory...
move %BASE_NAME%.so build_apk\lib\armeabi-v7a\lib%BASE_NAME%.so >nul

echo [ALU PACKAGER] Zipping APK...
powershell Compress-Archive -Path build_apk\* -DestinationPath %BASE_NAME%.zip -Force
move /Y %BASE_NAME%.zip %BASE_NAME%.apk >nul

echo [ALU PACKAGER] Cleaning up build artifacts...
rmdir /s /q build_apk
del %BASE_NAME%.alu.ll

echo ====================================================
echo [SUCCESS] Successfully packaged %BASE_NAME%.apk!
echo ====================================================
