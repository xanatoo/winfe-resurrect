{
  description = "Visual resurrection of the 1993 Udanax Gold Windows frontend (winfe)";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-24.11";

  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f system);
    in {
      packages = forAllSystems (system:
        let
          pkgs = import nixpkgs { inherit system; };
          pkgsWin = pkgs.pkgsCross.mingwW64;
        in rec {
          winfe-resurrect = pkgsWin.stdenv.mkDerivation {
            pname = "winfe-resurrect";
            version = "0.7.0";

            # Flake source; do NOT set sourceRoot = "." — that points at
            # $NIX_BUILD_TOP instead of the unpacked source directory.
            src = ./.;

            nativeBuildInputs = [ pkgsWin.stdenv.cc ];

            dontConfigure = true;

            buildPhase = ''
              runHook preBuild
              echo "Source tree:"
              ls -la
              $CXX -std=c++17 -O2 -Wall -mwindows \
                winfe_resurrect.cpp \
                -o winfe_resurrect.exe \
                -lcomctl32 -lgdi32 -luser32
              runHook postBuild
            '';

            installPhase = ''
              runHook preInstall
              mkdir -p $out/bin
              install -Dm755 winfe_resurrect.exe $out/bin/winfe_resurrect.exe
              mkdir -p $out/share/winfe-resurrect
              cp -f README.md fe.rc fe.h $out/share/winfe-resurrect/ 2>/dev/null || true
              runHook postInstall
            '';

            meta = with pkgs.lib; {
              description = "1993 Udanax Gold winfe visual resurrection (Windows executable)";
              license = licenses.mit;
              platforms = platforms.all;
              hydraPlatforms = [];
            };
          };

          winfe-resurrect-wine = pkgs.writeShellApplication {
            name = "winfe-resurrect";
            runtimeInputs = [ pkgs.wineWowPackages.stable ];
            text = ''
              exec wine ${winfe-resurrect}/bin/winfe_resurrect.exe "$@"
            '';
          };

          default = winfe-resurrect;
        });

      apps = forAllSystems (system: {
        default = {
          type = "app";
          program = "${self.packages.${system}.winfe-resurrect-wine}/bin/winfe-resurrect";
        };
        wine = {
          type = "app";
          program = "${self.packages.${system}.winfe-resurrect-wine}/bin/winfe-resurrect";
        };
      });

      devShells = forAllSystems (system:
        let pkgs = import nixpkgs { inherit system; };
        in {
          default = pkgs.mkShell {
            packages = with pkgs; [
              pkgsCross.mingwW64.stdenv.cc
              wineWowPackages.stable
              gnumake
            ];
            shellHook = ''
              echo "winfe-resurrect dev shell"
              echo "  make          # cross-compile with MinGW"
              echo "  nix build     # produce the .exe"
              echo "  nix run       # build + run under Wine"
            '';
          };
        });
    };
}
