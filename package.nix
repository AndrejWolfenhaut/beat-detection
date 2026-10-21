{
  pkgs,
  lib,
  stdenv,
  ...
}: stdenv.mkDerivation {
  pname = "beat-detection";
  version = "1.0";

  buildInputs = with pkgs; [ pipewire aubio ];

  nativeBuildInputs = with pkgs; [ pkg-config ];

  src = lib.fileset.toSource {
    root = ./.;
    fileset = lib.fileset.intersection
      (lib.fileset.fromSource (lib.sources.cleanSource ./.))
      (lib.fileset.unions [ ./Makefile ./src ]);
  };

  installPhase = ''
    mkdir -p $out/bin

    cp ./build/beat-detection $out/bin
  '';
}