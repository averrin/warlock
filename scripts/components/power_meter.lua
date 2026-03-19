return {
    name = "Power Meter",
    category = "Power",
    description = "Power Meter",
    attributes = {
        type = {
            title = "Type",
            description = "",
            type = AttributeType.STRING,
            value = "Power Meter"
        },
        passive = {
            title = "Passive",
            description = "Cannot change state",
            type = AttributeType.BOOL,
            value = true
        },
    },
    state = ComponentState.ACTIVE,
    size = ComponentSize.S,
    api = {
      getPowerInfo = function(component)
        return frame:getPowerInfo()
      end,
    },
}
