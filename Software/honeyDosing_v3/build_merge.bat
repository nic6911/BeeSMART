@echo off
REM === CONFIGURATION ===
set DATA_DIR=data
set BUILD_DIR=build
set FS_IMAGE=%BUILD_DIR%\littlefs.bin
set FS_SIZE=0x160000
set BLOCK_SIZE=4096
set PAGE_SIZE=256

REM === TOOL PATH ===
set MKLITTLEFS=C:\Users\rando\AppData\Local\Arduino15\packages\esp32\tools\mklittlefs\3.0.0-gnu12-dc7f933\mklittlefs.exe

REM === Ensure folders exist ===
if not exist %DATA_DIR% (
    echo ❌ Data folder not found: %DATA_DIR%
    pause
    exit /b
)

if not exist %BUILD_DIR% (
    echo 📁 Creating build folder...
    mkdir %BUILD_DIR%
)

REM === Generate LittleFS image ===
echo 🛠️ Generating LittleFS image...
"%MKLITTLEFS%" -c %DATA_DIR% -b %BLOCK_SIZE% -p %PAGE_SIZE% -s %FS_SIZE% %FS_IMAGE%

REM === Done ===
if exist %FS_IMAGE% (
    echo ✅ LittleFS image created: %FS_IMAGE%
) else (
    echo ❌ Failed to create LittleFS image.
)

pause