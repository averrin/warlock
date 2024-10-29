bp_names = {
	"Nexus",
	"Smart Battery",
	"Consumer",
	"Consumer",
	"Consumer",
	"Generator",
	"Solar Panel",
	"Miner",
	"Refinery",
	"Cargo Transporter",
}
frames = {}
for i, name in ipairs(bp_names) do
	frames[i] = gm:addFrameFromBlueprint(name)
end

gm:addConnection(frames[1], frames[2])
gm:addConnection(frames[2], frames[3])
gm:addConnection(frames[1], frames[6])
gm:addConnection(frames[4], frames[5])
gm:addConnection(frames[1], frames[8])
gm:addConnection(frames[1], frames[9])
