#include "datapath.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <binary file>\n", argv[0]);
        return 1;
    }

    FILE *fp = fopen(argv[1], "rb");
    if (!fp) {
        perror("File opening failed");
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

    Regs regs;
    init_regs(&regs);

    datapath(&regs, mem);

    printf("\n===== Program Finished =====\n");
    printf("Return value (v0) = %u\n", regs.reg[2]);
    printf("Total Instructions Executed: %d\n", inst_cnt);
    printf("R-type Instructions: %d\n", r_cnt);
    printf("I-type Instructions: %d\n", i_cnt);
    printf("J-type Instructions: %d\n", j_cnt);
    printf("Memory Access Instructions: %d\n", ma_cnt);
    printf("Taken Branches: %d\n", b_cnt);

    return 0;
}
