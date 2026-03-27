return {
	name = "Temperature Sensor",
	category = "Thermal",
	description = "Temperature Sensor",
	attributes = {
		type = {
			title = "Type",
			inspector = { readonly = true },
			description = "",
			type = AttributeType.STRING,
			value = "Temperature Sensor",
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
		getComponentTemperature = function(component)
			return attr(component, "temp")
		end,
		getFrameTemperature = function()
			return attr(frame, "temp")
		end,
		getEnvTemperature = function()
			return environment.temperature
		end,
	},
}
