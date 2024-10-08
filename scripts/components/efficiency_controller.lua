return {
    name = "Efficiency Limiter",
    description = "Efficiency Limiter",
    attributes = {
        type = {
            title = "Type",
            description = "",
            type = AttributeType.STRING,
            value = "Efficiency Limiter"
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
      setEfficiency = function(component, value)
        if value < 0 then
          value = 0
        elseif value > 1 then
          value = 1
        end
        component.data.attributes["efficiency"]:SetBaseValue(value)
      end,
    },
}
