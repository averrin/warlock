return {
	name = "Propulsion",
	category = "Movement",
	description = "Moves the frame in a given direction",
	icon = "thruster.png",
	attributes = {
		type = {
			title = "Type",
			inspector = { readonly = true },
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
			description = "Steady power draw while active",
			type = AttributeType.FLOAT,
			value = 10.0,
		},
		move_consumption = {
			title = "Move consumption",
			description = "Extra power drawn on each move (spike); move is blocked if the grid cannot supply it",
			type = AttributeType.FLOAT,
			value = 25.0,
		},
	},
	state = ComponentState.DEACTIVATED,
	size = ComponentSize.S,
	spendable_cost = { ["Electronic Parts"] = 10 },
	api = {
		move = function(frame, direction)
			local comp = frame:getComponentByType("Propulsion")
			local spd = 1.0
			local moveCost = 0.0
			if comp then
				local a = comp.data.attributes["speed"]
				if a then
					spd = a:GetFinalValue()
				end
				local mc = comp.data.attributes["move_consumption"]
				if mc then
					moveCost = mc:GetFinalValue()
				end
			end
			if spd < 0.05 then
				spd = 0.05
			end
			if moveCost < 0.0 then
				moveCost = 0.0
			end
			return frameWorld:moveFrame(frame.data.id, direction, frameWorld:subcellStep(), spd, moveCost)
		end,
	},
}
