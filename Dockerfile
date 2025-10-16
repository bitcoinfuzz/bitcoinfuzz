# Uses Ubuntu as base
FROM ubuntu:24.04

# Sets environment variables for installation
ENV DEBIAN_FRONTEND=noninteractive

# Installs basic tools
RUN apt-get update && apt-get install -y \
    wget curl git build-essential sudo \
    libc6-dev libgcc-11-dev libasan6 \
    cmake \
    && rm -rf /var/lib/apt/lists/*

# Installs Rust
RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
ENV PATH="/root/.cargo/bin:${PATH}"
RUN rustup install stable && rustup install nightly

# Installs Go
RUN wget https://golang.org/dl/go1.22.2.linux-amd64.tar.gz && \
    tar -C /usr/local -xzf go1.22.2.linux-amd64.tar.gz && \
    rm go1.22.2.linux-amd64.tar.gz
ENV PATH="/usr/local/go/bin:${PATH}"

# Installs .NET SDK 9.0
RUN wget https://packages.microsoft.com/config/ubuntu/22.04/packages-microsoft-prod.deb -O packages-microsoft-prod.deb && \
    dpkg -i packages-microsoft-prod.deb && \
    rm packages-microsoft-prod.deb && \
    apt-get update && apt-get install -y dotnet-sdk-9.0 && \
    rm -rf /var/lib/apt/lists/*

# Installs Java 21 (OpenJDK 21 from Adoptium)
RUN wget https://github.com/adoptium/temurin21-binaries/releases/download/jdk-21.0.7%2B6/OpenJDK21U-jdk_x64_linux_hotspot_21.0.7_6.tar.gz && \
    mkdir -p /opt/java && \
    tar -xzf OpenJDK21U-jdk_x64_linux_hotspot_21.0.7_6.tar.gz -C /opt/java && \
    rm OpenJDK21U-jdk_x64_linux_hotspot_21.0.7_6.tar.gz

# Installs Python 3.11 and pip
RUN apt-get update && apt-get install -y python3 python3-pip python3-venv && \
    rm -rf /var/lib/apt/lists/*

# Installs LLVM and Clang 18
RUN apt-get update && apt-get install -y \
    lsb-release wget software-properties-common gnupg && \
    wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add - && \
    add-apt-repository "deb http://apt.llvm.org/jammy/ llvm-toolchain-jammy-18 main" && \
    apt-get update && apt-get install -y \
    clang-18 clang++-18 && \
    ln -sfT /usr/bin/clang++-18 /usr/bin/clang++ && \
    ln -sfT /usr/bin/clang-18 /usr/bin/clang && \
    rm -rf /var/lib/apt/lists/*

# Configures environment variables
ENV CC=/usr/bin/clang
ENV CXX=/usr/bin/clang++
ENV LDFLAGS="-lsodium"
ENV JAVA_HOME="/opt/java/jdk-21.0.7+6"
ENV PATH="${JAVA_HOME}/bin:${PATH}"

# Working directory
WORKDIR /app

# Copies the repository
COPY . .

# Creates a virtual environment
RUN python3 -m venv /venv
ENV PATH="/venv/bin:$PATH"

# Installs dependencies for embit
RUN pip install -r modules/embit/requirements.txt

# Installs dependencies for C-lightning
RUN python3 -m pip install --upgrade pip && \
    python3 -m pip install mako
RUN git submodule update --init --recursive external/lightning
RUN apt-get update && apt-get install -y \
    libsqlite3-dev \
    libsodium-dev \
    jq \
    && rm -rf /var/lib/apt/lists/*

# Clean cache bitcoin core
RUN rm -rf /app/modules/bitcoin/univalue/build && \
    mkdir -p /app/modules/bitcoin/univalue/build && \
    cd /app/modules/bitcoin/univalue && \
    CXXFLAGS="-fsanitize=address" cmake -B build && \
    cmake --build build && \
    cd build && \
    make univalue

# Copies and gives permission to auto_build.sh
COPY auto_build.sh .
RUN chmod +x auto_build.sh

# CMD requires FUZZ, but FUZZ_RUNS and FUZZ_INPUT are optional
CMD ["bash", "-c", "mkdir -p /app/data/crash && \
    ./auto_build.sh && \
    if [ -z \"$FUZZ\" ]; then \
    echo \"Error: FUZZ not defined\"; \
    exit 1; \
    elif [ -n \"$FUZZ_INPUT\" ]; then \
    ./bitcoinfuzz -artifact_prefix=/app/data/crash/ \"$FUZZ_INPUT\"; \
    elif [ -n \"$FUZZ_RUNS\" ]; then \
    ./bitcoinfuzz -runs=$FUZZ_RUNS -artifact_prefix=/app/data/crash/ /app/data; \
    else \
    ./bitcoinfuzz -artifact_prefix=/app/data/crash/ /app/data; \
    fi"]
