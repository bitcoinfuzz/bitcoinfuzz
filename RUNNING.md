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

# You can also limit resources to the container:
# --env-file uses the .env file, -v binds a volume, --cpus and --memory
# limit cpus and memory, and --net none disables internet connection
docker run --rm \
    --name bitcoinfuzz-transaction_eval \
    --env-file .env \
    -v "$(pwd)/docker":/app/data \
    --cpus 2 \
    --memory 2g \
    --net none \
    bitcoinfuzz:transaction_eval
```

> [!NOTE]
> Replace the bind mount with a named volume if preferred: `docker run ... -v bitcoinfuzz-data:/app/data ...`. Be aware that folder will be created.

### Running several targets from one image

`FUZZ` selects the fuzz target at runtime, so the `--build-arg FUZZ=...` only bakes a default into
the image. Any target can be picked per container with `-e FUZZ=`, and its corpus/crashes land in
their own subfolder of `/app/data`:

```bash
docker run --rm -e FUZZ=psbt_v0_parse -v "$(pwd)/docker":/app/data bitcoinfuzz:script
# -> corpus in ./docker/psbt_v0_parse/corpus, crashes in ./docker/psbt_v0_parse/crash
```

Set `FUZZ_DATAROOT` to relocate the parent folder, or `FUZZ_DATADIR` to override the full path.

### Leak detection and fork mode

`LIBFUZZ_DETECT_LEAKS` defaults to `0` whenever `-fork` is on (which is the default, sized from
the container's CPU quota). It sets both libFuzzer's `-detect_leaks` flag and `detect_leaks` in
`ASAN_OPTIONS`, because those are separate knobs: `-detect_leaks=0` alone only disables
libFuzzer's per-input leak attribution, leaving ASan to still run LeakSanitizer at process exit.

That exit-time check is the problem under `-fork`. Fork children exit every time slice, so any
one-time library initialization allocation is reported on each child exit. The parent counts it
as a crash and writes an empty `crash-da39a3ee5e6b4b0d3255bfef95601890afd80709` artifact — that
SHA1 is the hash of no input at all, which is why replaying it does nothing. Without `-fork` the
process never exits, so the check never fires and the same run looks clean.

To hunt real leaks, use a dedicated single-process run rather than fighting fork mode:

```bash
docker run --rm -e FUZZ=verify_script -e LIBFUZZ_FORK=0 \
  -v "$(pwd)/docker":/app/data bitcoinfuzz:verify_script
```

Setting `LIBFUZZ_DETECT_LEAKS=1` re-enables both knobs even with `-fork` on, and an explicitly
provided `ASAN_OPTIONS` is always passed through untouched.

> [!IMPORTANT]
> Only `FUZZ` is a runtime choice. `CXXFLAGS` is a **build-time** arg that decides which library
> modules are compiled and linked in, and `docker-compose.yml` gives each target a minimal set. To
> run several targets from one image, build it with the union of the `-D` flags those targets need,
> otherwise the differential comparison runs against fewer implementations than expected.

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
CXXFLAGS="-DBITCOIN_CORE -DRUST_BITCOIN" ./scripts/auto_build.py
```

### Option 5 - AFL++ in Docker

The repository also ships an AFL++ image that builds the same target/module set
using `afl-clang-fast` and runs `afl-fuzz` inside the container.

```bash
mkdir -p docker-afl

docker build -f Dockerfile.afl -t bitcoinfuzz-afl:verify_script \
  --build-arg FUZZ=verify_script \
  --build-arg "CXXFLAGS=-DBITCOIN_CORE -DBTCD -DGOCOIN -DNBITCOIN -DLIBBITCOIN_SYSTEM" .

docker run --rm -it \
  -e FUZZ=verify_script \
  -e MODULES="BITCOIN_CORE,BTCD,LIBBITCOIN_SYSTEM" \
  -e AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1 \
  -v "$(pwd)/docker-afl":/app/data \
  bitcoinfuzz-afl:verify_script
```

On many Linux hosts, AFL++ aborts if the kernel `core_pattern` pipes crashes to
an external utility. For container-based local fuzzing, the practical fix is
usually:

```bash
-e AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1
```

If you want the stricter host-side fix instead, run:

```bash
echo core | sudo tee /proc/sys/kernel/core_pattern
```

By default the container stores AFL++ state under `/app/data/<target>/`:

- seeds: `/app/data/<target>/in`
- queue, crashes, hangs, stats: `/app/data/<target>/out`

If the input directory is empty, the entrypoint creates a single seed file
automatically. Resume is enabled by default via `AFL_AUTORESUME=1`; if
`/app/data/<target>/out/default` already exists, the container runs `afl-fuzz`
with `-i-` and continues the previous session.

You can pass additional AFL++ flags after the image name:

```bash
docker run --rm -it \
  -e FUZZ=verify_script \
  -v "$(pwd)/docker-afl":/app/data \
  bitcoinfuzz-afl:verify_script \
  -m none -t 2000+
```

This script cleans and builds modules based on `CXXFLAGS`, and then compiles the root project.

### Miniscript parser module pairings

For `miniscript_parse`, use **Bitcoin Core** and **Rust-Miniscript** together when you want a stricter
conformance-oriented comparison:

```bash
MODULES="BITCOIN_CORE,RUST_MINISCRIPT" FUZZ=miniscript_parse ./bitcoinfuzz ...
```

Use *Bitcoinerlab Miniscript* and *NBitcoin* together when you want to fuzz more
permissive miniscript parsers:

```bash
MODULES="BITCOINERLAB_MINISCRIPT,NBITCOIN" FUZZ=miniscript_parse ./bitcoinfuzz ...
```

Bitcoinerlab Miniscript and NBitcoin accept a broader set of miniscript strings
than Bitcoin Core and rust-miniscript. For example, they may accept arbitrary key names or
forms that stricter parsers reject at the parsing boundary. Because of that,
accept/reject mismatches between `BITCOIN_CORE` and `BITCOINERLAB_MINISCRIPT`
or `NBITCOIN` are often expected parser-policy differences rather than bugs in
the miniscript implementation. Pairing modules with similar permissiveness makes
crashes and result mismatches easier to interpret.

## Reproducing Crashes

When libFuzzer finds a crash it writes the input to `-artifact_prefix`, which the
entrypoint points at `/app/data/<target>/crash/`. With the default bind mount that is
`./docker/<target>/crash/crash-<sha1>` on the host.

> [!IMPORTANT]
> The entrypoint always appends `${FUZZ_DATADIR}/corpus` as the last positional argument,
> so adding a crash file to a normal `docker run` does **not** replay it — libFuzzer keeps
> fuzzing and treats the extra path as another corpus entry. Override the entrypoint to run
> the binary directly.

```bash
docker run --rm \
  -e FUZZ=verify_script \
  -e MODULES="LIBBITCOIN_SYSTEM,BITCOIN_CORE" \
  -v "$(pwd)/docker":/app/data \
  --entrypoint /app/bitcoinfuzz \
  bitcoinfuzz:verify_script \
  /app/data/verify_script/crash/crash-<sha1>
```

Given a file instead of a directory, libFuzzer executes that single input once and prints
the stack trace, then exits. Two things matter for the crash to reappear:

- **Use the same `MODULES` value as the fuzzing run.** Differential targets abort on a
  mismatch between implementations, so a different module set (or a set of one) will not
  reproduce it.
- **Do not pass `-fork`.** Overriding the entrypoint drops all the `LIBFUZZ_*` flags,
  including fork mode, which is what you want: `-fork` hides the stack trace in the child
  process. Re-add individual flags only if the crash depends on them (`-rss_limit_mb` for
  an OOM, `-timeout` for a hang, `-detect_leaks=1` for a leak).

For a differential mismatch rather than a segfault or assert, add `-e LOG_OUTPUTS=1` to
print each module's output and see which implementations disagree:

```bash
docker run --rm \
  -e FUZZ=verify_script \
  -e MODULES="LIBBITCOIN_SYSTEM,BITCOIN_CORE" \
  -e LOG_OUTPUTS=1 \
  -v "$(pwd)/docker":/app/data \
  --entrypoint /app/bitcoinfuzz \
  bitcoinfuzz:verify_script \
  /app/data/verify_script/crash/crash-<sha1>
```

### Minimizing a crash

Shrink the input before filing a bug report. The result is written next to the original as
`minimized-from-<sha1>`:

```bash
docker run --rm \
  -e FUZZ=verify_script \
  -e MODULES="LIBBITCOIN_SYSTEM,BITCOIN_CORE" \
  -v "$(pwd)/docker":/app/data \
  --entrypoint /app/bitcoinfuzz \
  bitcoinfuzz:verify_script \
  -minimize_crash=1 -runs=100000 \
  -artifact_prefix=/app/data/verify_script/crash/ \
  /app/data/verify_script/crash/crash-<sha1>
```

> [!NOTE]
> The image must have been built with the union of the `-D<MODULE>` flags for every module
> in `MODULES`, since `CXXFLAGS` is build-time only. Reproduce with the same image that
> produced the crash; rebuilding with different build args can make it disappear.

### Reproducing AFL++ crashes

AFL++ stores crashing inputs under `/app/data/<target>/out/default/crashes/` (`./docker-afl/...`
on the host). The AFL image is built with `-fsanitize=fuzzer`, so `/app/bitcoinfuzz` accepts
an input file the same way:

```bash
docker run --rm \
  -e FUZZ=verify_script \
  -e MODULES="BITCOIN_CORE,BTCD,LIBBITCOIN_SYSTEM" \
  -v "$(pwd)/docker-afl":/app/data \
  --entrypoint /app/bitcoinfuzz \
  bitcoinfuzz-afl:verify_script \
  '/app/data/verify_script/out/default/crashes/id:000000,sig:06,src:000000,...'
```

Quote the path - AFL++ crash filenames contain commas and colons.
