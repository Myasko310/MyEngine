# Folder Structure Fix

## Problem
The project had a nested folder structure:
```
MyEngine/               (git root)
  └── MyEngine/         (actual project files)
	  ├── src/
	  ├── CMakeLists.txt
	  └── ...
```

This caused confusion about where files were located and where builds were being output.

## Solution
Flattened the structure by moving all project files from `MyEngine/MyEngine/` to `MyEngine/`:

```
MyEngine/               (git root & project root)
  ├── src/
  ├── CMakeLists.txt
  ├── assets/
  ├── shaders/
  ├── build/
  └── ...
```

## What Was Done
1. Moved `docs/`, `include/`, `tools/`, `vcpkg_installed/` from nested folder to root
2. Removed old/empty `src/`, `shaders/`, `assets/` from root
3. Moved real `src/`, `shaders/`, `assets/` from nested folder to root
4. Moved active `build/` folder with compiled executable
5. Removed the now-empty nested `MyEngine/` folder
6. Copied texture PNGs to source assets folder
7. Staged all changes in git (recognized as rename/move)

## Benefits
- No more confusion about which folder to open in Visual Studio
- Clear single location for all project files
- Assets and build outputs in predictable locations
- CMake will now copy assets correctly on each build

## Next Steps
- Clean build recommended: Delete `build/`, `build_x64/`, `build_vcpkg_x64/` folders
- Reconfigure CMake in Visual Studio
- Build will automatically copy `assets/` and `shaders/` to output directory
