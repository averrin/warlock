return {
	name = "Near Field Communicator",
	category = "Network",
	description = "Provide data link (and power) for adjusting frames.",
	icon = "wi-fi.png",
	attributes = {
		type = {
			title = "Type",
			inspector = { readonly = true },
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
	spendable_cost = { ["Electronic Parts"] = 10 },
	api = {
		getConnectedFrames = function()
			return frameWorld:nfcFrames(frame.data.id, 0)
		end,
	},
}
