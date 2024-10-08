return {
  name = "Charger",
  description = "Battery charger",
  attributes = {
      type = {
          title = "Type",
          description = "",
          type = AttributeType.STRING,
          value = "Charger"
      },
      efficiency = {
          title = "Efficiency",
          description = "Component efficiency",
          type = AttributeType.FLOAT,
          value = 1.0,
      },
      consumption = {
          title = "Consumption",
          description = "Power consumption",
          type = AttributeType.FLOAT,
          value = 75.0,
      },
      charge_speed = {
          title = "Charge Speed",
          description = "Charge speed",
          type = AttributeType.FLOAT,
          value = 10.0,
      },
      target = {
          title = "Target component",
          description = "Component to operate on",
          type = AttributeType.INT,
          value = -1,
      },
      heat = {
          title = "Active heat",
          description = "Heat generated while active",
          type = AttributeType.FLOAT,
          value = 0.01
      },
  },
  state = ComponentState.DEACTIVATED,
  size = ComponentSize.S,
  api = {
    test = "yes",
    setTarget = function(self, battery)
      bid = battery.data.id
      self.data.attributes["target"]:SetBaseValue(bid)
    end,
    getConsumption = function(self)
      return self.data.attributes["consumption"]:GetFinalValue()
    end,
  }
}
