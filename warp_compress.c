#include <Python.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>     // For multithreading
#include "lz4.h"
#include "lz4frame.h"
#include "lz4hc.h"

// Set max to 512MB to allow for large block testing
#define MAX_BLOCK_SIZE (512 * 1024 * 1024) 
#define HEADER_SIZE 8  // 4 bytes original block size + 4 bytes compressed size

/* We need a struct to hold block results */
typedef struct {
    size_t offset_in;
    size_t orig_size;
    int comp_size;
    unsigned char* comp_buf; // This will point to the final, exact-sized buffer
} BlockResult;


/* Compress function (The "Champion" V4 Version) */
static PyObject* compress_hybrid(PyObject* self, PyObject* args) {
    Py_buffer input;
    size_t block_size; 

    if (!PyArg_ParseTuple(args, "y*L", &input, &block_size)) {
        PyErr_SetString(PyExc_TypeError, "Expected bytes and block_size (in bytes)");
        return NULL;
    }

    if (block_size > MAX_BLOCK_SIZE) {
        PyErr_Format(PyExc_ValueError, "Block size %Ld exceeds MAX_BLOCK_SIZE %lld", block_size, (long long)MAX_BLOCK_SIZE);
        return NULL;
    }
    if (block_size == 0) {
        PyErr_SetString(PyExc_ValueError, "Block size cannot be zero");
        return NULL;
    }

    const unsigned char* in_data = input.buf;
    size_t in_size = input.len;
    size_t num_blocks = (in_size + block_size - 1) / block_size;
    
    BlockResult* results = calloc(num_blocks, sizeof(BlockResult));
    if (!results) return PyErr_NoMemory();

    size_t total_comp_size = 0;
    PyObject* output = NULL; 

    #pragma omp parallel for schedule(dynamic) reduction(+:total_comp_size)
    for (size_t i = 0; i < num_blocks; ++i) {
        size_t offset_in = i * block_size;
        size_t orig_size = (offset_in + block_size <= in_size) ? block_size : (in_size - offset_in);

        results[i].offset_in = offset_in;
        results[i].orig_size = orig_size;

        // This is the simpler V4 allocation strategy (malloc-in-loop)
        // which we proved was just as fast as the complex one.
        size_t max_comp = LZ4_compressBound((int)orig_size);
        unsigned char* comp_buf = malloc(max_comp);
        
        if (!comp_buf) {
            #pragma omp critical
            {
                if (!PyErr_Occurred()) PyErr_NoMemory();
            }
            continue; 
        }

        int comp_size = LZ4_compress_default(
            (const char*)(in_data + offset_in),
            (char*)comp_buf,
            (int)orig_size,
            (int)max_comp
        );

        if (comp_size > 0 && comp_size < orig_size) {
            results[i].comp_size = comp_size;
            unsigned char* realloc_buf = realloc(comp_buf, comp_size);
            if (realloc_buf) results[i].comp_buf = realloc_buf;
            else results[i].comp_buf = comp_buf;
        } else {
            results[i].comp_size = (int)orig_size;
            memcpy(comp_buf, in_data + offset_in, orig_size);
            results[i].comp_buf = comp_buf;
        }
        total_comp_size += results[i].comp_size + HEADER_SIZE;
    }

    if (PyErr_Occurred()) { 
        for (size_t i = 0; i < num_blocks; ++i) free(results[i].comp_buf);
        free(results);
        return NULL; 
    }

    output = PyBytes_FromStringAndSize(NULL, total_comp_size);
    if (!output) {
        for (size_t i = 0; i < num_blocks; ++i) free(results[i].comp_buf);
        free(results);
        return PyErr_NoMemory();
    }
    
    unsigned char* out_data = (unsigned char*)PyBytes_AS_STRING(output);
    size_t out_offset = 0;

    for (size_t i = 0; i < num_blocks; ++i) {
        uint32_t orig_size_32 = (uint32_t)results[i].orig_size;
        uint32_t comp_size_32 = (uint32_t)results[i].comp_size;

        memcpy(out_data + out_offset, &orig_size_32, 4);
        memcpy(out_data + out_offset + 4, &comp_size_32, 4);
        memcpy(out_data + out_offset + HEADER_SIZE, results[i].comp_buf, comp_size_32);
        
        out_offset += HEADER_SIZE + comp_size_32;
        free(results[i].comp_buf); 
    }
    
    free(results);
    PyBuffer_Release(&input);
    return output;
}


// --- NEW: Block Index struct for parallel decompression ---
typedef struct {
    size_t in_offset;   // Start of compressed data
    size_t out_offset;  // Start of decompressed data
    uint32_t comp_size;
    uint32_t orig_size;
} BlockIndex;


/* Decompress function (NEW: Multithreaded) */
static PyObject* decompress_hybrid(PyObject* self, PyObject* args) {
    Py_buffer input;
    if (!PyArg_ParseTuple(args, "y*", &input)) return NULL;

    const unsigned char* in_data = input.buf;
    size_t in_size = input.len;
    size_t in_offset = 0;
    
    size_t num_blocks = 0;
    size_t total_uncompressed_size = 0;
    BlockIndex* index = NULL;
    size_t index_capacity = 1024; // Start with capacity for 1024 blocks

    index = malloc(index_capacity * sizeof(BlockIndex));
    if (!index) return PyErr_NoMemory();

    // --- PASS 1: Build the block index (single-threaded) ---
    while (in_offset + HEADER_SIZE <= in_size) {
        uint32_t block_size, comp_size;
        memcpy(&block_size, in_data + in_offset, 4);
        memcpy(&comp_size, in_data + in_offset + 4, 4);

        if (block_size > MAX_BLOCK_SIZE || comp_size > in_size - (in_offset + HEADER_SIZE)) {
            free(index);
            PyBuffer_Release(&input);
            return PyErr_Format(PyExc_RuntimeError, "Hybrid decompression failed: invalid block header (size %u)", block_size);
        }

        // Grow index array if needed
        if (num_blocks >= index_capacity) {
            index_capacity *= 2;
            BlockIndex* new_index = realloc(index, index_capacity * sizeof(BlockIndex));
            if (!new_index) {
                free(index);
                PyBuffer_Release(&input);
                return PyErr_NoMemory();
            }
            index = new_index;
        }

        // Store block info
        index[num_blocks].in_offset = in_offset + HEADER_SIZE;
        index[num_blocks].out_offset = total_uncompressed_size;
        index[num_blocks].orig_size = block_size;
        index[num_blocks].comp_size = comp_size;

        // Move to next block
        in_offset += HEADER_SIZE + comp_size;
        total_uncompressed_size += block_size;
        num_blocks++;
    }

    if (in_offset != in_size) {
        free(index);
        PyBuffer_Release(&input);
        return PyErr_Format(PyExc_RuntimeError, "Hybrid decompression failed: trailing data");
    }

    // --- PASS 2: Decompress all blocks in parallel ---
    PyObject* out = PyBytes_FromStringAndSize(NULL, total_uncompressed_size); 
    if (!out) {
        free(index);
        PyBuffer_Release(&input);
        return PyErr_NoMemory();
    }
    
    unsigned char* out_data = (unsigned char*)PyBytes_AS_STRING(out);
    int error_flag = 0;

    #pragma omp parallel for schedule(dynamic)
    for (size_t i = 0; i < num_blocks; ++i) {
        if (error_flag) continue; // Stop if an error has occurred in another thread

        const BlockIndex* block = &index[i];
        const unsigned char* in_ptr = in_data + block->in_offset;
        unsigned char* out_ptr = out_data + block->out_offset;
        
        if (block->comp_size == block->orig_size) {
            // Data was stored uncompressed, just copy it
            memcpy(out_ptr, in_ptr, block->orig_size);
        } else {
            // Data is LZ4 compressed, decompress it
            int decomp_size = LZ4_decompress_safe(
                (const char*)in_ptr,
                (char*)out_ptr,
                (int)block->comp_size,
                (int)block->orig_size
            );
            if (decomp_size != (int)block->orig_size) {
                #pragma omp critical
                {
                    if (!PyErr_Occurred()) {
                        PyErr_Format(PyExc_RuntimeError, "Hybrid decompression failed: LZ4 size mismatch");
                    }
                    error_flag = 1;
                }
            }
        }
    } // --- END PARALLEL LOOP ---

    free(index);
    PyBuffer_Release(&input);
    
    if (error_flag) {
        Py_DECREF(out);
        return NULL;
    }
    
    return out;
}


/* Python Module Definitions */
static PyMethodDef WarpHybridMethods[] = {
    {"compress_hybrid", compress_hybrid, METH_VARARGS, "Compress using Blocked LZ4 (multithreaded).\nArgs: (data_bytes, block_size_in_bytes)"},
    {"decompress_hybrid", decompress_hybrid, METH_VARARGS, "Decompress Blocked LZ4 (multithreaded)"},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef warphybridmodule = {
    PyModuleDef_HEAD_INIT,
    "warphybrid",
    NULL,
    -1,
    WarpHybridMethods
};

PyMODINIT_FUNC PyInit_warphybrid(void) {
    return PyModule_Create(&warphybridmodule);
}
