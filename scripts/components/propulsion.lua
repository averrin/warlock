return {
	name = "Propulsion",
	category = "Movement",
	description = "Moves the frame in a given direction",
	icon = "thruster.png",
	attributes = {
		type = {
			title = "Type",
			description = "",
			type = AttributeType.STRING,
			value = "Propulsion",
		},
		speed = {
			title = "Speed",
			description = "How fast each step completes (higher = quicker motion)",
			type = AttributeType.FLOAT,
			value = 1.0,
		},
		consumption = {
			title = "Consumption",
			description = "Power consumption per movement",
			type = AttributeType.FLOAT,
			value = 10.0,
		},
	},
	state = ComponentState.DEACTIVATED,
	size = ComponentSize.S,
	api = {
		move = function(frame, direction)
			local comp = frame:getComponentByType("Propulsion")
			local spd = 1.0
			if comp then
				local a = comp.data.attributes["speed"]
				if a then
					spd = a:GetFinalValue()
				end
			end
			if spd < 0.05 then
				spd = 0.05
			end
			return frameWorld:moveFrame(frame.data.id, direction, frameWorld:subcellStep(), spd)
		end,
	},
}
