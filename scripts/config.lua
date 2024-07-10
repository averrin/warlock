require("scripts/controls")
require("scripts/gui")

settings = {
	meta_data_files = { "main.meta" },
	proto_files = { "main.proto" },
	init_script = "scripts/init.lua",
	tileset = "boxy",
	seed = 873130520,
	location_type = "MIX",
	margin = 8,

	light = {
		alpha_per_d = 10,
		alpha_blend_inc = 0.5,
		flick_delay = 200,
		blend_mode = "blend",
		max_bright = 220,
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
	info("Key pressed: " .. event.combo)
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
