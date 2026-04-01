# RUNNING

## Environment Preparation

Install Docker, Docker Compose, jq, and just before interacting with this repository:

```bash
# macOS (Homebrew)
brew install docker jq just
brew install docker-compose # if not bundled with Docker Desktop

# Ubuntu / Debian
sudo apt-get update
sudo apt-get install -y docker.io jq just
sudo curl -L "https://github.com/docker/compose/releases/download/v5.0.0/docker-compose-$(uname -s)-$(uname -m)" -o /usr/local/bin/docker-compose
sudo chmod +x /usr/local/bin/docker-compose

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

## Your First Fuzzing Session — Annotated Output Guide

When you run a fuzzer for the first time, the output can look intimidating. Here is a breakdown of what happens when you start a fuzzing session.

### Step 1: Starting the Fuzzer
Let's fuzz the `script` target using only the Bitcoin Core implementation:

```bash
MODULES="BITCOIN_CORE" docker compose up script
```

### Step 2: Understanding libFuzzer Output
Once Docker builds the image and starts the container, the fuzzer (`libFuzzer`) takes over. You will see an initialization sequence followed by continuous metrics:

```text
INFO: Seed: 192837465
INFO: Loaded 1 modules   (76505 inline 8-bit counters)
INFO: Found 12 inputs from corpus.
#12      INITED cov: 540 ft: 610 corp: 12/8192b exec/s: 0 rss: 42Mb
```
* **`Seed`**: The random seed used for this run. If you want to reproduce the exact same run, you can pass `-seed=192837465`.
* **`Loaded X modules`**: Indicates the fuzzer successfully instrumented the binary.
* **`INITED`**: Indicates the fuzzer has loaded your initial inputs (corpus) and is ready.
* **`cov` (Coverage)**: The total number of code blocks/edges hit so far. *This metric going up is good!*
* **`corp` (Corpus)**: The number of saved inputs in memory and their total size in bytes.
* **`rss` (Memory)**: The RAM usage of the fuzzer.

As the fuzzer runs, it will print new lines whenever it discovers a new path in the code:
```text
#500     NEW    cov: 550 ft: 625 corp: 15/9030b lim: 10 exec/s: 250 rss: 45Mb
#1000    pulse  cov: 550 ft: 625 corp: 15/9030b lim: 15 exec/s: 333 rss: 47Mb
```
* **`NEW`**: The fuzzer mutated an input and successfully triggered new, previously unseen code! It saves this input to its corpus to mutate it further later.
* **`pulse`**: A periodic ping just to let you know the fuzzer is still running, even if it hasn't found new coverage recently.
* **`exec/s`**: How many thousands of executions (mutations tested) are happening per second.

### Step 3: Finding Crash Artifacts
If the fuzzer encounters a bug (like a memory leak, crash, or timeout), it will abort execution and print a detailed stack trace. At the very end of the stack trace, you will see something like this:

```text
==12== ERROR: AddressSanitizer: heap-use-after-free on address ...
...
artifact_prefix='./'; Test unit written to ./crash-1234567890abcdef
```

Because of our `docker-compose.yml` configuration, the `/app/data` folder inside the container is mapped directly to the `./docker/` folder on your host machine.

**To analyze the crash:**
Look inside the `./docker/` folder in your project directory (this will be created automatically if it doesn't exist). You will find a file named `crash-1234567890abcdef`. This file contains the exact raw byte input that caused the program to crash. You can pass this file directly to the compiled binary to reproduce the crash during debugging.
