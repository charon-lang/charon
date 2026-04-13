{
    inputs = {
        nixpkgs.url = "github:NixOS/nixpkgs/nixos-25.11";
        flake-utils.url = "github:numtide/flake-utils";
    };

    outputs =
        { nixpkgs, flake-utils, ... }:
        flake-utils.lib.eachDefaultSystem (
            system:
            let
                pkgs = import nixpkgs { inherit system; };
            in
            {
                devShells.default = pkgs.mkShell {
                    shellHook = "export NIX_SHELL_NAME='charon'; export NIX_HARDENING_ENABLE=''";
                    buildInputs = with pkgs; [
                        meson
                        ninja
                        pkg-config

                        clang_20
                        llvmPackages_20.clang-tools
                        lld_20

                        pcre2
                        llvmPackages_20.libllvm

                        gdb
                        valgrind

                        python3
                    ];
                };
            }
        );
}
