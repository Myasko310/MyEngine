# Texture Debugging Guide

## Issue: Textures Appearing Black

If textures appear black on objects, here's how to debug:

### 1. Check Console Output

When you load a texture, you should see:
```
[Texture] Loaded texture: assets/textures/checkerboard.png (512x512, 3 channels)
[MeshRendererSystem] Texture bound: ID=X, Path=assets/textures/checkerboard.png
```

If you see "Failed to load texture", check the path.

### 2. Verify Texture Paths

The engine runs from the build output directory. Textures must be relative to that directory.

**Correct paths:**
- `assets/textures/checkerboard.png`
- `../assets/textures/checkerboard.png` (if assets is in parent dir)

**Check where assets folder is:**
```powershell
Get-ChildItem -Recurse -Filter "checkerboard.png"
```

### 3. Common Issues

#### A. Wrong Working Directory
The engine might be running from a different directory than expected.

**Solution:** Copy the `assets` folder to your build output directory:
```powershell
Copy-Item -Recurse -Force assets out/build/x64-debug/assets
```

#### B. Texture Not Loaded
Check if the texture pointer is null or if `useTexture` is false.

#### C. UV Coordinates Wrong
The cube primitives should have UVs. Check if older entities don't have UVs.

**Solution:** Delete old entities and create new cubes after the texture system was added.

#### D. Shader Uniform Not Set
Ensure the shader has the correct uniforms: `u_UseTexture`, `u_Texture`

### 4. Quick Test

Try this in the engine console (if available) or via Inspector UI:

1. Select "Cube" entity
2. Load texture: `assets/textures/colors.png`
3. Enable "Use Texture"
4. If you see 4 colored squares (R/G/B/Y), UVs are working
5. If solid color, UVs might be wrong
6. If black, texture didn't load

### 5. Manual Path Fix

If textures are in the wrong location, move them:

```powershell
# From MyEngine root
$buildDir = "out/build/x64-debug"  # Adjust for your build config
if (!(Test-Path "$buildDir/assets")) {
	Copy-Item -Recurse assets "$buildDir/assets"
}
```

### 6. Verify OpenGL State

The rendering pipeline should:
1. Bind shader (`glUseProgram`)
2. Set uniforms
3. Activate texture unit (`glActiv eTexture(GL_TEXTURE0)`)
4. Bind texture (`glBindTexture`)
5. Draw mesh

This order is correct in `MeshRendererSystem.cpp`.
