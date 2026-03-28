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
    takeFrom = function(item, frame2)
      local_storage = locator(frame, ".Storage")
      storages = frame2:getStorages()
      for i, storage in ipairs(storages) do
        stack = local_storage.api.getStackByItem(item)
        if stack then
          local_storage.api.transferFrom(storage, stack)
        end
      end
    end
    giveTo = function(item, frame2)
      local_storage = locator(frame, ".Storage")
      storages = frame2:getStorages()
      for i, storage in ipairs(storages) do
        stack = local_storage.api.getStackByItem(item)
        if stack then
          local_storage.api.transferTo(storage, stack)
        end
      end
    end

    nfc = locator(frame, ".NFC")
    local_storage = locator(frame, ".Storage")
    if local_storage == nil then
      print("local storage is not found")
      return
    end
    frames = nfc.api.getConnectedFrames(frame)
    for i, frame2 in ipairs(frames) do
      if frame2:hasComponentType("Miner", false) then
        takeFrom("Spark Ore", frame2)
      end
      if frame2:hasComponentType("Refinery", false) then
        giveTo("Spark Ore", frame2)
        takeFrom("Spark Stone", frame2)
      end
      if frame2:hasComponentType("Generator", false) then
        giveTo("Spark Stone", frame2)
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
