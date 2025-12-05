{
  description = "bitcoinfuzz development shell for fuzzing builds";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs {
          inherit system;
        };
      in {
        devShells.default = pkgs.mkShell {
          packages =
            with pkgs;
            [
              clang
              lld
              llvmPackages.llvm
              gnumake
              cmake
              pkg-config
              autoconf
              automake
              jq
              sqlite
              libtool
              unzip
              zip
              git
              go
              rustup
              jdk21
              dotnet-sdk_10
              libsodium
              lowdown
              (python3.withPackages (ps: with ps; [ pip setuptools distutils mako ]))
            ];

          shellHook = ''
            export CC=${pkgs.clang}/bin/clang
            export CXX=${pkgs.clang}/bin/clang++
            export GOROOT=${pkgs.go}/share/go
            export JAVA_HOME=${pkgs.jdk21.home}
            export DOTNET_ROOT=${pkgs.dotnet-sdk_10}
            export PATH=$DOTNET_ROOT/bin:$PATH
          '';
        };
      });
}
