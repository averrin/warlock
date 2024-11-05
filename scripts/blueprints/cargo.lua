return {
	name = "Cargo Transporter",
	size = FrameSize.M,
	components = {
		"Core",
		"Storage",
		"Near Field Communicator",
		"Data Wire Connector",
		"Power Wire Connector",
	},
	code = [[
return {
  start = function()
  end,
  update = function()
    -- wdc = frame:getComponentByType("Data Connector")
    -- print(wdc.api.getConnectedFrames())

    takeFrom = function(item, frame2)
      local_storage = frame:getComponentByType("Storage")
      storages = frame2:getStorages()
      for i, storage in ipairs(storages) do
        stack = storage:getStackByItem(item)
        if stack then
          success = local_storage.storage:transferFrom(storage, stack)
        end
      end
    end
    giveTo = function(item, frame2)
      local_storage = frame:getComponentByType("Storage")
      storages = frame2:getStorages()
      for i, storage in ipairs(storages) do
        stack = local_storage.storage:getStackByItem(item)
        if stack then
          success = local_storage.storage:transferTo(storage, stack)
        end
      end
    end

    nfc = frame:getComponentByType("NFC")
    local_storage = frame:getComponentByType("Storage")
    if local_storage == nil then
      print("local storage is not found")
      return
    end
    frames = nfc.api.getConnectedFrames()
    for i, frame in ipairs(frames) do
      if frame:hasComponentType("Miner", false) then
        takeFrom("Spark Ore", frame)
      end
      if frame:hasComponentType("Refinery", false) then
        giveTo("Spark Ore", frame)
        takeFrom("Spark Stone", frame)
      end
      if frame:hasComponentType("Generator", false) then
        giveTo("Spark Stone", frame)
      end
    end
  end,
  idle = function()
  end,
  stop = function()
  end,
}
  ]],
}
