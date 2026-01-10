@echo off
rem Run from the script directory so relative paths work
pushd "%~dp0" >nul

rem Choose glslc from VULKAN_SDK if available, otherwise fallback
if defined VULKAN_SDK (
    set "GLSLC=%VULKAN_SDK%\Bin\glslc.exe"
) else (
    set "GLSLC=C:\Libraries\VulkanSDK\1.4.321.1\Bin\glslc.exe"
)

if not exist "%GLSLC%" (
    echo Error: glslc not found at "%GLSLC%".
    echo Set the VULKAN_SDK environment variable or adjust the fallback path in this script.
    popd >nul
    pause
    exit /b 1
)

rem Ensure output folder exists
if not exist "shaders\compiled" mkdir "shaders\compiled"

rem Compile all .vert .frag .comp files in shaders
setlocal enabledelayedexpansion
set "FAILED=0"
for %%F in (shaders\*.vert shaders\*.frag shaders\*.comp) do (
    echo Compiling %%F ...
    "%GLSLC%" "%%F" -o "shaders\compiled\%%~nF%%~xF.spv"
    if errorlevel 1 (
        echo.
        echo Error: compilation failed for %%F
        set "FAILED=1"
        goto :finish
    )
)

:finish
if "%FAILED%"=="0" (
    echo.
    echo Shader compilation successfully completed.
) else (
    echo.
    echo Shader compilation stopped due to errors.
)
endlocal

popd >nul
pause