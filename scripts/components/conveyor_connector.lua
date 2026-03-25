return {
	name = "Conveyor Connector",
	category = "Connectors",
	description = "Connects frames via conveyor belt for item transport.",
	attributes = {
		type = {
			title = "Type",
			inspector = { readonly = true },
			description = "",
			type = AttributeType.STRING,
			value = "Conveyor Connector",
		},
		max_connections = {
			title = "Max Connections",
			description = "Maximum number of connections",
			type = AttributeType.INT,
			value = 1,
		},
		max_connection_distance = {
			title = "Connection radius",
			description = "Max center-to-center distance to connect (world units).",
			type = AttributeType.FLOAT,
			value = 500.0,
		},
		stable = {
			title = "Stable",
			description = "Does continue work after power loss",
			type = AttributeType.BOOL,
			value = true,
		},
		temp_proof = {
			title = "Temp Proof",
			description = "Does not overheat or freeze",
			type = AttributeType.BOOL,
			value = true,
		},
		mode = {
			title = "Mode",
			description = "SEND or RECEIVE",
			type = AttributeType.STRING,
			value = "SEND",
			inspector = { widget = "select", options = { "SEND", "RECEIVE" } },
		},
		throughput = {
			title = "Throughput",
			description = "Items per second",
			type = AttributeType.FLOAT,
			value = 1.0,
		},
		target = {
			title = "Target component",
			description = "Component to operate on",
			type = AttributeType.INT,
			value = -1,
			inspector = { widget = "link", link_scope = "frame", link_filter = "Storage" },
		},
		filter = {
			title = "Filter",
			description = "Item name to filter (optional)",
			type = AttributeType.STRING,
			value = "",
		},
	},
	state = ComponentState.ACTIVE,
	size = ComponentSize.S,
	require = { "Storage" },
	api = {
		setTarget = function(self, storage)
			sid = storage.data.id
			self.data.attributes["target"]:SetBaseValue(sid)
		end,
		setMode = function(self, mode)
			if mode == "SEND" or mode == "RECEIVE" then
				self.data.attributes["mode"]:SetBaseValue(mode)
			end
		end,
		setFilter = function(self, filter)
			self.data.attributes["filter"]:SetBaseValue(filter)
		end,
	},
}
