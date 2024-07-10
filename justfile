bin_name := "hellfrost"

[no-cd, private]
_init:
  rm -rf ./build
  cmake -S . -B ./build
  -rm compile_commands.json
  ln -s ./build/compile_commands.json .

[no-cd]
init: _init
  cd ./build/bin && ln -s ../../tilesets ../../imgui.ini ../../game.bin ../../fonts ../../scripts .

[no-cd]
build:
  rm -rf ./build/bin/{{bin_name}}
  cd ./build && make -j18

[no-cd]
run args="": build
  ./build/bin/{{bin_name}} {{args}}
