return {
	name = "Wireless Data Connector",
	category = "Connectors",
	description = "Provide a small-range wireless data link for adjusting frames.",
	icon = "wi-fi.png",
	attributes = {
		type = {
			title = "Type",
			description = "",
			type = AttributeType.STRING,
			value = "Wireless Data Connector",
		},
		consumption = {
			title = "Consumption",
			description = "Power consumption",
			type = AttributeType.FLOAT,
			value = 25,
		},
		temp_proof = {
			title = "Temperature Proof",
			description = "Can operate in high temperature",
			type = AttributeType.BOOL,
			value = true,
		},
	},
	state = ComponentState.ACTIVE,
	size = ComponentSize.S,
	api = {
		getConnectedFrames = function()
			-- Side-by-side medium frames only: center-to-center distance <= 150.
			frames = oracle:getWirelessDataFrames(frame.data.id, 150.0)
			return frames
		end,
	},
}
