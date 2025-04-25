#include "dataPath.h"
#include <stdio.h>

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

    while (r.pc != 0xFFFFFFFF) {
        uint32_t old_regs[32];
        memcpy(old_regs, r.Reg, sizeof(old_regs));
        uint32_t old_pc = r.pc;

        Instruction inst;
        fetch(&r, mem, &inst);
        if (inst.mips_inst == 0) {
            printf("NOP\n\n");
            continue;
        }

        instruction_count++;
        decode(&inst);
        set_control_signals(&inst);

        uint32_t alu_result = 0;
        execute(&r, &inst, &alu_result);

        uint32_t mem_result = memory_access(&r, inst, alu_result);
        write_back(&r, inst, mem_result);

        print_diff(&r, old_regs, old_pc);
        printf("--------------------------\n");

        if (inst.opcode == 0x00) R_count++;
        else if (inst.opcode == 0x02 || inst.opcode == 0x03) J_count++;
        else I_count++;
    }

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