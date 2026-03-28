require("scripts/controls")
require("scripts/gui")
require("scripts/editor")
require("scripts/draw")

settings = {
	meta_data_files = { "data/main.meta" },
	proto_files = { "data/frame.proto" },
	init_script = "scripts/init.lua",
	init_states = { "data/init.state" },
	current_state = "save/current.state",
	spendable_pool = {
		["Frame Parts"]      = 1000,
		["Ultralight Structures"]       = 1000,
		["Electronic Parts"] = 1000,
		["Science Packs"]    = 1000,
		["Repair Packs"]     = 1000,
		["Advanced Chips"]    = 1000,
		["Next Gen Composits"]     = 1000,
		["Holographic Chips"]     = 1000,
	},
}

luaJob = function()
	info("Lua job started")
end

emitter.connect("init", function(event)
	info("Init component: " .. event.component)
	if event.component == "job_manager" then
		-- TODO: fix this
		-- jobs:add("Lua job", luaJob, true)
	end
end)

emitter.connect("key", function(event)
	var("Pressed", event.combo)
end)

emitter.connect("job_start", function(event)
	info("Job started: " .. event.job.title)
end)

emitter.connect("job_complete", function(event)
	info("Job completed: " .. event.job.title)
end)

emitter.connect("job_error", function(event)
	error("Job [" .. event.job.title .. "] error: " .. event.job.error)
end)

emitter.connect("job_update", function(event)
	info("Job [" .. event.job.title .. "] progress: " .. event.job.progress .. "%")
end)

emitter.publish("init", { component = "config.lua" })
return settings
