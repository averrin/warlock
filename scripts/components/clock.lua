return {
	name = "Clock",
	description = "Clock",
	icon = "alarm-clock.png",
	attributes = {
		type = {
			title = "Type",
			description = "",
			type = AttributeType.STRING,
			value = "Clock",
		},
		passive = {
			title = "Passive",
			description = "Cannot change state",
			type = AttributeType.BOOL,
			value = true,
		},
	},
	state = ComponentState.ACTIVE,
	size = ComponentSize.S,
	api = {
		getTime = function()
			return environment.minutes
		end,
		getDays = function()
			return environment.days
		end,
	},
}
