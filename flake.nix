{
  description = "A Nix-flake-based C/C++ development environment";

  inputs.nixpkgs.url = "https://flakehub.com/f/NixOS/nixpkgs/0"; # stable Nixpkgs

  outputs = { self, ... }@inputs:
  let
    supportedSystems = [ "x86_64-linux" ];

    forEachSupportedSystem = f: inputs.nixpkgs.lib.genAttrs supportedSystems (
      system: f {
        inherit system;
        pkgs = import inputs.nixpkgs { inherit system; };
      }
    );
  in
  {
    devShells = forEachSupportedSystem (
      { pkgs, system }: {
        default = pkgs.mkShell.override {
          stdenv = pkgs.gccStdenv;
        } {
          buildInputs = with pkgs; [ pipewire aubio ];

          packages = with pkgs; [
            cppcheck
            self.formatter.${system}
            pkg-config
          ]
          ++
          lib.optionals (!stdenv.hostPlatform.isDarwin) [ gdb ];
        };
      }
    );

    packages = forEachSupportedSystem (
      { pkgs, system }: {
        default = pkgs.callPackage ./package.nix {};
      }
    );

    formatter = forEachSupportedSystem ({ pkgs, ... }: pkgs.nixfmt);
  };
}