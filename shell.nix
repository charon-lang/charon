{ pkgs ? import <nixpkgs> {} }:
pkgs.mkShell {
    buildInputs = with pkgs; [
        pcre2
        llvmPackages_18.libllvm
        gcc13
        gnumake
    ];
}