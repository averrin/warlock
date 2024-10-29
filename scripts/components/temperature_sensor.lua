return {
	name = "Temperature Sensor",
	description = "Temperature Sensor",
	attributes = {
		type = {
			title = "Type",
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
			return component.data.attributes["temp"]:GetFinalValue()
		end,
		getFrameTemperature = function()
			return frame.data.attributes["temp"]:GetFinalValue()
		end,
		getEnvTemperature = function()
			return environment.temperature
		end,
	},
}
