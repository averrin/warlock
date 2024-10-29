return {
	name = "Close Range Communicator",
	description = "Close Range Communicator",
	attributes = {
		type = {
			title = "Type",
			description = "",
			type = AttributeType.STRING,
			value = "Temperature Sensor",
		},
		consumption = {
			title = "Consumption",
			description = "Power consumption",
			type = AttributeType.FLOAT,
			value = 100,
		},
	},
	state = ComponentState.DEACTIVATED,
	size = ComponentSize.S,
	api = {
		getNearFrames = function() end,
	},
}
