#pragma once

// Marks an entity (typically the primary camera) as the active audio listener.
// Only one listener should be active (isPrimary = true) at a time; AudioSystem
// picks the first primary listener it finds each frame.
struct AudioListenerComponent
{
	bool isPrimary = true;
	float gain = 1.0f;
};
