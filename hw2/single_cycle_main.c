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

    uint32_t temp;
    size_t index = 0;
    while (fread(&temp, sizeof(uint32_t), 1, fp)) {
        for (int i = 0; i < 4; i++) {
            memory[index++] = (temp >> (8 * i)) & 0xFF;
        }
    }
    fclose(fp);

    init_registers();

    while (REGS.pc != 0xFFFFFFFF) {
        uint32_t old_regs[32];
        memcpy(old_regs, REGS.Reg, sizeof(old_regs));
        uint32_t old_pc = REGS.pc;

        Instruction inst = fetch();
        if (inst.raw == 0) continue;
        instruction_count++;

        inst = decode(inst);
        set_control(inst);

        if (control.Jump) {
            if (control.JumpLink) REGS.Reg[31] = REGS.pc;
            REGS.pc = (REGS.pc & 0xF0000000) | (inst.address << 2);
        } else if (control.JumpReg) {
            if (control.JumpLink) REGS.Reg[31] = REGS.pc;
            REGS.pc = REGS.Reg[inst.rs];
        } else {
            uint32_t alu_result = execute(inst);

            if (control.Branch) {
                int taken = 0;
                if (inst.opcode == 0x04 && alu_result == 0) taken = 1; // beq
                if (inst.opcode == 0x05 && alu_result != 0) taken = 1; // bne
                if (taken) REGS.pc = old_pc + 4 + ((int16_t)inst.immediate << 2);
            }

            uint32_t mem_result = 0;
            memory_access(inst, alu_result, &mem_result);
            write_back(inst, mem_result);
        }

        printf("Cycle %u:\n", instruction_count);
        print_diff(old_regs, old_pc);
        printf("------------------------\n");
    }

    printf("\nProgram finished.\nReturn value ($v0): %u\n", REGS.Reg[2]);
    printf("Total instructions executed: %u\n", instruction_count);
    return 0;
}