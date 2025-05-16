#include "structure.h"

void execute_cycle(Registers* registers, uint8_t* memory) {
    InstructionInfo info;
    ControlSignals control;
    
    uint32_t instruction = instruction_fetch(registers, memory);
    
    if (instruction == 0) {
        printf("\tNOP\n\n");
        return;
    }
    
    instruction_decode(instruction, registers, &info, &control);
    uint32_t alu_result = execute_instruction(&info, &control, registers);
    uint32_t memory_data = memory_access(alu_result, info.rt_value, &control, &info);
    write_back(alu_result, memory_data, &info, &control, registers);
    printf("\n");
}

void run_processor(Registers* registers, uint8_t* memory) {
    while (registers->program_counter != 0xffffffff) {
        printf("================================\n");
        printf("Cycle : %d\n", instruction_count);
        
        execute_cycle(registers, memory);
    }

    printf("===== Final Result =====\n");
    printf("Cycles: %d, R-type instructions: %d, I-type instructions: %d, J-type instructions: %d\n", instruction_count, rtype_count, itype_count, jtype_count);
    printf("Memory operations: %d, Branch instructions: %d\n", memory_count, branch_count);
    printf("Return value(v0) : %d (0x%x)\n", registers->regs[2], registers->regs[2]);
}