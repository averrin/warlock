return {
	name = "Storage",
	description = "Minor Storage",
	attributes = {
		type = {
			title = "Type",
			value = "Storage",
		},
		slots = {
			title = "Slots",
			description = "Number of slots",
			type = AttributeType.INT,
			value = 1,
		},
		passive = {
			title = "Passive",
			description = "Passive storage",
			type = AttributeType.BOOL,
			value = true,
		},
	},
	state = ComponentState.ACTIVE,
	size = ComponentSize.M,
	api = {},
}
