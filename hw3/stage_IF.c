// stage_IF.c - 참고 코드와 일치하도록 수정
#include "structure.h"

// 참고 코드와 동일한 변수명 사용
extern uint64_t inst_count;

void stage_IF() {
    // 참고 코드처럼 초기화
    memset(&if_id_latch, 0, sizeof(if_id_latch));
    
    if (registers.pc == 0xFFFFFFFF) {
        if_id_latch.valid = false;
        return;
    }

    if ((registers.pc & 0x3) || registers.pc + 3 >= MEMORY_SIZE) {
        if_id_latch.valid = false;
        return;
    }

    uint32_t instruction = 0;
    uint32_t pc = registers.pc;
    
    // 참고 코드와 동일한 방식으로 instruction 읽기
    for (int i = 0; i < 4; i++) {
        instruction = instruction << 8;
        instruction |= memory[pc + (3 - i)];
    }

    if_id_latch.next_pc = pc + 4;  
    if_id_latch.instruction = instruction;
    if_id_latch.pc = pc;
    if_id_latch.valid = true;
    
    if_id_latch.opcode = instruction >> 26;                                 // opcode 부분, [31-26]
    if_id_latch.reg_src = (instruction >> 21) & 0x0000001f;          // rs
    if_id_latch.reg_tar = (instruction >> 16) & 0x0000001f;          // rt
    if_id_latch.funct = instruction & 0x3f;                                 // funct
    if_id_latch.forward_a = 0;
    if_id_latch.forward_b = 0;
    if_id_latch.forward_a_val = 0;
    if_id_latch.forward_b_val = 0;
}