return {
	name = "Lidar",
	category = "Sensor",
	description = "Scans adjacent cells for obstacles",
	icon = "lidar.png",
	attributes = {
		type = {
			title = "Type",
			description = "",
			type = AttributeType.STRING,
			value = "Lidar",
		},
		consumption = {
			title = "Consumption",
			description = "Power consumption per scan",
			type = AttributeType.FLOAT,
			value = 2.0,
		},
	},
	state = ComponentState.DEACTIVATED,
	size = ComponentSize.S,
	api = {
		scan = function(frame)
			return frameWorld:scanAdjacent(frame.data.id)
		end,
	},
}
