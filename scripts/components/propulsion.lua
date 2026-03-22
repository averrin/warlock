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
			return oracle:moveFrame(frame.data.id, direction)
		end,
	},
}
