#pragma once

#include <stdint.h>
#include <stddef.h>

/**
 * @brief Compress input data using RLE with control byte bitmask for runs and literals.
 * 
 * This function encodes the input buffer into an output buffer where each group of up to 8 blocks
 * is preceded by a control byte. Each bit in the control byte indicates whether the corresponding block
 * is a run-length encoded sequence (bit=1) or a single literal byte (bit=0).
 * 
 * @param out Pointer to the output buffer to store compressed data.
 * @param in Pointer to the input buffer to be compressed.
 * @param in_size Size of the input buffer in bytes.
 * @return Size of the compressed data written to the output buffer.
 */
size_t compress_rle(uint8_t* out, const uint8_t* in, size_t in_size);

/**
 * @brief Decompress data compressed by compress_rle().
 * 
 * This function reads the compressed input buffer with control bytes followed by blocks of either
 * run-length encoded sequences or literals, reconstructing the original uncompressed data.
 * 
 * @param out Pointer to the output buffer to store decompressed data.
 * @param in Pointer to the compressed input buffer.
 * @param in_size Size of the compressed input buffer in bytes.
 * @return Size of the decompressed data written to the output buffer.
 */
size_t decompress_rle(uint8_t* out, const uint8_t* in, size_t in_size);
