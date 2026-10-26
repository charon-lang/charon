{
    inputs = {
        nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";
    };

    outputs =
        { nixpkgs, ... }:
        let
            systems = [
                "x86_64-linux"
                "aarch64-linux"
                "x86_64-darwin"
                "aarch64-darwin"
            ];
            forEachSystem = f: nixpkgs.lib.genAttrs systems (system: f (import nixpkgs { inherit system; }));
        in
        {
            devShells = forEachSystem (pkgs: {
                default = pkgs.mkShell {
                    NIX_SHELL_NAME = "charon";

                    nativeBuildInputs = with pkgs; [
                        meson
                        ninja
                        pkg-config

                        clang_20
                        llvmPackages_20.clang-tools
                        lld_20

                        llvmPackages_20.libllvm

                        gdb
                        valgrind

                        python3
                    ];
                };
            });
        };
}
