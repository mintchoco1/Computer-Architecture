#include "structure.h"

void setup_control_signals(Instruction* inst, Control_Signals* control) {
    initialize_control(control);

    switch (inst->opcode) {
        case 0: // R-type
            control->regdst = 1;       // rd를 목적지 레지스터로 사용
            control->regwrite = 1;     // 레지스터 쓰기 활성화
            control->aluop = 2;        // R-type ALU 연산
            if (inst->funct == 0x08) {  // jr
                control->jump = 1;
                control->regwrite = 0; // 레지스터 쓰기 비활성화
            }
            else if (inst->funct == 0x09) { // jalr
                control->jump = 1;
                control->regwrite = 1; // PC+4를 rd에 저장
            }
            break;        
        case 2: // j
            control->jump = 1;
            control->regwrite = 0;     // 레지스터 쓰기 비활성화
            break;
        case 3: // jal
            control->jump = 1;
            control->regwrite = 0;     // $ra에 쓰기 위해 활성화
            break;
        case 4: // beq
        case 5: // bne
            control->branch = 1;
            control->aluop = 1;        // 뺄셈 연산 (비교용)
            control->regwrite = 0;     // 레지스터 쓰기 비활성화
            break;
        case 8: // addi
        case 9: // addiu
            control->alusrc = 1;       // immediate 값 사용
            control->regwrite = 1;     // 레지스터 쓰기 활성화
            break;
        case 10: // slti
        case 11: // sltiu
            control->alusrc = 1;       // immediate 값 사용
            control->regwrite = 1;     // 레지스터 쓰기 활성화
            control->aluop = 3;        // 비교 연산
            break;
        case 12: // andi
            control->alusrc = 1;       // immediate 값 사용
            control->regwrite = 1;     // 레지스터 쓰기 활성화
            control->aluop = 4;        // AND 연산
            break;
        case 13: // ori
            control->alusrc = 1;       // immediate 값 사용
            control->regwrite = 1;     // 레지스터 쓰기 활성화
            control->aluop = 5;        // OR 연산
            break;
        case 15: // lui
            control->alusrc = 1;       // immediate 값 사용
            control->regwrite = 1;     // 레지스터 쓰기 활성화
            control->aluop = 6;        // LUI 연산
            break;
        case 35: // lw
            control->alusrc = 1;       // immediate 값 사용
            control->memtoreg = 1;    // 메모리 데이터 사용
            control->regwrite = 1;     // 레지스터 쓰기 활성화
            control->memread = 1;      // 메모리 읽기 활성화
            break;
        case 43: // sw
            control->alusrc = 1;       // immediate 값 사용
            control->memwrite = 1;     // 메모리 쓰기 활성화
            control->regwrite = 0;     // 레지스터 쓰기 비활성화
            break;
    }
}