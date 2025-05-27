#include "structure.h"

void stage_IF(){
    // 종료 조건 체크
    if (registers.pc == 0xFFFFFFFF) {
        if_id_latch.valid = false;
        printf("IF: HALT - PC reached 0xFFFFFFFF\n");
        return;
    }

    // 메모리 범위 검사
    if (registers.pc + 3 >= MEMORY_SIZE) {
        printf("IF: Memory access out of bounds at PC=0x%08x\n", registers.pc);
        if_id_latch.valid = false;
        return;
    }

    // 명령어 페치 (리틀 엔디안)
    uint32_t instruction = 0;
    uint32_t pc = registers.pc;
    
    // 참고 코드 스타일로 수정 - 리틀 엔디안 처리
    for (int i = 0; i < 4; i++){
        instruction = (instruction << 8) | memory[pc + (3 - i)];
    }

    // IF/ID 래치에 저장
    if_id_latch.instruction = instruction;
    if_id_latch.pc = pc;
    if_id_latch.valid = true;

    printf("IF: PC=0x%08x, Inst=0x%08x\n", pc, instruction);

    // PC 업데이트 (브랜치/점프에서 수정될 수 있음)
    registers.pc += 4;
}