#pragma once

#include <memory>
#include <vector>

#include "rendering/Mesh.h"

// Level-of-Detail component.
//
// Attach alongside MeshComponent. MeshRendererSystem will pick the highest-
// resolution LOD whose distanceThreshold is >= camera distance (LODs must be
// stored in ascending threshold order). If no LOD matches the mesh on
// MeshComponent is used unchanged.
//
// Example setup (3 LODs):
//   lod.levels = {
//       { 10.0f,  highResMesh  },   // use high-res when closer than 10 units
//       { 40.0f,  midResMesh   },   // use mid-res  when closer than 40 units
//       { 100.0f, lowResMesh   },   // use low-res  when closer than 100 units
//   };
struct LODComponent
{
	struct Level
	{
		float distanceThreshold = 0.0f;                   // max camera distance for this level
		std::shared_ptr<MyEngine::Mesh> mesh = nullptr;   // mesh to use at this level
		std::string assetPath;                            // optional: path for serialization
	};

	std::vector<Level> levels;   // must be sorted ascending by distanceThreshold
	int   activeLevel    = 0;    // written each frame by MeshRendererSystem (read-only for game code)
	bool  enabled        = true;
};
