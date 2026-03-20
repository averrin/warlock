return {
	name = "Near Field Communicator",
	category = "Network",
	description = "Provide data link (and power) for adjusting frames.",
	icon = "wi-fi.png",
	attributes = {
		type = {
			title = "Type",
			description = "",
			type = AttributeType.STRING,
			value = "NFC",
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
			frames = oracle:getNFCFrames(frame.data.id, 1.0)
			return frames
		end,
	},
}
