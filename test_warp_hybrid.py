import zlib
import lzma
import time
import traceback
import warphybrid  # Your compiled C module
import os
import sys

# --- Benchmark Function (Unchanged) ---
def benchmark(name, comp_func, decomp_func, data, label):
    print(f"\n--- {label:<20} | {len(data) / 1024 / 1024:.2f} MB ---")
    try:
        t0 = time.time()
        compressed = comp_func(data)
        t1 = time.time()
    except Exception as e:
        print(f"{name:<10} ❌ failed during compression: {type(e).__name__}: {e}")
        traceback.print_exc()
        return

    try:
        t2 = time.time()
        decompressed = decomp_func(compressed)
        t3 = time.time()
    except Exception as e:
        print(f"{name:<10} ❌ failed during decompression: {type(e).__name__}: {e}")
        traceback.print_exc()
        return

    if decompressed != data:
        print(f"{name:<10} ❌ Data mismatch (length {len(data)} vs {len(decompressed)})")
        return

    ratio = 100.0 * len(compressed) / len(data) if len(data) > 0 else 0
    print(f"{name:<10} ✅ Ratio: {ratio:6.2f}%  C:{t1 - t0:.4f}s  D:{t3 - t2:.4f}s")

# --- "Smart" Wrapper (Unchanged) ---
KB = 1024
MB = 1024 * 1024
GB = 1024 * 1024 * 1024
DEFAULT_BLOCK_SIZE = 1 * MB

def compress_hybrid(data, block_size_bytes=None):
    if block_size_bytes is None:
        if len(data) < DEFAULT_BLOCK_SIZE:
            block_size_to_use = len(data)
        else:
            block_size_to_use = DEFAULT_BLOCK_SIZE
    else:
        block_size_to_use = block_size_bytes
        
    if block_size_to_use == 0 and len(data) > 0:
        block_size_to_use = len(data)
    elif len(data) == 0:
        return b'' # Handle 0KB file

    return warphybrid.compress_hybrid(data, block_size_to_use)

def decompress_hybrid(data):
    if len(data) == 0:
        return b'' # Handle 0KB file
    return warphybrid.decompress_hybrid(data)

# --- Data Generation/Loading Functions ---
def make_data(size, compressible=True):
    if size == 0:
        return b''
    if compressible:
        return (b'XYZ123' * (size // 6 + 1))[:size]
    else:
        from os import urandom
        return urandom(size)

def load_data_from_disk(filename):
    if not os.path.exists(filename):
        print(f"Error: File not found: {filename}")
        print(f"Please create it first with the 'dd' command.")
        return None
    print(f"\nLoading {filename} ({os.path.getsize(filename)/MB:.2f} MB)...")
    with open(filename, "rb") as f:
        return f.read()

# --- NEW: Test Definitions ---
IN_MEMORY_TESTS = {
    "0 KB": 0,
    "50 KB": 50 * KB,
    "500 KB": 500 * KB,
}

ON_DISK_TESTS = {
    "1 MB":   (1*MB,   "1mb_c.bin",   "1mb_r.bin"),
    "5 MB":   (5*MB,   "5mb_c.bin",   "5mb_r.bin"),
    "10 MB":  (10*MB,  "10mb_c.bin",  "10mb_r.bin"),
    "100 MB": (100*MB, "100mb_c.bin", "100mb_r.bin"),
    "250 MB": (250*MB, "250mb_c.bin", "250mb_r.bin"),
    "500 MB": (500*MB, "500mb_c.bin", "500mb_r.bin"),
    "1 GB":   (1*GB,   "1gb_c.bin",   "1gb_r.bin"),
    "5 GB":   (5*GB,   "5gb_c.bin",   "5gb_r.bin"),
}
# --- END NEW ---


if __name__ == "__main__":
    
    # --- Run In-Memory Tests ---
    for label, size_bytes in IN_MEMORY_TESTS.items():
        print(f"\n=============================================")
        print(f"  Testing {label} In-Memory")
        print(f"=============================================")
        
        data_c = make_data(size_bytes, compressible=True)
        data_i = make_data(size_bytes, compressible=False)
        
        benchmark("zlib", zlib.compress, zlib.decompress, data_c, f"{label} Compressible")
        benchmark("Hybrid", compress_hybrid, decompress_hybrid, data_c, f"{label} Compressible")
        
        benchmark("zlib", zlib.compress, zlib.decompress, data_i, f"{label} Incompressible")
        benchmark("Hybrid", compress_hybrid, decompress_hybrid, data_i, f"{label} Incompressible")

    # --- Run On-Disk Tests ---
    for label, (size_bytes, c_file, r_file) in ON_DISK_TESTS.items():
        
        # --- Test Compressible ---
        print(f"\n=============================================")
        print(f"  Testing {label} On-Disk (Compressible)")
        print(f"=============================================")
        data = load_data_from_disk(c_file)
        if data:
            if len(data) != size_bytes:
                print(f"Warning: File {c_file} size mismatch. Expected {size_bytes}, got {len(data)}")
            
            benchmark("zlib", zlib.compress, zlib.decompress, data, f"{label} (zlib)")
            benchmark("Hybrid", compress_hybrid, decompress_hybrid, data, f"{label} (Hybrid)")
            
            # Clear memory
            data = None 
        
        # --- Test Incompressible ---
        print(f"\n=============================================")
        print(f"  Testing {label} On-Disk (Incompressible)")
        print(f"=============================================")
        data = load_data_from_disk(r_file)
        if data:
            if len(data) != size_bytes:
                print(f"Warning: File {r_file} size mismatch. Expected {size_bytes}, got {len(data)}")

            benchmark("zlib", zlib.compress, zlib.decompress, data, f"{label} (zlib)")
            benchmark("Hybrid", compress_hybrid, decompress_hybrid, data, f"{label} (Hybrid)")
            
            # Clear memory
            data = None

    print("\n=============================================")
    print("  Full Benchmark Suite Complete.")
    print("=============================================")
