return {
	name = "Lidar",
	category = "Sensor",
	description = "Scans adjacent cells for obstacles",
	icon = "lidar.png",
	attributes = {
		type = {
			title = "Type",
			inspector = { readonly = true },
			description = "",
			type = AttributeType.STRING,
			value = "Lidar",
		},
		consumption = {
			title = "Consumption",
			description = "Power consumption per scan",
			type = AttributeType.FLOAT,
			value = 100.0,
		},
	},
	state = ComponentState.DEACTIVATED,
	size = ComponentSize.S,
	spendable_cost = { ["Electronic Parts"] = 10 },
	api = {
		scan = function(frame)
			return frameWorld:scanAdjacent(frame.data.id)
		end,
	},
}
