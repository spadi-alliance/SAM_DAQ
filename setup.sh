#!/bin/bash

# SAM DAQ Setup Script
# This script sets up the conda environment and builds the project

set -e  # Exit on any error

echo "🚀 Setting up SAM DAQ Development Environment"
echo "=============================================="

# Check if conda is available
if ! command -v conda &> /dev/null; then
    echo "❌ Error: conda not found. Please install Miniforge or Anaconda first."
    echo "   Download from: https://github.com/conda-forge/miniforge"
    exit 1
fi

# Check if environment file exists
if [ ! -f "environment-minimal.yml" ]; then
    echo "❌ Error: environment-minimal.yml not found."
    echo "   Make sure you're in the project root directory."
    exit 1
fi

# Create conda environment
echo "📦 Creating conda environment 'SAMDAQ'..."
if conda env list | grep -q "SAMDAQ"; then
    echo "⚠️  Environment 'SAMDAQ' already exists. Skipping creation."
else
    conda env create -f environment-minimal.yml
    echo "✅ Environment created successfully!"
fi

# Activate environment check
echo ""
echo "🔧 Build Instructions:"
echo "====================="
echo "1. Activate the environment:"
echo "   conda activate SAMDAQ"
echo ""
echo "2. Create build directory and build:"
echo "   mkdir -p build && cd build"
echo "   cmake .."
echo "   make -j\$(nproc)"
echo ""
echo "3. Run the application:"
echo "   ./SAMDAQ"
echo ""
echo "🎉 Setup complete! Follow the instructions above to build and run."

# Optional: Auto-activate if in supported shell
if [[ $- == *i* ]]; then
    echo ""
    read -p "Would you like to activate the environment now? (y/n): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo "Activating SAMDAQ environment..."
        conda activate SAMDAQ
    fi
fi
