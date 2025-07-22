#include "rle.h"

size_t compress_rle(uint8_t* out, const uint8_t* in, size_t in_size) {
    size_t in_pos = 0, out_pos = 0;
    uint8_t ctrl_byte = 0;
    size_t ctrl_pos = out_pos++; // reserve byte for control mask
    int bit_index = 0;

    while (in_pos < in_size) {
        size_t run_len = 1;

        // count run length up to 255
        while (in_pos + run_len < in_size &&
               in[in_pos] == in[in_pos + run_len] &&
               run_len < 255) {
            run_len++;
        }

        if (run_len >= 2) {
            ctrl_byte |= (1 << bit_index);       // bit=1 -> run-length encoded block
            out[out_pos++] = (uint8_t)run_len;
            out[out_pos++] = in[in_pos];
            in_pos += run_len;
        } else {
            out[out_pos++] = in[in_pos++];       // bit=0 -> literal byte
        }

        bit_index++;

        // write control byte every 8 blocks
        if (bit_index == 8) {
            out[ctrl_pos] = ctrl_byte;
            ctrl_pos = out_pos++;
            ctrl_byte = 0;
            bit_index = 0;
        }
    }

    // write the last control byte if the block count is less than 8
    if (bit_index > 0) {
        out[ctrl_pos] = ctrl_byte;
    }

    return out_pos;
}

size_t decompress_rle(uint8_t* out, const uint8_t* in, size_t in_size) {
    size_t in_pos = 0, out_pos = 0;

    while (in_pos < in_size) {
        uint8_t ctrl = in[in_pos++];

        for (int bit = 0; bit < 8 && in_pos < in_size; bit++) {
            if (ctrl & (1 << bit)) {
                // run-length block
                if (in_pos + 1 >= in_size) break;
                uint8_t count = in[in_pos++];
                uint8_t sym = in[in_pos++];
                for (uint8_t i = 0; i < count; i++) {
                    out[out_pos++] = sym;
                }
            } else {
                // literal byte
                if (in_pos >= in_size) break;
                out[out_pos++] = in[in_pos++];
            }
        }
    }

    return out_pos;
}
