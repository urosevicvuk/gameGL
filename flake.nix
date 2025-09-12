{
  description = "A Nix-flake-based C/C++ development environment";

  inputs.nixpkgs.url = "https://flakehub.com/f/NixOS/nixpkgs/0.1";

  outputs =
    inputs:
    let
      supportedSystems = [
        "x86_64-linux"
        "aarch64-linux"
        "x86_64-darwin"
        "aarch64-darwin"
      ];
      forEachSupportedSystem =
        f:
        inputs.nixpkgs.lib.genAttrs supportedSystems (
          system:
          f {
            pkgs = import inputs.nixpkgs { inherit system; };
          }
        );
    in
    {
      devShells = forEachSupportedSystem (
        { pkgs }:
        {
          default =
            pkgs.mkShell.override
              {
                # Override stdenv in order to change compiler:
                # stdenv = pkgs.clangStdenv;
              }
              {
                packages =
                  with pkgs;
                  [
                    clang-tools
                    cmake
                    codespell
                    conan
                    cppcheck
                    doxygen
                    gtest
                    lcov
                    vcpkg
                    vcpkg-tool
                    # Graphics libraries for RAFGL
                    glfw-wayland
                    mesa
                    libGL
                    libGLU
                    libglvnd
                    libdecor
                    wayland
                    wayland-protocols
                    libxkbcommon
                    xorg.libX11
                    xorg.libXcursor
                    xorg.libXrandr
                    xorg.libXi
                    xorg.libXinerama
                    # Build tools
                    pkg-config
                  ]
                  ++ (if system == "aarch64-darwin" then [ ] else [ gdb ]);

                shellHook = ''
                  export LIBGL_DRIVERS_PATH="${pkgs.mesa}/lib/dri"
                  export LD_LIBRARY_PATH="${pkgs.mesa}/lib:${pkgs.libglvnd}/lib:${pkgs.glfw-wayland}/lib:$LD_LIBRARY_PATH"
                  export MESA_GL_VERSION_OVERRIDE="4.6"
                  export MESA_GLSL_VERSION_OVERRIDE="460"

                  # Working EGL/Wayland configuration for GLFW
                  export EGL_PLATFORM=wayland
                  export WAYLAND_DEBUG=0
                  export __EGL_VENDOR_LIBRARY_DIRS="${pkgs.mesa}/share/glvnd/egl_vendor.d"
                '';
              };
        }
      );
    };
}
