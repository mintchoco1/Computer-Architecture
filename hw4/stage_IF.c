#include "structure.h"

extern uint64_t inst_count;

void stage_IF() {

    memset(&if_id_latch, 0, sizeof(if_id_latch));
    
    if (registers.pc == 0xFFFFFFFF) {
        if_id_latch.valid = false;
        printf("[IF] PC=0xFFFFFFFF (HALT)\n");
        return;
    }

    if ((registers.pc & 0x3) || registers.pc + 3 >= MEMORY_SIZE) {
        if_id_latch.valid = false;
        printf("[IF] PC=0x%08x (OUT OF BOUNDS)\n", registers.pc);
        return;
    }

    uint32_t instruction = 0;
    uint32_t pc = registers.pc;
    
    for (int i = 0; i < 4; i++) {
        instruction = instruction << 8;
        instruction |= memory[pc + (3 - i)];
    }

    if_id_latch.next_pc = pc + 4;  
    if_id_latch.instruction = instruction;
    if_id_latch.pc = pc;
    if_id_latch.valid = true;
    
    if_id_latch.opcode = instruction >> 26;                            
    if_id_latch.reg_src = (instruction >> 21) & 0x0000001f;       
    if_id_latch.reg_tar = (instruction >> 16) & 0x0000001f;         
    if_id_latch.funct = instruction & 0x3f;                                
    if_id_latch.forward_a = 0;
    if_id_latch.forward_b = 0;
    if_id_latch.forward_a_val = 0;
    if_id_latch.forward_b_val = 0;

    printf("[IF] ");
    print_instruction_details(pc, instruction);
    printf("\n");
}