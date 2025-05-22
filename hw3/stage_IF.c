#include "structure.h"

void stage_IF(){
    printf("IF stage");

    // 종료 조건 체크 (PC가 종료 명령어를 가리키는 경우)
    if (registers.pc == 0xFFFFFFFF) {
        if_id_latch.valid = false;
        printf("HALT: PC reached 0xFFFFFFFF\n");
        return;
    }

        // 메모리 범위 검사
    if (registers.pc + 3 >= MEMORY_SIZE) {
        printf("Memory access out of bounds at PC=0x%08x\n", registers.pc);
        if_id_latch.valid = false;
        return;
    }

    uint32_t instruction = 0;
    uint32_t pc = registers.pc;
    for (int i = 0; i < 4; i++){
        instruction = (instruction << 8) | memory[pc + (3 - i)];
    }

    if_id_latch.instruction = instruction;
    if_id_latch.pc = pc;
    if_id_latch.valid = true;

    printf("Fetched Instruction: 0x%08x\n", instruction);

    registers.pc += 4;
}