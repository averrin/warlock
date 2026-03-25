return {
	name = "Load Limiter",
	category = "Power",
	description = "Load Limiter",
	attributes = {
		type = {
			title = "Type",
			inspector = { readonly = true },
			description = "",
			type = AttributeType.STRING,
			value = "Load Limiter",
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
		setLoad = function(component, value)
			if value < 0 then
				value = 0
			elseif value > 1 then
				value = 1
			end
			component.data.attributes["load"]:SetBaseValue(value)
		end,
	},
}
