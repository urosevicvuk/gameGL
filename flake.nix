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
                    # Development tools
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
                    pkg-config
                    
                    # Graphics libraries for RAFGL
                    glfw-wayland
                    mesa
                    libGL
                    libGLU
                    libglvnd
                    libdecor
                    
                    # Wayland support
                    wayland
                    wayland-protocols
                    libxkbcommon
                    
                    # X11 libraries (needed by GLFW)
                    xorg.libX11
                    xorg.libXcursor
                    xorg.libXrandr
                    xorg.libXi
                    xorg.libXinerama
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
                  
                  # Suppress GVFS/GLib version mismatch errors (cosmetic fix)
                  # GVFS is not needed for OpenGL applications but gets loaded by the desktop environment
                  export GIO_EXTRA_MODULES=""
                '';
              };
        }
      );
    };
}
