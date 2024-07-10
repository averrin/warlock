{ pkgs ? import <nixpkgs> { config.allowUnfree = true; } }:
pkgs.mkShell {
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
}
