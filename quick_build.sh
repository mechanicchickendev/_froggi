#!/bin/bash
PROJECT_ROOT=~/Documents/_froggi
BUILD_DIR="$PROJECT_ROOT/build"

cd "$BUILD_DIR" || exit 1

echo "Quick rebuilding..."
cmake --build . || exit 1

echo "Copying resources..."

# Copy engine shaders
if [ -d "$PROJECT_ROOT/engine/shaders" ]; then
    mkdir -p "$BUILD_DIR/engine/shaders"
    cp -r "$PROJECT_ROOT/engine/shaders/"* "$BUILD_DIR/engine/shaders/" 2>/dev/null || true
    echo "Engine shaders copied"
fi

# Copy game shaders (if you have any)
if [ -d "$PROJECT_ROOT/games/sample/shaders" ]; then
    mkdir -p "$BUILD_DIR/games/sample/shaders"
    cp -r "$PROJECT_ROOT/games/sample/shaders/"* "$BUILD_DIR/games/fig/shaders/" 2>/dev/null || true
    echo "Game shaders copied"
fi

# Copy assets (models, textures, etc.)
if [ -d "$PROJECT_ROOT/games/sample/assets" ]; then
    mkdir -p "$BUILD_DIR/games/sample/assets"
    # Use the correct source path and preserve directory structure
    rsync -a --delete "$PROJECT_ROOT/games/sample/assets/" "$BUILD_DIR/games/sample/assets/" 2>/dev/null || \
    cp -r "$PROJECT_ROOT/games/sample/assets/"* "$BUILD_DIR/games/sample/assets/" 2>/dev/null || true
    echo "Assets copied"
fi

echo "Running Game..."
cd games/sample
./sample
