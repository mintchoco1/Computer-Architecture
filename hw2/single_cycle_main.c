#include "dataPath.h"

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
        for (int i = 0; i < 4; i++) {
            mem[index++] = (buffer >> (8 * i)) & 0xFF;
        }
    }
    fclose(fp);

    Registers r;
    init_registers();

    while (r.pc != 0xFFFFFFFF) {
        uint32_t old_regs[32];
        memcpy(old_regs, r.Reg, sizeof(old_regs));
        uint32_t old_pc = r.pc;

        Instruction inst = fetch(&r, mem, &inst);
        if (inst.raw == 0) 
            continue;
        instruction_count++;

        inst = decode(&inst);
        set_control_signals(&inst);
        uint32_t alu_result = 0;


        
        printf("Cycle %u:\n", instruction_count);
        printf("------------------------\n");
    }

    printf("\nProgram finished.\nReturn value ($v0): %u\n", r.Reg[2]);
    printf("Total instructions executed: %u\n", instruction_count);
    return 0;
}