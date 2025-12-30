#!/bin/bash
# Update nebula-appdir with local source changes
# Run this after making changes to sync them to the AppDir

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
APP_DIR="$SCRIPT_DIR/nebula-appdir/nebula-appdir/resources/app"

echo "🚀 Updating Nebula AppDir..."
echo "   Source: $SCRIPT_DIR"
echo "   Target: $APP_DIR"
echo ""

# Check if target exists
if [ ! -d "$APP_DIR" ]; then
    echo "❌ Error: AppDir not found at $APP_DIR"
    exit 1
fi

# Files to sync (main app files)
FILES=(
    "main.js"
    "preload.js"
    "package.json"
    "gpu-config.js"
    "gpu-fallback.js"
    "performance-monitor.js"
    "plugin-manager.js"
    "theme-manager.js"
    "bookmarks.json"
)

# Directories to sync
DIRS=(
    "renderer"
    "themes"
    "assets"
    "plugins"
    "documentation"
)

# Sync individual files
echo "📄 Syncing files..."
for file in "${FILES[@]}"; do
    if [ -f "$SCRIPT_DIR/$file" ]; then
        cp "$SCRIPT_DIR/$file" "$APP_DIR/$file"
        echo "   ✓ $file"
    else
        echo "   ⚠ $file (not found, skipping)"
    fi
done

# Sync directories
echo ""
echo "📁 Syncing directories..."
for dir in "${DIRS[@]}"; do
    if [ -d "$SCRIPT_DIR/$dir" ]; then
        # Use rsync if available, otherwise use cp
        if command -v rsync &> /dev/null; then
            rsync -a --delete "$SCRIPT_DIR/$dir/" "$APP_DIR/$dir/"
        else
            rm -rf "$APP_DIR/$dir"
            cp -r "$SCRIPT_DIR/$dir" "$APP_DIR/$dir"
        fi
        echo "   ✓ $dir/"
    else
        echo "   ⚠ $dir/ (not found, skipping)"
    fi
done

echo ""
echo "✅ AppDir updated successfully!"
echo ""
echo "To run Nebula, use:"
echo "   ./nebula-appdir/run-nebula.sh"
