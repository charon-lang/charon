{
    inputs = {
        nixpkgs.url = "github:NixOS/nixpkgs/nixos-25.05";
        flake-utils.url = "github:numtide/flake-utils";
    };

    outputs = { self, nixpkgs, flake-utils, ... }: flake-utils.lib.eachDefaultSystem (system:
        let
            pkgs = import nixpkgs { inherit system; };
            inherit (pkgs) mkShell;
        in {
            devShells.default = mkShell {
                shellHook = "export NIX_SHELL_NAME='charon'";
                nativeBuildInputs = with pkgs; [
                    meson
                    ninja
                    pkgconf

                    clang_20
                    llvmPackages_20.clang-tools
                    lld_20

                    pcre2
                    llvmPackages_20.libllvm

                    gdb
                    valgrind

                    sphinx
                    python312Packages.sphinx-rtd-theme
                ];
            };
        }
    );
}
