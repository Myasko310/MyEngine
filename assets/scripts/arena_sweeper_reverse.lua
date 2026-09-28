-- Counter-rotating sweeper arm; faster and opposite to arena_sweeper.lua.
local speed = -1.8

function OnStart(entityId)
	engine_log("arena_sweeper_reverse started for entity " .. tostring(entityId))
end

function OnUpdate(entityId, dt)
	local rx, ry, rz = engine_get_rotation()
	if ry == nil then
		return
	end
	engine_set_rotation(rx, (ry + speed * dt) % (math.pi * 2.0), rz)
end
