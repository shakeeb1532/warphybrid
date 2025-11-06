warphybrid: A High-Speed, Multithreaded Compression Library
warphybrid is a high-performance, multithreaded Python C-extension for compression, built on the LZ4 algorithm. It is designed to be a "drop-in" replacement for zlib in any application where speed is more critical than ratio.

It is consistently 80x-200x faster at compression and 2x-5x faster at decompression than the standard zlib library, while also being more robust by supporting files larger than 4GB.

The Problem
Standard zlib (the engine behind gzip) is a single-threaded bottleneck. In modern, high-traffic applications, zlib's CPU-intensive compression and slow decompression add significant latency to API calls, cache reads, and logging pipelines.

The warphybrid Solution
warphybrid is built for speed and scales with modern multi-core CPUs.

Fast Core: Uses the LZ4 algorithm, famous for its "at-speed" compression and blazing-fast decompression.

Fully Multithreaded: Uses OpenMP to parallelize both compression and decompression over all available CPU cores.

Intelligent Block Design: Data is split into 1MB blocks, which provides the optimal balance of ratio and parallelism.

Robust & Smart:

>4GB Support: Natively handles massive files. Our 5GB test completed successfully while zlib failed.

Incompressible Data Bypass: Intelligently detects pre-compressed (e.g., PDF, JPG) or random data and stores it raw, turning a slow decompression into an ultra-fast memcpy.

Performance Benchmarks (vs. zlib)
Benchmarks were run on an M1 Mac. The 1GB test shows the massive, scalable advantage of warphybrid.

1GB Compressible Data (e.g., JSON, Text, Logs)
_________________________________________________
Compressor	|Compression	|Decompression	|Ratio
_________________________________________________
zlib	      |4.109 s	    |0.283 s	      |0.10%
_________________________________________________
warphybrid	|0.020 s	    |0.068 s	      |0.39%
_________________________________________________
Speed-Up	  |200x Faster	|4.1x Faster	  |
_________________________________________________


1GB Incompressible Data (e.g., Random, Encrypted, Videos)
__________________________________________________
Compressor	|Compression	|Decompression	|Ratio
--------------------------------------------------
zlib	      |19.576 s	    |0.210 s	      |100.03%
--------------------------------------------------
warphybrid	|0.227 s	    |0.093 s	      |100.00%
--------------------------------------------------
Speed-Up	  |86x Faster	  |2.2x Faster	
---------------------------------------------------


5GB Compressible Data (Stress Test)
Compressor	Compression	Decompression	Ratio
zlib	FAILED	FAILED	N/A
warphybrid	0.152 s	0.619 s	0.39%

Export to Sheets

Build & Installation
This project is a Python C-extension and must be compiled.

1. Prerequisites
You need a C compiler and the lz4 and OpenMP libraries.

macOS (Apple Silicon / Intel):

Bash

# Install Homebrew
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install dependencies
brew install lz4 libomp
Linux (Debian/Ubuntu):

Bash

sudo apt-get update
sudo apt-get install -y build-essential python3-dev liblz4-dev libomp-dev
Windows:

Install the "C++ build tools" from the Visual Studio Build Tools.

Install lz4 via vcpkg.

The setup.py is configured to use the MSVC compiler's /openmp flag.

2. Compile the Module
Once dependencies are installed, you can build the module from the project root.

Bash

# Create and activate a virtual environment
python3 -m venv venv
source venv/bin/activate

# Install build tools
pip install setuptools

# Build the C-extension
python3 setup.py build_ext --inplace
If this succeeds, you will have a warphybrid.so (or warphybrid.pyd on Windows) file in your directory.

How to Use
The module's API is simple and designed to be familiar.

Python

import warphybrid
import os

# --- 1. Smart Default Compression ---
# The compressor will intelligently use a 1MB block size
# or the file size if it's smaller.
my_data = b"This is some data to compress" * 1000
compressed_data = warphybrid.compress_hybrid(my_data)

# --- 2. Manual Override (for tuning) ---
# You can also specify a block size in bytes.
KB_100 = 100 * 1024
compressed_100kb_blocks = warphybrid.compress_hybrid(my_data, KB_100)

# --- 3. Decompression ---
# Decompression is multithreaded and automatic.
decompressed_data = warphybrid.decompress_hybrid(compressed_data)

assert my_data == decompressed_data
print("Success!")
Running the Benchmark
The included test script can be used to run benchmarks and create large test files.

Bash

# Create the test files (1GB+ files will take time)
python3 test_warp_hybrid.py --create-files

# Run the full benchmark suite
python3 test_warp_hybrid.py
