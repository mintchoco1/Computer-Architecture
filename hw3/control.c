#include "structure.h"

void initialize_control(ControlSignals* control) {
    control->reg_dst = 0;
    control->reg_write = 0;
    control->alu_src = 0;
    control->mem_to_reg = 0;
    control->mem_read = 0;
    control->mem_write = 0;
    control->branch = 0;
    control->jump = 0;
    control->alu_op = 0;
}


void setup_control_signals(InstructionInfo* info, ControlSignals* control) {
    initialize_control(control);

    switch (info->opcode) {
        case 0: // R-type
            control->reg_dst = 1;       // rd를 목적지 레지스터로 사용
            control->reg_write = 1;     // 레지스터 쓰기 활성화
            control->alu_op = 2;        // R-type ALU 연산
            if (info->funct == 0x08) {  // jr
                control->jump = 1;
                control->reg_write = 0; // 레지스터 쓰기 비활성화
            }
            else if (info->funct == 0x09) { // jalr
                control->jump = 1;
                control->reg_write = 1; // PC+4를 rd에 저장
            }
            break;        
        case 2: // j
            control->jump = 1;
            control->reg_write = 0;     // 레지스터 쓰기 비활성화
            break;
        case 3: // jal
            control->jump = 1;
            control->reg_write = 0;     // $ra에 쓰기 위해 활성화
            break;
        case 4: // beq
        case 5: // bne
            control->branch = 1;
            control->alu_op = 1;        // 뺄셈 연산 (비교용)
            control->reg_write = 0;     // 레지스터 쓰기 비활성화
            break;
        case 8: // addi
        case 9: // addiu
            control->alu_src = 1;       // immediate 값 사용
            control->reg_write = 1;     // 레지스터 쓰기 활성화
            break;
        case 10: // slti
        case 11: // sltiu
            control->alu_src = 1;       // immediate 값 사용
            control->reg_write = 1;     // 레지스터 쓰기 활성화
            control->alu_op = 3;        // 비교 연산
            break;
        case 12: // andi
            control->alu_src = 1;       // immediate 값 사용
            control->reg_write = 1;     // 레지스터 쓰기 활성화
            control->alu_op = 4;        // AND 연산
            break;
        case 13: // ori
            control->alu_src = 1;       // immediate 값 사용
            control->reg_write = 1;     // 레지스터 쓰기 활성화
            control->alu_op = 5;        // OR 연산
            break;
        case 15: // lui
            control->alu_src = 1;       // immediate 값 사용
            control->reg_write = 1;     // 레지스터 쓰기 활성화
            control->alu_op = 6;        // LUI 연산
            break;
        case 35: // lw
            control->alu_src = 1;       // immediate 값 사용
            control->mem_to_reg = 1;    // 메모리 데이터 사용
            control->reg_write = 1;     // 레지스터 쓰기 활성화
            control->mem_read = 1;      // 메모리 읽기 활성화
            break;
        case 43: // sw
            control->alu_src = 1;       // immediate 값 사용
            control->mem_write = 1;     // 메모리 쓰기 활성화
            control->reg_write = 0;     // 레지스터 쓰기 비활성화
            break;
    }
}