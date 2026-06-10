#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Compressor declarations from ELFZ wrapper headers
int lz4_compress_c(const unsigned char *in, size_t in_sz, unsigned char **out, size_t *out_sz);
int zx0_compress_c(const unsigned char *in, size_t in_sz, unsigned char **out, size_t *out_sz);

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <lz4|zx0> <source_file> <output_header_file>\n", argv[0]);
        return 1;
    }

    const char *mode = argv[1];
    const char *source_path = argv[2];
    const char *output_path = argv[3];

    int use_zx0 = 0;
    if (strcmp(mode, "zx0") == 0) {
        use_zx0 = 1;
    } else if (strcmp(mode, "lz4") != 0) {
        fprintf(stderr, "Error: Unknown mode '%s'. Must be 'lz4' or 'zx0'.\n", mode);
        return 1;
    }

    // Read source binary
    FILE *f_in = fopen(source_path, "rb");
    if (!f_in) {
        perror("Error opening source file");
        return 1;
    }

    fseek(f_in, 0, SEEK_END);
    long file_sz = ftell(f_in);
    fseek(f_in, 0, SEEK_SET);

    if (file_sz <= 0) {
        fprintf(stderr, "Error: Source file is empty or invalid size.\n");
        fclose(f_in);
        return 1;
    }

    size_t decompressed_sz = (size_t)file_sz;
    unsigned char *in_buf = malloc(decompressed_sz);
    if (!in_buf) {
        perror("Allocation error for input buffer");
        fclose(f_in);
        return 1;
    }

    if (fread(in_buf, 1, decompressed_sz, f_in) != decompressed_sz) {
        perror("Error reading source file");
        free(in_buf);
        fclose(f_in);
        return 1;
    }
    fclose(f_in);

    unsigned char *out_buf = NULL;
    size_t compressed_sz = 0;
    int ret = 0;

    printf("Compressing '%s' using %s (original size: %zu bytes)...\n",
           source_path, use_zx0 ? "ZX0" : "LZ4-HC", decompressed_sz);

    if (use_zx0) {
        ret = zx0_compress_c(in_buf, decompressed_sz, &out_buf, &compressed_sz);
    } else {
        ret = lz4_compress_c(in_buf, decompressed_sz, &out_buf, &compressed_sz);
    }

    free(in_buf);

    if (ret != 0 || !out_buf || compressed_sz == 0) {
        fprintf(stderr, "Compression failed!\n");
        if (out_buf) free(out_buf);
        return 1;
    }

    printf("Compression completed successfully. Compressed size: %zu bytes (ratio: %.2f%%)\n",
           compressed_sz, (double)compressed_sz * 100.0 / decompressed_sz);

    // Write C header payload file
    FILE *f_out = fopen(output_path, "w");
    if (!f_out) {
        perror("Error opening output header file");
        free(out_buf);
        return 1;
    }

    fprintf(f_out, "/* Generated automatically by bin_packer. Do not modify. */\n");
    fprintf(f_out, "#ifndef GUI_PAYLOAD_H\n");
    fprintf(f_out, "#define GUI_PAYLOAD_H\n\n");
    fprintf(f_out, "#define GUI_DECOMPRESSED_SIZE %zu\n", decompressed_sz);
    fprintf(f_out, "#define GUI_COMPRESSED_SIZE %zu\n\n", compressed_sz);
    fprintf(f_out, "const unsigned char gui_compressed_data[GUI_COMPRESSED_SIZE] = {\n");

    for (size_t i = 0; i < compressed_sz; i++) {
        fprintf(f_out, "0x%02x", out_buf[i]);
        if (i < compressed_sz - 1) {
            fprintf(f_out, ", ");
        }
        if ((i + 1) % 12 == 0) {
            fprintf(f_out, "\n");
        }
    }
    fprintf(f_out, "\n};\n\n");
    fprintf(f_out, "#endif /* GUI_PAYLOAD_H */\n");

    fclose(f_out);
    free(out_buf);

    printf("Generated header: %s\n", output_path);
    return 0;
}
