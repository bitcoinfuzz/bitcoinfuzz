# syntax=docker/dockerfile:1.20
FROM ubuntu:24.04 AS base

# llvm-symbolizer devs, python and sodium libs and jre
RUN --mount=type=cache,target=/var/cache/apt,id=fuzz-apt-cache-base \
    --mount=type=cache,target=/var/lib/apt/lists,id=fuzz-apt-lists-base \
    apt-get update && apt-get install -y --no-install-recommends --no-install-suggests \
    libcurl4t64 \
    libllvm18 \
    libpython3-dev \
    libsodium-dev \
    openjdk-21-jre-headless

FROM base AS builder

ENV DEBIAN_FRONTEND=noninteractive

WORKDIR /build

RUN --mount=type=cache,target=/var/cache/apt,id=fuzz-apt-cache-builder \
    --mount=type=cache,target=/var/lib/apt/lists,id=fuzz-apt-lists-builder \
    apt-get update && apt-get install -y --no-install-recommends --no-install-suggests \
    autoconf \
    automake \
    clang-18 \
    cmake \
    curl \
    gcc \
    git \
    golang-go \
    jq \
    libasan6 \
    libboost-all-dev \
    libc6-dev \
    libclang-rt-dev \
    libfuzzer-18-dev \
    libgcc-11-dev \
    libsqlite3-dev \
    libtool \
    llvm-dev \
    lowdown \
    make \
    openjdk-21-jdk-headless \
    pkgconf \
    python3 \
    python3-dev \
    python3-pip \
    python3-venv \
    rustup \
    unzip

# Install .NET SDK 9.0 using Microsoft's install script
# See https://learn.microsoft.com/en-us/dotnet/core/tools/dotnet-install-script
RUN curl -sSf -L -o dotnet-install.sh https://dot.net/v1/dotnet-install.sh && \
    chmod +x dotnet-install.sh && \
    ./dotnet-install.sh --channel 9.0 --install-dir /usr/local/share/dotnet && \
    rm dotnet-install.sh && \
    ln -vs /usr/local/share/dotnet/dotnet /usr/local/bin/dotnet && \
    dotnet --info

ENV PATH="/venv/bin:$PATH" \
    DOTNET_CLI_TELEMETRY_OPTOUT=1

# Install Python dependencies
COPY modules/embit/requirements.txt /tmp/requirements.txt
RUN --mount=type=cache,target=/root/.cache/pip,id=fuzz-pip \
    python3 -m venv /venv && \
    python3 -m ensurepip && \
    python3 -m pip install --upgrade pip && \
    python3 -m pip install mako setuptools && \
    python3 -m pip install -r /tmp/requirements.txt

# Get the source
COPY . .
RUN git submodule update --init --recursive .

# Lastly envs
ENV CC=/usr/bin/clang-18 \
    CXX=/usr/bin/clang++-18 \
    LDFLAGS="-lsodium"

ARG CXXFLAGS
RUN \
    --mount=type=cache,target=/root/.cache/go-build,id=fuzz-go-build \
    --mount=type=cache,target=/root/.cache/pip,id=fuzz-pip-build \
    --mount=type=cache,target=/root/.cargo/git,id=fuzz-cargo-git \
    --mount=type=cache,target=/root/.cargo/registry,id=fuzz-cargo-registry \
    --mount=type=cache,target=/root/.gradle,id=fuzz-gradle \
    --mount=type=cache,target=/root/.m2,id=fuzz-maven \
    --mount=type=cache,target=/root/.nuget/packages,id=fuzz-nuget-build \
    --mount=type=cache,target=/root/.rustup,id=fuzz-rustup \
    --mount=type=cache,target=/root/go/pkg/mod,id=fuzz-go-mod \
    export JAVA_HOME=/usr/lib/jvm/java-21-openjdk-$(dpkg --print-architecture) && \
    /build/auto_build.sh

FROM base AS runner

LABEL org.opencontainers.image.title="Bitcoin Fuzz"
LABEL org.opencontainers.image.description="Bitcoin fuzzing framework for security testing Bitcoin implementations"
LABEL org.opencontainers.image.source="https://github.com/bitcoinfuzz/bitcoinfuzz"
LABEL org.opencontainers.image.licenses="MIT"

WORKDIR /app

VOLUME ["/app/data"]
ARG FUZZ
ENV FUZZ_DATADIR=/app/data/${FUZZ} \
    FUZZ=${FUZZ}


RUN if [ -z "${FUZZ}" ]; then \
    echo "FUZZ build arg var is required"; \
    exit 1; \
    fi && \
    mkdir -p ${FUZZ_DATADIR}

COPY --from=builder --chmod=0755 /build/bitcoinfuzz .
# shared libs
COPY --from=builder \
    --parents \
    --exclude=**/gradle-wrapper.jar \
    --exclude=**/eclair_extracted/ \
    /build/modules/*/lib /
COPY --from=builder /build/*.so .

# Copy only the symbolizer to avoid bloating the base image
COPY --from=builder \
    /usr/lib/llvm-18/bin/llvm-symbolizer /usr/bin/llvm-symbolizer
ENV ASAN_SYMBOLIZER_PATH /usr/bin/llvm-symbolizer

# interpreted language sources
COPY --from=builder --parents \
    --exclude=modules/bitcoin/secp256k1/tools \
    /build/./modules/**/*.py .

# Transform envs into cli params using the defaults (some sane defaults)
# from the website https://llvm.org/docs/LibFuzzer.html#options
# mkdir to init and make sure we have write permissions
#
# TODO: Fix the use of nproc to actually use cgroup based "cpus"
ENTRYPOINT mkdir -p $FUZZ_DATADIR/crash \
    $FUZZ_DATADIR/corpus \
    && exec /app/bitcoinfuzz \
    -artifact_prefix=${FUZZ_DATADIR}/crash/ \
    -merge_control_file=${FUZZ_DATADIR}/merge \
    \
    $( [ -n "${LIBFUZZ_SEED}" ] && echo "-seed=${LIBFUZZ_SEED}" ) \
    -runs=${LIBFUZZ_RUNS:--1} \
    -max_len=${LIBFUZZ_MAX_LEN:-10000} \
    -len_control=${LIBFUZZ_LEN_CONTROL:-100} \
    -timeout=${LIBFUZZ_TIMEOUT:-300} \
    -rss_limit_mb=${LIBFUZZ_RSS_LIMIT_MB:-1024} \
    -malloc_limit_mb=${LIBFUZZ_MALLOC_LIMIT_MB:-0} \
    -max_total_time=${LIBFUZZ_MAX_TOTAL_TIME:-0} \
    -merge=${LIBFUZZ_MERGE:-0} \
    $( [ -n "${LIBFUZZ_DICT}" ] && echo "-dict=${LIBFUZZ_DICT}" ) \
    $( [ -n "${LIBFUZZ_MINIMIZE_CRASH}" ] && echo "-minimize_crash=${LIBFUZZ_MINIMIZE_CRASH}" ) \
    -reload=${LIBFUZZ_RELOAD:-1} \
    -jobs=${LIBFUZZ_JOBS:-0} \
    -fork=${LIBFUZZ_FORKS:-$(nproc)} \
    $( [ -n "${LIBFUZZ_WORKERS}" ] && echo "-workers=${LIBFUZZ_WORKERS}" ) \
    -reduce_inputs=${LIBFUZZ_REDUCE_INPUTS:-1} \
    -print_pcs=${LIBFUZZ_PRINT_PCS:-0} \
    -print_final_stats=${LIBFUZZ_PRINT_FINAL_STATS:-1} \
    -detect_leaks=${LIBFUZZ_DETECT_LEAKS:-1} \
    -only_ascii=${LIBFUZZ_ONLY_ASCII:-0} \
    ${FUZZ_DATADIR}/corpus
