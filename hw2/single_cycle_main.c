#include "datapath.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <binary_file>\n", argv[0]);
        return 1;
    }

    FILE *fp = fopen(argv[1], "rb");
    if (!fp) {
        perror("Error opening file");
        return 1;
    }

    uint32_t buffer;
    size_t index = 0;
    while (fread(&buffer, sizeof(uint32_t), 1, fp)) {
        mem[index++] = (buffer >> 24) & 0xFF;
        mem[index++] = (buffer >> 16) & 0xFF;
        mem[index++] = (buffer >> 8) & 0xFF;
        mem[index++] = buffer & 0xFF;
    }
    fclose(fp);

    Registers r;
    init_Registers(&r);

    run(&r, mem);

    printf("\nProgram finished.\n");
    printf("Return value ($v0): %u\n", r.Reg[2]);
    printf("Total instructions executed: %u\n", instruction_count);
    printf("Memory access instructions: %u\n", mem_access_count);
    printf("Taken branches: %u\n", branch_taken_count);
    printf("R-type instructions: %u\n", R_count);
    printf("I-type instructions: %u\n", I_count);
    printf("J-type instructions: %u\n", J_count);
    return 0;
}