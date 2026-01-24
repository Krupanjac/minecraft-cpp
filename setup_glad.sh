#!/bin/bash

set -e

echo "Setting up GLAD OpenGL loader for Debian..."

# Check python3
if ! command -v python3 >/dev/null 2>&1; then
    echo "❌ Python3 not found. Install it with:"
    echo "sudo apt install python3"
    exit 1
fi

# Check pip
if ! command -v pip3 >/dev/null 2>&1; then
    echo "pip3 not found. Installing python3-pip..."
    sudo apt update
    sudo apt install -y python3-pip python3-venv
i

# Create virtual environment if not exists
if [ ! -d "glad_venv" ]; then
    echo "Creating virtual environment..."
    python3 -m venv glad_venv
fi

# Activate venv
source glad_venv/bin/activate

# Upgrade pip
pip install --upgrade pip

# Install glad
pip install glad

# Create output directory
mkdir -p external/glad

echo "Generating GLAD files for OpenGL 4.5 Core..."
python -m glad \
  --generator=c \
  --profile=core \
  --api="gl=4.5" \
  --out-path=external/glad

echo "✅ GLAD files generated successfully!"
echo "Location: external/glad"

deactivate
