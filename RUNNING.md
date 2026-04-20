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

---

## Annotated First Fuzzing Session

This walkthrough runs the `deserialize_block` target end-to-end and explains every
line of output. Use it to orient yourself the first time you run bitcoinfuzz.

### 1. Start the session

```bash
cp .env.example .env        # sets LIBFUZZ_RUNS=1000 so the run finishes quickly
just run deserialize_block
```

`just run` expands to:

```
docker compose up deserialize_block --force-recreate --build
```

Docker reads the `deserialize_block` service from `docker-compose.yml`, which has:

```yaml
args:
  CXXFLAGS: "-DBITCOIN_CORE -DRUST_BITCOIN -DBTCD"
  FUZZ: deserialize_block
```

### 2. Reading the Docker build output

The Dockerfile is a two-stage build (`builder` → `runner`).

**Builder stage** — compiles everything:

```
 => [builder] RUN ... /build/auto_build.py
```

`auto_build.py` reads `CXXFLAGS`, builds `BITCOIN_CORE`, `RUST_BITCOIN`, and `BTCD`
in parallel (each module runs its own `Makefile`, produces `module.a`), then runs
the root `make` to link `bitcoinfuzz`.

**Runner stage** — copies only what is needed to run:

```
 => [runner] COPY --from=builder /build/bitcoinfuzz .
 => [runner] COPY --from=builder /build/modules/*/lib /
 => [runner] COPY --from=builder /build/*.so .
```

The final image is small: only the `bitcoinfuzz` binary, shared libraries (Go, .NET,
Python), and `llvm-symbolizer` for readable crash stacks.

### 3. Container startup and libFuzzer flags

The container `ENTRYPOINT` creates two directories under `/app/data/deserialize_block/`:

```
/app/data/deserialize_block/corpus/   ← seed corpus; persists between runs
/app/data/deserialize_block/crash/    ← crash artifacts saved here
```

Both are inside the `./docker` bind-mount, so on your host they appear at:

```
./docker/deserialize_block/corpus/
./docker/deserialize_block/crash/
```

The `bitcoinfuzz` binary is then launched with the following effective flags
(defaults from the Dockerfile `ENTRYPOINT`, overridable via `.env`):

| Flag | Default | Set via |
|------|---------|---------|
| `-runs` | `-1` (unlimited) | `LIBFUZZ_RUNS` |
| `-max_len` | `10000` | `LIBFUZZ_MAX_LEN` |
| `-len_control` | `100` | `LIBFUZZ_LEN_CONTROL` |
| `-timeout` | `300` (per-input, seconds) | `LIBFUZZ_TIMEOUT` |
| `-rss_limit_mb` | `1024` | `LIBFUZZ_RSS_LIMIT_MB` |
| `-max_total_time` | `0` (disabled) | `LIBFUZZ_MAX_TOTAL_TIME` |
| `-fork` | number of CPUs (from cgroup or `nproc`) | `LIBFUZZ_FORKS` |
| `-reduce_inputs` | `1` (auto-minimize corpus) | `LIBFUZZ_REDUCE_INPUTS` |
| `-print_final_stats` | `1` | `LIBFUZZ_PRINT_FINAL_STATS` |
| `-detect_leaks` | `1` | `LIBFUZZ_DETECT_LEAKS` |

### 4. libFuzzer startup lines

```
INFO: Running with entropic power schedule (0xFF, 100).
INFO: Seed: 2847392010
INFO: Loaded 1 modules   (12890 inline 8-bit counters): 12890 [0x..., 0x...),
INFO: Loaded 1 PC tables (12890 PCs): 12890 [0x...,0x...),
INFO: 0 files found in /app/data/deserialize_block/corpus
INFO: -max_len is not provided; libFuzzer will not generate inputs larger than 10000 bytes
INFO: A corpus of 0 items, with 0 seeds
```

| Line | What it means |
|------|---------------|
| `entropic power schedule` | Strategy for allocating fuzzing energy; entropic favours inputs that reach rare edges |
| `Seed: N` | RNG seed — pass `-seed=N` to reproduce a specific run exactly |
| `inline 8-bit counters` | One coverage counter per basic block, inserted by `-fsanitize=fuzzer` at compile time |
| `PC tables` | Program-counter table linking each counter to a source location; used by `llvm-symbolizer` in crash stacks |
| `0 files found in … corpus` | Corpus directory is empty on first run; libFuzzer generates all inputs from scratch |
| `-max_len … 10000 bytes` | Maximum generated input size — higher than libFuzzer's own default because this is set explicitly in the `ENTRYPOINT` |

> **Note:** Because `-fork=N` is set (multi-process mode), you will see startup
> lines from each worker process, which can look noisy. Each worker independently
> updates the shared corpus directory.

### 5. Per-iteration status lines

```
#2      INITED cov: 142 ft: 156 corp: 1/1b lim: 4    exec/s: 0    rss: 210Mb
#512    NEW    cov: 287 ft: 341 corp: 8/48b lim: 8    exec/s: 512  rss: 224Mb
#1024   REDUCE cov: 287 ft: 341 corp: 8/39b lim: 11   exec/s: 1024 rss: 224Mb
#2048   pulse  cov: 301 ft: 362 corp: 14/91b lim: 17  exec/s: 1024 rss: 231Mb
```

| Field | Meaning |
|-------|---------|
| `#N` | Total inputs executed across all workers |
| `INITED` | Corpus initialisation finished; fuzzing proper begins |
| `NEW` | Input reached at least one previously-unseen edge — added to corpus |
| `REDUCE` | An existing corpus input was shrunk without losing coverage (because `-reduce_inputs=1`) |
| `pulse` | Periodic heartbeat printed even when nothing new is found |
| `cov: N` | Distinct edges (basic-block pairs) covered so far |
| `ft: N` | Feature count — a finer-grained metric that includes call-stack context |
| `corp: N/Xb` | Corpus size: N inputs, X bytes total |
| `lim: N` | Current generated-input size cap (libFuzzer grows this gradually up to `-max_len`) |
| `exec/s` | Inputs executed per second — the primary throughput metric |
| `rss: NMb` | Resident set size of the fuzzer process; watch for unbounded growth |

**What to watch:** `cov` climbs fast at first then plateaus — that plateau is normal
and means the fuzzer is deep-fuzzing existing paths. A sustained drop in `exec/s`
usually means a slow FFI call or a memory allocation in the hot path.

### 6. When bitcoinfuzz detects a divergence

`driver.cpp` runs each input through every loaded module and compares results. When
two modules disagree the target-specific failure message is printed, followed by
module names and their outputs, then `assert()` fires:

```
Block deserialization failed
Module: Rustbitcoin
Result: 000000000000000000000000...
Module: Btcd
Result: 0000000000000000000000001a...
Assertion failed: (response == *last_response)
```

libFuzzer catches the `SIGABRT` from the failed assert and writes the triggering
input to the crash directory:

```
artifact_prefix='/app/data/deserialize_block/crash/';
Test unit written to /app/data/deserialize_block/crash/crash-a1b2c3d4e5f6...
```

On your host this file appears at:

```
./docker/deserialize_block/crash/crash-a1b2c3d4e5f6...
```

### 7. Inspecting a crash artifact

```bash
ls docker/deserialize_block/crash/
# crash-a1b2c3d4e5f6...

# View raw bytes
xxd docker/deserialize_block/crash/crash-a1b2c3d4e5f6... | head -4
```

**Reproduce deterministically** — pass the crash file as the sole argument; libFuzzer
runs exactly once on that input and prints the divergence again:

```bash
docker run --rm \
  -v "$(pwd)/docker":/app/data \
  bitcoinfuzz:deserialize_block \
  /app/bitcoinfuzz /app/data/deserialize_block/crash/crash-a1b2c3d4e5f6...
```

**Reproduce from a source build** (no Docker):

```bash
CXXFLAGS="-DBITCOIN_CORE -DRUST_BITCOIN -DBTCD" ./auto_build.py
./bitcoinfuzz docker/deserialize_block/crash/crash-a1b2c3d4e5f6...
```

**Minimise the crash** — find the smallest input that still triggers the same divergence:

```bash
LIBFUZZ_MINIMIZE_CRASH=1 \
LIBFUZZ_RUNS=10000 \
docker compose up deserialize_block --force-recreate
# minimised input is written to ./docker/deserialize_block/crash/
```

Or override the artifact path directly:

```bash
docker run --rm \
  -v "$(pwd)/docker":/app/data \
  bitcoinfuzz:deserialize_block \
  /app/bitcoinfuzz \
    -minimize_crash=1 \
    -exact_artifact_path=/app/data/deserialize_block/crash/minimised \
    /app/data/deserialize_block/crash/crash-a1b2c3d4e5f6...
```

### 8. Normal exit and final stats

When `LIBFUZZ_RUNS` or `LIBFUZZ_MAX_TOTAL_TIME` is reached, libFuzzer prints final
statistics (enabled by `-print_final_stats=1`):

```
stat::number_of_executed_units: 1000
stat::average_exec_per_sec:     923
stat::new_units_added:          47
stat::slowest_unit_time_sec:    0
stat::peak_rss_mb:              231
```

| Field | Meaning |
|-------|---------|
| `number_of_executed_units` | Total inputs run (matches `LIBFUZZ_RUNS` if set) |
| `average_exec_per_sec` | Average throughput over the whole session |
| `new_units_added` | Corpus entries discovered — a rough measure of coverage gain |
| `slowest_unit_time_sec` | Slowest single input; useful for spotting pathological inputs |
| `peak_rss_mb` | Peak memory — compare against `-rss_limit_mb` (default 1024) |

The corpus accumulated during the run is persisted in
`./docker/deserialize_block/corpus/` and re-used automatically on the next run,
so coverage compounds across sessions.
