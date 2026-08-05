# Test Textures for MyEngine

This directory contains procedurally generated test textures for the MyEngine texture system.

## Available Textures

### 1. **checkerboard.png** (512x512)
- Classic black and white checkerboard pattern
- 32x32 pixel squares
- Useful for: Testing texture wrapping, UV mapping, and texture filtering

### 2. **grid.png** (512x512)
- Light gray background with black grid lines
- 64x64 pixel grid spacing
- Useful for: Visualizing texture coordinates and alignment

### 3. **gradient.png** (512x512)
- RGB gradient from corner to corner
- Red increases left-to-right
- Green increases top-to-bottom
- Blue decreases left-to-right
- Useful for: Testing color accuracy and texture interpolation

### 4. **brickwall.png** (512x512)
- Red-brown brick texture with gray mortar
- Offset brick pattern (every other row)
- Procedural variation for realistic look
- Useful for: Testing realistic materials and architectural visualization

### 5. **colors.png** (512x512)
- Four colored quadrants:
  - Top-left: Red
  - Top-right: Green
  - Bottom-left: Blue
  - Bottom-right: Yellow
- Useful for: Quick UV orientation testing

### 6. **uvtest.png** (512x512)
- UV coordinate visualization texture
- Red channel = U coordinate (0 to 1, left to right)
- Green channel = V coordinate (0 to 1, top to bottom)
- Blue channel = 0 (black)
- Useful for: Debugging UV mapping and verifying texture coordinates

## How to Use in MyEngine

1. **Load via Inspector UI:**
   - Select an entity with a mesh renderer
   - In the Inspector panel, find "Mesh Renderer"
   - Enter texture path: `assets/textures/checkerboard.png`
   - Click "Load Texture"
   - Check "Use Texture"

2. **Load via Code:**
   ```cpp
   auto texture = MyEngine::AssetManager::LoadTexture("assets/textures/brickwall.png");
   renderer.texture = texture;
   renderer.useTexture = true;
   ```

## Regenerating Textures

To regenerate these textures or create new ones, run:
```bash
./GenerateTextures.exe
```

The tool source code is in `MyEngine/tools/GenerateTextures.cpp`

## Notes

- All textures are 512x512 pixels
- Saved in PNG format (RGB, no alpha)
- Generated procedurally for consistency
- Can be regenerated at any time without external dependencies
