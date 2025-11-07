warphybrid: A High-Speed, Multithreaded Compression Library

warphybrid is a high-performance, multithreaded Python C-extension for compression, built on the LZ4 algorithm. It is designed to be a "drop-in" replacement for zlib in any application where speed is more critical than compression ratio.

It offers blazing-fast performance — up to 200x faster at compression and up to 5x faster at decompression than zlib — while also supporting files larger than 4GB.

Table of Contents

Introduction

Features

Performance Benchmarks

Installation

Prerequisites

Build Instructions

Usage

Benchmarking

Dependencies

Troubleshooting

License

Introduction

Standard zlib (the engine behind gzip) is a single-threaded bottleneck. In modern, high-traffic applications, its CPU-intensive compression and slow decompression can significantly increase latency in API calls, cache reads, and logging systems.

warphybrid solves this by using the LZ4 algorithm and fully utilizing modern multi-core CPUs for both compression and decompression.

Features

⚡ LZ4-Powered Core: High-speed compression/decompression.

🧵 Multithreaded: Uses OpenMP to parallelize operations across all CPU cores.

🧠 Intelligent Block Design: Uses 1MB blocks to balance speed and ratio.

📦 >4GB File Support: Natively handles massive files.

🚀 Incompressible Data Detection: Skips unnecessary compression for pre-compressed or random data, turning slow decompression into a simple memcpy.

Performance Benchmarks

System: M1 Mac
Data Size: 1GB and 5GB
Benchmark Script: test_warp_hybrid.py

📄 1GB Compressible Data (JSON, Text, Logs)
Compressor	Compression	Decompression	Ratio	Speed-Up
zlib	4.109 s	0.283 s	0.10%	—
warphybrid	0.020 s	0.068 s	0.39%	200x / 4.1x
🗃 1GB Incompressible Data (Random, Encrypted, Video)
Compressor	Compression	Decompression	Ratio	Speed-Up
zlib	19.576 s	0.210 s	100.03%	—
warphybrid	0.227 s	0.093 s	100.00%	86x / 2.2x
🧪 5GB Compressible Data (Stress Test)
Compressor	Compression	Decompression	Ratio
zlib	❌ FAILED	❌ FAILED	N/A
warphybrid	0.152 s	0.619 s	0.39%
Installation
Prerequisites

Python 3.x

C compiler with OpenMP support

lz4 and libomp libraries

macOS (Apple Silicon / Intel)
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
brew install lz4 libomp

Linux (Debian/Ubuntu)
sudo apt-get update
sudo apt-get install -y build-essential python3-dev liblz4-dev libomp-dev

Windows

Install Visual Studio Build Tools with C++ support.

Install lz4 via vcpkg
.

Ensure /openmp is enabled in the compiler options (set in setup.py).

Build Instructions
# Create and activate a virtual environment
python3 -m venv venv
source venv/bin/activate

# Install build tools
pip install setuptools

# Compile the C-extension
python3 setup.py build_ext --inplace


You should now see warphybrid.so (Linux/macOS) or warphybrid.pyd (Windows) in your directory.

Usage

The API is designed to be minimal and familiar to zlib users.

import warphybrid

data = b"This is some data to compress" * 1000

# 1. Smart compression (1MB blocks by default)
compressed = warphybrid.compress_hybrid(data)

# 2. Optional: Manual block size (in bytes)
compressed_custom = warphybrid.compress_hybrid(data, 100 * 1024)  # 100KB blocks

# 3. Decompression
decompressed = warphybrid.decompress_hybrid(compressed)

assert decompressed == data
print("Success!")

Benchmarking

Run the benchmark script to test performance and create large test files:

# Generate test files (1GB+)
python3 test_warp_hybrid.py --create-files

# Run benchmarks
python3 test_warp_hybrid.py

Dependencies

LZ4 – High-speed compression algorithm (lz4.github.io/lz4
)

OpenMP – For multithreading

Python C API – For building native extensions

Troubleshooting

❗ Missing OpenMP or lz4: Make sure libomp and lz4 are installed correctly.

🪟 Windows build issues: Ensure you're using MSVC with /openmp, and dependencies are available via vcpkg.

🐍 Python Version Errors: Confirm you're using a compatible Python version (3.x).

License

MIT License. See LICENSE
 for details.

Let me know if you want to:

Add author/contributor credits

Include a CHANGELOG.md or versioning section

Package it for PyPI (setup.py, pyproject.toml)

Add badges (build status, license, PyPI version)

I'm happy to help with any of those next.
