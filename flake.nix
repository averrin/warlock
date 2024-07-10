{
  description = "Hellfrost development environment";

  inputs.flake-utils.url = "github:numtide/flake-utils";

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem
      (system:
        let pkgs = nixpkgs.legacyPackages.${system}; in
        {
          nixpkgs.config.allowUnfree = true;
          devShells.default = import ./shell.nix { inherit pkgs; };
        }
      );
}
