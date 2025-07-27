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
                    cjson
                    llvmPackages_18.libllvm
                    lld_18
                    clang_18
                    clang-tools_18
                    gnumake
                    gdb
                    bear
                    valgrind
                ];
            };

            defaultPackage = stdenv.mkDerivation rec {
                name = "charon";
                src = self;

                nativeBuildInputs = with pkgs; [ clang_18 clang-tools_18 ];
                buildInputs = with pkgs; [ pcre2 llvmPackages_18.libllvm ];

                installPhase = ''
                    mkdir -p $out/bin
                    cp charon $out/bin/
                '';

                meta = {
                    description = "Based programming language.";
                    homepage = https://git.thenest.dev/wux/charon;
                    maintainers = with lib.maintainers; [ wux ];
                };
            };
        }
    );
}
