#!/usr/bin/env -S just --justfile

required_bins := require("docker") + require("docker-compose") + require("jq")

[default]
_default:
  @just --list


# Run all fuzzing targets
run-all: (run '')

# Starts a fuzzing target using docker and docker compose
run TARGET:
    docker compose up {{TARGET}} --force-recreate --build

# Cleans docker build cache
docker-clean:
    docker builder prune -f

# Builds an image for a specific fuzz target
docker-build TARGET *EXTRA_DOCKER_ARGS='':
    #!/usr/bin/env bash
    set -euo pipefail
    DEFAULT_FLAGS=`docker compose config --format=json | jq -r '.services.{{TARGET}}.build.args.CXXFLAGS'`

    if [ -z "$DEFAULT_FLAGS" ] || [ "$DEFAULT_FLAGS" = "null" ]; then
        echo "Error: Service '{{TARGET}}' not found or build arguments are missing in docker-compose.yml" >&2
        exit 1
    fi

    set -x
    docker build -t bitcoinfuzz:{{TARGET}} --build-arg "FUZZ={{TARGET}}" --build-arg "CXXFLAGS=${DEFAULT_FLAGS}" {{EXTRA_DOCKER_ARGS}} .

# Builds an image from the builder layer (useful to debug compilation/linking/files)
docker-builder TARGET: (docker-build TARGET '--target=builder -t bitcoinfuzz:builder')

# Builds all fuzzing targets according to docker-compose.yml
docker-build-all:
    #!/usr/bin/env bash
    set -euo pipefail
    TARGETS=`docker compose config --format=json | jq -r '.services[].build.args.FUZZ'`
    for TARGET in $TARGETS; do
        echo "---> Building image for $TARGET"
        just docker-build $TARGET || echo -e "---> Failed to build $TARGET"
    done
