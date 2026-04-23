# RUNNING

## Environment Preparation

Install Docker with the `docker compose` plugin, jq, and just before interacting with this repository:

```bash
# macOS (Homebrew)
brew install docker jq just

# Ubuntu / Debian
sudo apt-get update
sudo apt-get install -y docker.io jq just

docker --version
docker compose version
jq --version
just --version
```

## Environment variables

Create a project-local environment file to control libFuzzer flags:

```bash
cp .env.example .env
# edit LIBFUZZ_* entries as needed
```

Consult the upstream [libFuzzer](https://llvm.org/docs/LibFuzzer.html#options) docs to better understand the options: 

## Run Options

### Option 1 – Quick run via `just`

```bash
just run <fuzz target>

# e.g.
just run script
just run descriptor_parse
```

> [!NOTE]
> Each target relies on a writable `/app/data`. The default compose configuration binds `./docker` on the host. Ensure it exists (`mkdir -p docker`) or adjust the volume mapping before running.

### Option 2 – Custom run via Docker Compose

1. Modify `docker-compose.yml` (build args, env vars, new services, cpu quota, etc.).
2. Launch services:

```bash
docker compose up <fuzz target | compose service> --build --force-recreate

# e.g.
docker compose up script --build --force-recreate
docker compose up descriptor_parse miniscript_parse --build

# later, to stop and remove containers
docker compose down
```

> [!NOTE]
> Keep the `./docker:/app/data` bind (or swap for a named volume like `docker volume create bitcoinfuzz-data`) so
 crashes/corpora persist on the host.

### Option 3 – Manual `docker run`

```bash
mkdir docker
# See auto_build.py and Dockerfile to understand better the build args
docker build -t bitcoinfuzz:script \
  --build-arg FUZZ=script \
  --build-arg "CXXFLAGS=-DBITCOIN_CORE -DRUST_BITCOIN" .

docker run --rm \
  --name bitcoinfuzz-script \
  -e FUZZ=script \
  -e LIBFUZZ_TIMEOUT=300 \
  -v "$(pwd)/docker":/app/data \
  bitcoinfuzz:script

# You can also limit resources to the container
docker run --rm \
    --name bitcoinfuzz-transaction_eval \
    --env-file .env \ # use the .env file
    -v "$(pwd)/docker":/app/data # bind a volume
    --cpus 2 \ # limit cpus
    --memory 2g \ # limit memory
    --net none \ # disable internet connection
    bitcoinfuzz:transaction_eval
```

> [!NOTE]
> Replace the bind mount with a named volume if preferred: `docker run ... -v bitcoinfuzz-data:/app/data ...`. Be aware that folder will be created.

### Selective Module Loading

You can use the `MODULES` environment variable to load only specific modules at runtime, without rebuilding the image:

```bash
# Load only Bitcoin Core and rust-bitcoin
docker run --rm \
  -e MODULES="BITCOIN_CORE,RUST_BITCOIN" \
  -v "$(pwd)/docker":/app/data \
  bitcoinfuzz:script

# Load only Lightning implementations
docker run --rm \
  -e MODULES="LDK,LND,CLIGHTNING" \
  -v "$(pwd)/docker":/app/data \
  bitcoinfuzz:deserialize_invoice

# With docker compose
MODULES="BITCOIN_CORE,RUST_BITCOIN" docker compose up deserialize_block
```

### Option 4 – Running from Source

To build outside of Docker (useful for local debugging), execute `auto_build.py` with the required flags:

```bash
# pass in the modules you want to compile with -D<mod> in CXXFLAGS
CXXFLAGS="-DBITCOIN_CORE -DRUST_BITCOIN" ./auto_build.py
```

This script cleans and builds modules based on `CXXFLAGS`, and then compiles the root project.
