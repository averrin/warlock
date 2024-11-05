return {
	name = "Capacitor",
	description = "Generic capacitor",
	icon = "battery-100.png",
	attributes = {
		type = {
			title = "Type",
			description = "",
			type = AttributeType.STRING,
			value = "Battery",
		},
		status = {
			title = "Status",
			description = "Operation status: IDLE, CHARGING, DISCHARGING",
			type = AttributeType.STRING,
			value = "IDLE",
		},
		capacity = {
			title = "Capacity",
			description = "Power capacity",
			type = AttributeType.INT,
			value = 2000,
		},
		charge = {
			title = "Charge",
			description = "Stored charge",
			type = AttributeType.INT,
			value = 0,
		},
		discharge = {
			title = "Discharge",
			description = "How many power can give",
			type = AttributeType.INT,
			value = 2000,
		},
		charge_speed = {
			title = "Charge speed",
			description = "How fast can charge",
			type = AttributeType.INT,
			value = 2000,
		},
		stable = {
			title = "Stable",
			description = "Does continue work after power loss",
			type = AttributeType.BOOL,
			value = true,
		},
		heat = {
			title = "Active heat",
			description = "Heat generated while active",
			type = AttributeType.FLOAT,
			value = 0.0,
		},
		charge_heat = {
			title = "Charge heat",
			description = "Heat generated while charging",
			type = AttributeType.FLOAT,
			value = 50,
		},
		discharge_heat = {
			title = "Discharge heat",
			description = "Heat generated while discharging",
			type = AttributeType.FLOAT,
			value = 50,
		},
	},
	state = ComponentState.DEACTIVATED,
	size = ComponentSize.M,
	api = {
		getStatus = function(self)
			return self.data.attributes["status"]:GetFinalValue()
		end,
		getCharge = function(self)
			return self.data.attributes["charge"]:GetFinalValue()
		end,
		getCapacity = function(self)
			return self.data.attributes["capacity"]:GetFinalValue()
		end,
	},
}
