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
            version = "0.7.2";
            src = self;

            nativeBuildInputs = [ pkgsWin.stdenv.cc ];
            dontConfigure = true;

            buildPhase = ''
              runHook preBuild
              $CXX -std=c++17 -O2 -Wall -mwindows \
                winfe_resurrect.cpp \
                -o winfe_resurrect.exe \
                -lcomctl32 -lgdi32 -luser32
              runHook postBuild
            '';

            installPhase = ''
              runHook preInstall
              mkdir -p $out/bin $out/share/winfe-resurrect
              install -Dm755 winfe_resurrect.exe $out/bin/winfe_resurrect.exe
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
            runtimeInputs = [ pkgs.wineWowPackages.stable pkgs.coreutils ];
            text = ''
              set -euo pipefail
              export WINEPREFIX="''${WINEPREFIX:-$(mktemp -d -t winfe-wine-XXXXXX)}"
              export WINEARCH=win64
              export WINEDLLOVERRIDES="mscoree,mshtml="
              # Initialize prefix quietly (first run only)
              if [[ ! -f "$WINEPREFIX/system.reg" ]]; then
                wineboot --init 2>/dev/null || true
                # Give wineserver a moment to finish
                wineserver -w 2>/dev/null || true
              fi
              exec wine64 ${winfe-resurrect}/bin/winfe_resurrect.exe "$@"
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
          };
        });
    };
}
