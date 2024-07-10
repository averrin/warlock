node = {}
function node:init()
  debug("Script initialized", node.id)
end
function node:create()
  debug("Script created")
end
function node:interact()
  debug("Script interact")
end
function node:destroy()
  debug("Script destroy")
end

function node:update(delta)
  -- node.counter = node.counter + 1
  -- print("Script update: " .. node.counter)
  -- print("Script update: " .. string.format("%.2f", delta))
end
return node


