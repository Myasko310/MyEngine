local t = 0.0

function OnStart(entityId)
	engine_log("spin_and_bob started for entity " .. tostring(entityId))
end

function OnUpdate(entityId, dt)
	t = t + dt

	local x, y, z = engine_get_position()
	if x == nil then
		return
	end

	local bob = math.sin(t * 2.0) * 0.4
	local drift = math.cos(t * 0.7) * 0.02

	engine_set_position(x + drift, 0.8 + bob, z)
end
