{
  description = "Hellfrost development environment";

  inputs = {
    nixgl.url = "github:nix-community/nixGL";
  };

  inputs.flake-utils.url = "github:numtide/flake-utils";

  outputs =
    { self, nixpkgs, flake-utils, nixgl } @ inputs:
    let overlays = [ nixgl.overlay ];
    in
    flake-utils.lib.eachDefaultSystem
      (system:
      let pkgs = import nixpkgs { inherit overlays system; };
      in rec
      {
        packages.${system}.default =
          { };
        devShells.default =
          pkgs.mkShell {
            packages = [
              nixgl.packages.${system}.nixGLDefault
            ];
            nativeBuildInputs = with pkgs; [
              pkgs.nixgl.auto.nixGLDefault
            ];
            buildInputs = (with pkgs; [
              git
              gnumake
              gcc
              cmake
              just
              gdb
              udev.dev
              alsa-lib
              freetype.dev
              boost.dev
              elfutils.dev
              libGL.dev
              libGLU
              libGLU.dev
            ]) ++ (with pkgs.xorg; [
              libX11
              libX11.dev
              libXcursor.dev
              libXrandr.dev
              libXrender.dev
              xorgproto
            ]);
          };
      }
      );
}
