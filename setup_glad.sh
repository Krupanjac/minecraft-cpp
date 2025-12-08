#!/bin/bash
# Setup script to generate GLAD files

echo "Setting up GLAD OpenGL loader..."

# Check if Python is available
if ! command -v python3 &> /dev/null; then
    echo "Error: Python 3 is required but not found"
    echo "Please install Python 3 and try again"
    exit 1
fi

# Install glad-generator if not present
pip3 install glad 2>/dev/null || pip install glad 2>/dev/null

# Generate GLAD files
echo "Generating GLAD files for OpenGL 4.5 Core..."
python3 -m glad --generator=c --out-path=external/glad --profile=core --api="gl=4.5" 2>/dev/null || \
python -m glad --generator=c --out-path=external/glad --profile=core --api="gl=4.5"

if [ $? -eq 0 ]; then
    echo "GLAD files generated successfully!"
    echo "Files created in external/glad/"
else
    echo "Error: Failed to generate GLAD files"
    echo ""
    echo "Alternative method:"
    echo "1. Visit https://glad.dav1d.de/"
    echo "2. Select: Language=C/C++, Specification=OpenGL, gl=4.5, Profile=Core"
    echo "3. Click 'Generate'"
    echo "4. Extract the zip to external/glad/"
    exit 1
fi
