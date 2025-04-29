#include "datapath3.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

uint8_t mem[MEM_SIZE];

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <binary file>\n", argv[0]);
        return 1;
    }

    FILE *fp = fopen(argv[1], "rb");
    if (!fp) {
        perror("File open failed");
        return 1;
    }

    uint32_t buffer;
    size_t idx = 0;
    while (fread(&buffer, sizeof(uint32_t), 1, fp)) {
        mem[idx++] = (buffer >> 24) & 0xFF;
        mem[idx++] = (buffer >> 16) & 0xFF;
        mem[idx++] = (buffer >> 8) & 0xFF;
        mem[idx++] = buffer & 0xFF;
    }
    fclose(fp);

    uint32_t regs[32] = {0};
    init_regs(regs);
    datapath(regs);

    printf("\n===== Program Finished =====\n");
    printf("Return value (v0) = %u\n", regs[2]);
    printf("Total Instructions: %d\n", inst_cnt);
    printf("R-type: %d, I-type: %d, J-type: %d\n", r_cnt, i_cnt, j_cnt);
    printf("Memory Accesses: %d, Branch Taken: %d\n", mem_cnt, branch_cnt);

    return 0;
}
