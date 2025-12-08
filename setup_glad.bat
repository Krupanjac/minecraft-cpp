@echo off
REM Setup script to generate GLAD files for Windows

echo Setting up GLAD OpenGL loader...

REM Check if Python is available
python --version >nul 2>&1
if errorlevel 1 (
    echo Error: Python is required but not found
    echo Please install Python and try again
    exit /b 1
)

REM Install glad-generator if not present
pip install glad >nul 2>&1

REM Generate GLAD files
echo Generating GLAD files for OpenGL 4.5 Core...
python -m glad --generator=c --out-path=external/glad --profile=core --api="gl=4.5"

if %errorlevel% equ 0 (
    echo GLAD files generated successfully!
    echo Files created in external/glad/
) else (
    echo Error: Failed to generate GLAD files
    echo.
    echo Alternative method:
    echo 1. Visit https://glad.dav1d.de/
    echo 2. Select: Language=C/C++, Specification=OpenGL, gl=4.5, Profile=Core
    echo 3. Click 'Generate'
    echo 4. Extract the zip to external/glad/
    exit /b 1
)
