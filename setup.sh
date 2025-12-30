#!/bin/bash

# Nebula Browser Setup Script
# This script installs dependencies and fixes Electron sandbox permissions
# Works on Steam Deck and other Linux systems without sudo

echo "========================================="
echo "  Nebula Browser Setup Script"
echo "========================================="
echo ""

# Navigate to the project directory
cd "$(dirname "$0")"

# Run npm install
echo "[1/2] Installing dependencies..."
npm install

if [ $? -ne 0 ]; then
    echo "❌ npm install failed!"
    exit 1
fi

echo ""
echo "[2/2] Fixing Electron sandbox permissions..."
echo "This requires root access. You may be prompted for your password."
echo ""

# Fix chrome-sandbox permissions
SANDBOX_PATH="$(pwd)/node_modules/electron/dist/chrome-sandbox"

if [ ! -f "$SANDBOX_PATH" ]; then
    echo "❌ chrome-sandbox not found at $SANDBOX_PATH"
    echo "   Make sure npm install completed successfully."
    exit 1
fi

# Function to run command as root
run_as_root() {
    if command -v sudo &> /dev/null; then
        sudo "$@"
    elif command -v pkexec &> /dev/null; then
        pkexec "$@"
    elif command -v doas &> /dev/null; then
        doas "$@"
    else
        echo "No privilege escalation tool found (sudo/pkexec/doas)"
        return 1
    fi
}

# Try to fix permissions
echo "Attempting to set sandbox permissions..."
run_as_root chown root:root "$SANDBOX_PATH"
CHOWN_RESULT=$?

run_as_root chmod 4755 "$SANDBOX_PATH"
CHMOD_RESULT=$?

if [ $CHOWN_RESULT -eq 0 ] && [ $CHMOD_RESULT -eq 0 ]; then
    echo "✅ Sandbox permissions fixed successfully!"
    echo ""
    echo "========================================="
    echo "  Setup complete! Run 'npm start' to launch Nebula"
    echo "========================================="
    echo ""
    echo "💡 TIP: For GPU acceleration on Linux, run:"
    echo "   NEBULA_GPU_ALLOW_LINUX=1 npm start"
    echo "========================================="
else
    echo "❌ Failed to set sandbox permissions automatically."
    echo ""
    echo "On Steam Deck, open Konsole and run:"
    echo "   pkexec bash -c 'chown root:root $SANDBOX_PATH && chmod 4755 $SANDBOX_PATH'"
    echo ""
    echo "Or switch to desktop mode and run as root:"
    echo "   su -c 'chown root:root $SANDBOX_PATH && chmod 4755 $SANDBOX_PATH'"
    exit 1
fi
