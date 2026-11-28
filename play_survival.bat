@echo off
cd /d "%~dp0"
if not exist "build\sage.exe" (
    echo [SAGE] build\sage.exe not found! Build SAGE first with: cmake --build build --target sage
    pause
    exit /b 1
)
"build\sage.exe" --survival %*
