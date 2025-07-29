{
    inputs = {
        nixpkgs.url = "github:NixOS/nixpkgs/nixos-25.05";
        flake-utils.url = "github:numtide/flake-utils";
    };

    outputs = { self, nixpkgs, flake-utils, ... }: flake-utils.lib.eachDefaultSystem (system:
        let
            pkgs = import nixpkgs { inherit system; };
            inherit (pkgs) lib stdenv mkShell;
        in {
            devShells.default = mkShell {
                shellHook = "export NIX_SHELL_NAME='charon'";
                nativeBuildInputs = with pkgs; [
                    meson
                    ninja
                    pkgconf
                    pcre2
                    llvmPackages_20.libllvm
                    llvmPackages_20.clang-tools
                    lld_20
                    clang_20
                    gnumake
                    gdb
                    bear
                    valgrind
                    go
                ];
            };

            defaultPackage = stdenv.mkDerivation rec {
                name = "charon";
                src = self;

                nativeBuildInputs = with pkgs; [ meson ninja pkgconf clang_20 llvmPackages_20.clang-tools go ];
                buildInputs = with pkgs; [ pcre2 llvmPackages_20.libllvm ];

                configurePhase = ''
                    runHook preConfigure
                    meson setup build --native-file native.ini
                    runHook postConfigure
                '';

                buildPhase = ''
                    ninja -C build lib/libcharon.a charonc/charonc
                '';

                installPhase = ''
                    mkdir -p $out/bin $out/lib
                    cp build/lib/libcharon.a $out/lib/
                    cp build/charonc/charonc $out/bin/
                '';

                meta = {
                    description = "Based programming language.";
                    homepage = "https://github.com/charon-lang/charon";
                    maintainers = with lib.maintainers; [ wux ];
                };
            };
        }
    );
}
