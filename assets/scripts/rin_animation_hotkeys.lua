local defaultBlendDuration = 0.15
local lastClipIndex = -1

local keyCodes = {
	49, -- 1
	50, -- 2
	51, -- 3
	52, -- 4
	53, -- 5
	54, -- 6
	55, -- 7
	56, -- 8
	57  -- 9
}

function OnStart(entityId)
	if not self.has_animation() then
		engine_log("rin_animation_hotkeys: entity " .. tostring(entityId) .. " has no AnimationComponent with clips")
		return
	end

	local clipCount = self.get_animation_clip_count()
	engine_log("rin_animation_hotkeys ready for entity " .. tostring(entityId) .. " with " .. tostring(clipCount) .. " clip(s)")
	for clipIndex = 0, clipCount - 1 do
		local clipName = self.get_animation_clip_name(clipIndex)
		engine_log("rin_animation_hotkeys: clip " .. tostring(clipIndex + 1) .. " = " .. tostring(clipName))
	end
	lastClipIndex = self.get_active_animation_clip()
end

function OnUpdate(entityId, dt)
	if not self.has_animation() then
		return
	end

	local clipCount = self.get_animation_clip_count()
	if clipCount <= 0 then
		return
	end

	for i = 1, #keyCodes do
		if engine.is_key_pressed(keyCodes[i]) then
			local clipIndex = i - 1
			if clipIndex < clipCount and clipIndex ~= lastClipIndex then
				local ok = self.play_animation_clip(clipIndex, defaultBlendDuration)
				if ok then
					lastClipIndex = clipIndex
					engine_log("rin_animation_hotkeys: switched to clip " .. tostring(clipIndex + 1))
				end
			end
		end
	end
end
