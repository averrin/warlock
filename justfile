bin_name := "hellfrost"

[no-cd, private]
_init:
  rm -rf ./build
  cmake -S . -B ./build -DCMAKE_POLICY_VERSION_MINIMUM=3.5
  -rm compile_commands.json
  ln -s ./build/compile_commands.json .

[no-cd]
_link:
  cd ./build/bin && ln -s ../../tilesets ../../imgui.ini ../../game.bin ../../fonts ../../scripts ../../data .

[no-cd]
init: _init _link

[no-cd]
build:
  rm -rf ./build/bin/{{bin_name}}
  cd ./build && make -j18

[no-cd]
run args="": build
  DISPLAY=:1 ./build/bin/{{bin_name}} {{args}}
