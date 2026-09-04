# MyEngine

MyEngine is a C++17 game engine/editor project using CMake, OpenGL, Lua scripting, and an ECS scene model.

## Scripting

### Script Types

MyEngine supports two script execution modes:

- **Entity scripts** (via `ScriptComponent`)
  - Typical callbacks: `OnStart(id)`, `OnUpdate(id, dt)`, `OnEnable(id)`, `OnDisable(id)`
  - Collision callbacks: `OnCollisionEnter(id, otherId)`, `OnCollisionExit(id, otherId)`, `OnTriggerEnter(id, otherId)`, `OnTriggerExit(id, otherId)`
  - Access `self.*` helpers that operate on the bound entity without passing an ID.
- **Global scripts** (configured in the **Scripting** panel)
  - Typical callbacks: `OnGlobalStart()`, `OnGlobalUpdate(dt)`, `OnGlobalEnable()`, `OnGlobalDisable()`
  - Event hooks: `OnEntityCreated(entityID)`, `OnEntityDestroyed(entityID)`, `OnComponentAdded(entityID, componentName)`, `OnComponentRemoved(entityID, componentName)`
  - Access `engine.*` and `scene.*` helpers for cross-entity logic.

### Entity Script `self.*` API

These helpers are pre-bound to the entity that owns the `ScriptComponent`:

| Function | Description |
|---|---|
| `self.get_position()` | Returns `x, y, z` of bound entity position |
| `self.set_position(x, y, z)` | Sets bound entity position |
| `self.translate(dx, dy, dz)` | Moves bound entity by offset |
| `self.get_rotation()` | Returns `x, y, z` Euler angles |
| `self.set_rotation(x, y, z)` | Sets Euler rotation |
| `self.get_scale()` | Returns `x, y, z` scale |
| `self.set_scale(x, y, z)` | Sets scale |
| `self.has_rigidbody()` | Returns `true` if entity has a Rigidbody |
| `self.get_velocity()` | Returns `vx, vy, vz` |
| `self.set_velocity(vx, vy, vz)` | Sets velocity |
| `self.get_gravity_scale()` | Returns gravity scale multiplier |
| `self.set_gravity_scale(value)` | Sets gravity scale |
| `self.get_kinematic()` | Returns `true` if kinematic |
| `self.set_kinematic(bool)` | Enables/disables kinematic mode |
| `self.has_light()` | Returns `true` if entity has a Light |
| `self.get_light_color()` | Returns `r, g, b` |
| `self.set_light_color(r, g, b)` | Sets light colour |
| `self.get_light_intensity()` | Returns intensity |
| `self.set_light_intensity(value)` | Sets intensity |
| `self.get_light_range()` | Returns range |
| `self.set_light_range(value)` | Sets range |
| `self.get_light_cast_shadows()` | Returns `true` if shadows cast |
| `self.set_light_cast_shadows(bool)` | Enables/disables shadow casting |
| `self.get_id()` | Returns the entity integer ID |
| `self.log(msg)` | Prints `[Lua] msg` to stdout |

### Common API (`engine.*` / `scene.*`)

These helpers are available through `engine.*` (and `scene.*` aliases where noted):

- **Entity lookup:**
  - `engine.find_entity_by_name(name)` -- returns entity ID or `nil`
  - `engine.entity_exists(entityID)` -- returns `true`/`false`
  - `engine.get_entity_name(entityID)` -- returns name string or `nil`
- **Transform (by entity ID):**
  - `engine.get_position_of(entityID)` -- returns `x, y, z`
  - `engine.set_position_of(entityID, x, y, z)`
  - `engine.translate_of(entityID, dx, dy, dz)`
- **Rigidbody (by entity ID):**
  - `engine.has_rigidbody(entityID)` -- returns `true`/`false`
  - `engine.get_velocity(entityID)` -- returns `vx, vy, vz`
  - `engine.set_velocity(entityID, vx, vy, vz)`
  - `engine.get_gravity_scale(entityID)` -- returns number
  - `engine.set_gravity_scale(entityID, value)`
  - `engine.get_kinematic(entityID)` -- returns `true`/`false`
  - `engine.set_kinematic(entityID, isKinematic)`
- **Light (by entity ID):**
  - `engine.has_light(entityID)` -- returns `true`/`false`
  - `engine.get_light_color(entityID)` -- returns `r, g, b`
  - `engine.set_light_color(entityID, r, g, b)`
  - `engine.get_light_intensity(entityID)` -- returns number
  - `engine.set_light_intensity(entityID, value)`
  - `engine.get_light_range(entityID)` -- returns number
  - `engine.set_light_range(entityID, value)`
  - `engine.get_light_cast_shadows(entityID)` -- returns `true`/`false`
  - `engine.set_light_cast_shadows(entityID, enabled)`
- **Entity/component management:**
  - `engine.create_entity(name)` / `scene.create_entity(name)` -- returns new entity ID
  - `engine.destroy_entity(entityID)` / `scene.destroy_entity(entityID)`
  - `engine.add_component(entityID, componentName, ...)` / `scene.add_component(...)`
  - `engine.remove_component(entityID, componentName)` / `scene.remove_component(...)`
- **Input:**
  - `engine.is_key_down(keyCode)`, `engine.is_key_pressed(keyCode)`, `engine.is_key_released(keyCode)`
  - `engine.is_mouse_button_down(btn)`, `engine.is_mouse_button_pressed(btn)`, `engine.is_mouse_button_released(btn)`
  - `engine.get_mouse_delta()` -- returns `dx, dy`
  - `engine.get_mouse_wheel()` -- returns `wx, wy`
  - `engine.get_mouse_position()` -- returns `x, y`
  - `engine.is_mouse_captured()` -- returns `true` while camera control is active
- **Logging:** `engine.log(msg)`

Component names supported by `add_component`/`remove_component`:

| Name | Extra args for `add_component` |
|---|---|
| `"transform"` | -- |
| `"rigidbody"` | -- |
| `"light"` | -- |
| `"script"` | `scriptPath` (string), `enabled` (bool), `autoStart` (bool) |

> **Error handling:** Passing wrong argument types or operating on an entity that lacks the required component raises a Lua error. Use guard checks (`engine.entity_exists`, `engine.has_rigidbody`, `engine.has_light`) before calling getters when entity state is uncertain.

### Example: Global script controlling another entity

```lua
function OnGlobalUpdate(dt)
	local player = engine.find_entity_by_name("Player")
	if player and engine.has_rigidbody(player) then
		local vx, vy, vz = engine.get_velocity(player)
		engine.set_velocity(player, vx, vy, vz + 0.25 * dt)
	end
end
```

### Example: Create a runtime helper entity

```lua
local helperID = nil

function OnGlobalStart()
	helperID = scene.create_entity("RuntimeLight")
	scene.add_component(helperID, "transform")
	scene.add_component(helperID, "light")
	engine.set_position_of(helperID, 0.0, 3.0, 0.0)
	engine.set_light_color(helperID, 1.0, 0.95, 0.75)
	engine.set_light_intensity(helperID, 2.0)
end

function OnGlobalDisable()
	if helperID and scene.entity_exists(helperID) then
		scene.destroy_entity(helperID)
		helperID = nil
	end
end
```

### Example: Entity script using `self`

```lua
function OnStart(id)
	engine.log("started for entity " .. tostring(id))
end

function OnUpdate(id, dt)
	if engine.is_key_down(87) then -- W
		self.translate(0.0, 0.0, -5.0 * dt)
	end
	if engine.is_key_down(83) then -- S
		self.translate(0.0, 0.0, 5.0 * dt)
	end
end

function OnCollisionEnter(id, otherId)
	engine.log("Collided with entity " .. tostring(otherId))
end
```

### Example: React to entity lifecycle events (global script)

```lua
function OnEntityCreated(entityID)
	engine.log("New entity: " .. tostring(entityID))
end

function OnEntityDestroyed(entityID)
	engine.log("Destroyed entity: " .. tostring(entityID))
end

function OnComponentAdded(entityID, componentName)
	engine.log("Added " .. componentName .. " to " .. tostring(entityID))
end

function OnComponentRemoved(entityID, componentName)
	engine.log("Removed " .. componentName .. " from " .. tostring(entityID))
end
```

### Global Script Execution Order

Global scripts execute in the order they appear in the **Scripting** panel. Use the **Move Up** / **Move Down** buttons in the editor to change execution order; the order is persisted in the scene file.

### Persistence

Global script entries are saved with scene files (`sceneVersion 3`), including path, enabled/auto-start state, and execution order index.
