from setuptools import setup, Extension
import sys
import os

# --- Cross-Platform OpenMP Setup ---
# Default flags
compile_args = ['-O3']
link_args = []

if sys.platform == 'darwin':
    # macOS (clang)
    # We need to point to the Homebrew-installed libomp
    # NOTE: This path is for Apple Silicon (M1/M2/M3).
    # For Intel Macs, the path might be /usr/local/opt/libomp/...
    homebrew_prefix = '/opt/homebrew/opt/libomp'
    
    if not os.path.exists(homebrew_prefix):
        print("Warning: Homebrew libomp not found at {homebrew_prefix}")
        print("Build may fail. Install with 'brew install libomp'")

    openmp_compile_args = [
        '-Xpreprocessor', '-fopenmp',
        f'-I{homebrew_prefix}/include'
    ]
    openmp_link_args = [
        '-lomp',
        f'-L{homebrew_prefix}/lib'
    ]

elif sys.platform == 'win32':
    # Windows (MSVC compiler)
    openmp_compile_args = ['/openmp']
    openmp_link_args = [] # /openmp handles linking

else:
    # Linux (gcc) and other Unix
    openmp_compile_args = ['-fopenmp']
    openmp_link_args = ['-fopenmp']

compile_args.extend(openmp_compile_args)
link_args.extend(openmp_link_args)
# --- End Cross-Platform Setup ---


module = Extension(
    'warphybrid',
    # These are the lz4 source files
    sources=['warp_compress.c', 'lz4.c', 'lz4frame.c', 'lz4hc.c', 'xxhash.c'],
    include_dirs=['.'],
    # Add the platform-specific flags
    extra_compile_args=compile_args,
    extra_link_args=link_args,
)

setup(
    name='warphybrid',
    version='1.2.0', # Updated version for cross-platform
    description='A cross-platform, high-speed, multithreaded LZ4 compressor',
    ext_modules=[module],
)
