#!/bin/bash
# libs/setup_python_env.sh
# Sets up a virtual environment and installs required dependencies for analysis scripts.

set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VENV_DIR="$PROJECT_ROOT/.venv"

echo ">> Setting up Python environment in $VENV_DIR..."

if [ ! -d "$VENV_DIR" ]; then
    python3 -m venv "$VENV_DIR"
    echo ">> Virtual environment created."
fi

# Activate venv
source "$VENV_DIR/bin/activate"

# Install dependencies
echo ">> Installing dependencies..."
pip install --upgrade pip
pip install tree-sitter==0.20.1

echo ">> Setup complete. To use analysis scripts:"
echo "   source .venv/bin/activate"
echo "   python3 scripts/structure.py"
