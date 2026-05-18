@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
cd /d "%SCRIPT_DIR%"

set "DXC_EXE=D:\vulkan\Bin\dxc.exe"
if not exist "%DXC_EXE%" (
    set "DXC_EXE=%VULKAN_SDK%\Bin\dxc.exe"
)
if not exist "%DXC_EXE%" (
    set "DXC_EXE=dxc"
)

echo Using DXC: %DXC_EXE%
echo Compiling shaders...

echo.
echo [1/4] Compiling vertex shader for Vulkan (SPIR-V)...
"%DXC_EXE%" -spirv -T vs_6_0 -E main_vs -fspv-target-env=vulkan1.3 -Fo vert.spv vert.hlsl
if errorlevel 1 (
    echo Failed to compile vertex shader for Vulkan.
    pause
    exit /b 1
)

echo [2/4] Compiling fragment shader for Vulkan (SPIR-V)...
"%DXC_EXE%" -spirv -T ps_6_0 -E main_ps -fspv-target-env=vulkan1.3 -Fo frag.spv frag.hlsl
if errorlevel 1 (
    echo Failed to compile fragment shader for Vulkan.
    pause
    exit /b 1
)

echo.
echo [3/4] Compiling vertex shader for DX12 (DXIL)...
"%DXC_EXE%" -T vs_6_0 -E main_vs -Fo vert.dxil vert.hlsl
if errorlevel 1 (
    echo Failed to compile vertex shader for DX12.
    pause
    exit /b 1
)

echo [4/4] Compiling fragment shader for DX12 (DXIL)...
"%DXC_EXE%" -T ps_6_0 -E main_ps -Fo frag.dxil frag.hlsl
if errorlevel 1 (
    echo Failed to compile fragment shader for DX12.
    pause
    exit /b 1
)

echo.
echo ========================================
echo All shaders compiled successfully.
echo   Vulkan: vert.spv, frag.spv
echo   DX12:   vert.dxil, frag.dxil
echo ========================================
pause
