-- Rotates the entity around its Y axis. Set a negative speed for the reverse sweeper.
local speed = 1.2

function OnStart(entityId)
	engine_log("arena_sweeper started for entity " .. tostring(entityId))
end

function OnUpdate(entityId, dt)
	local rx, ry, rz = engine_get_rotation()
	if ry == nil then
		return
	end
	engine_set_rotation(rx, (ry + speed * dt) % (math.pi * 2.0), rz)
end
