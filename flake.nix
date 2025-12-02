{
  description = "bitcoinfuzz development shell for fuzzing builds";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    flake-utils.url = "github:numtide/flake-utils";
    rust-overlay.url = "github:oxalica/rust-overlay";
  };

  outputs = { self, nixpkgs, flake-utils, rust-overlay }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        overlays = [ (import rust-overlay) ];
        pkgs = import nixpkgs {
          inherit system overlays;
        };
        inherit (pkgs) lib stdenv;

        rustToolchain =
          pkgs.rust-bin.selectLatestNightlyWith (toolchain:
            toolchain.default.override {
              extensions = [ "rust-src" "clippy" "rustfmt" ];
            });

        jdk = pkgs.jdk17;
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
              libtool
              unzip
              zip
              git
              go
              rustup
              rustToolchain
              jdk
              maven
              gradle
              dotnet-sdk_8
              libsodium
              perl
              python3
              python3Packages.pip
              python3Packages.setuptools
              python3Packages.wheel
              python3Packages.mako
            ]
            ++ lib.optionals stdenv.isDarwin [ libiconv ];

          shellHook = ''
            export CC=${pkgs.clang}/bin/clang
            export CXX=${pkgs.clang}/bin/clang++
            export GO111MODULE=on
            export CGO_ENABLED=1
            export GOROOT=${pkgs.go}/share/go
            export PATH=$PWD/.cargo/bin:${rustToolchain}/bin:${pkgs.go}/bin:$PATH
            export CARGO=${rustToolchain}/bin/cargo
            export RUSTC=${rustToolchain}/bin/rustc
            export JAVA_HOME=${jdk.home}
            export DOTNET_ROOT=${pkgs.dotnet-sdk_8}
            export PATH=$DOTNET_ROOT/bin:$PATH
            export RUSTUP_HOME=$PWD/.rustup
            export CARGO_HOME=$PWD/.cargo
            export RUSTUP_TOOLCHAIN=nightly
            mkdir -p "$RUSTUP_HOME" "$CARGO_HOME/bin"
            # Simple shim so `cargo +nightly …` works without rustup
            cat > "$CARGO_HOME/bin/cargo" <<'EOF'
#!/usr/bin/env bash
set -e
if printf '%s' "$1" | grep -q '^+'; then
  shift
fi
exec ${rustToolchain}/bin/cargo "$@"
EOF
            chmod +x "$CARGO_HOME/bin/cargo"
            # Stub rustup to no-op for `rustup default nightly` expected by auto_build.sh
            cat > "$CARGO_HOME/bin/rustup" <<'EOF'
#!/usr/bin/env bash
set -e
if [ "x$1" = "xdefault" ]; then
  # ignore requested default toolchain; use the Nix-provided toolchain in PATH
  exit 0
fi
exec ${pkgs.rustup}/bin/rustup "$@"
EOF
            chmod +x "$CARGO_HOME/bin/rustup"
            echo "Using clang at: $CC"
          '';
        };
      });
}
