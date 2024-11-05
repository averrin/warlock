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
	"Smart Battery",
}
frames = {}
for i, name in ipairs(bp_names) do
	frames[i] = gm:addFrameFromBlueprint(name)
end

gm:addConnection(frames[1], frames[2], ConnectionType.POWER)
gm:addConnection(frames[2], frames[3], ConnectionType.POWER)
gm:addConnection(frames[1], frames[6], ConnectionType.POWER)
gm:addConnection(frames[4], frames[5], ConnectionType.POWER)
gm:addConnection(frames[1], frames[8], ConnectionType.POWER)
gm:addConnection(frames[1], frames[9], ConnectionType.POWER)
gm:addConnection(frames[1], frames[10], ConnectionType.POWER)

gm:addConnection(frames[1], frames[10], ConnectionType.DATA)
