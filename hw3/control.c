#include "structure.h"

void initialize_control(Control_Signals* control) {
    control->regdst = 0;
    control->regwrite = 0;
    control->alusrc = 0;
    control->memtoreg = 0;
    control->memwrite = 0;
    control->memread = 0;
    control->branch = 0;
    control->jump = 0;
    control->aluop = 0;
    control->rs_ch = 0;  // rs를 사용하는지 여부
    control->rt_ch = 0;  // rt를 사용하는지 여부
    control->ex_skip = 0;  // 실행 단계를 건너뛸지 여부
}

void setup_control_signals(Instruction* inst, Control_Signals* control) {
    initialize_control(control);

    switch (inst->opcode) {
        case 0: // R-type
            control->regdst = 1;
            control->regwrite = 1;
            control->aluop = 2;
            control->rs_ch = 1;  // rs 사용
            control->rt_ch = 1;  // rt 사용
            
            if (inst->funct == 0x08) {  // jr
                control->jump = 1;
                control->regwrite = 0;
                control->ex_skip = 1;
                control->rt_ch = 0;  // jr은 rt 사용 안함
            }
            else if (inst->funct == 0x09) { // jalr
                control->jump = 1;
                control->regwrite = 1;
                control->ex_skip = 1;
                control->rt_ch = 0;  // jalr은 rt 사용 안함
            }
            else if (inst->funct == 0x00 || inst->funct == 0x02) { // sll, srl
                control->rs_ch = 0;  // shift 명령어는 rs 사용 안함
            }
            break;
            
        case 2: // j
            control->jump = 1;
            control->regwrite = 0;
            control->ex_skip = 1;
            break;
            
        case 3: // jal
            control->jump = 1;
            control->regwrite = 1;
            control->ex_skip = 1;
            break;
            
        case 4: // beq
        case 5: // bne
            control->branch = 1;
            control->aluop = 1;
            control->regwrite = 0;
            control->ex_skip = 1;
            control->rs_ch = 1;  // 비교를 위해 rs 사용
            control->rt_ch = 1;  // 비교를 위해 rt 사용
            break;
            
        case 8: // addi
        case 9: // addiu
            control->alusrc = 1;
            control->regwrite = 1;
            control->rs_ch = 1;  // rs 사용
            break;
            
        case 10: // slti
        case 11: // sltiu
            control->alusrc = 1;
            control->regwrite = 1;
            control->aluop = 3;
            control->rs_ch = 1;  // rs 사용
            break;
            
        case 12: // andi
            control->alusrc = 1;
            control->regwrite = 1;
            control->aluop = 4;
            control->rs_ch = 1;  // rs 사용
            break;
            
        case 13: // ori
            control->alusrc = 1;
            control->regwrite = 1;
            control->aluop = 5;
            control->rs_ch = 1;  // rs 사용
            break;
            
        case 15: // lui
            control->alusrc = 1;
            control->regwrite = 1;
            control->aluop = 6;
            // lui는 rs 사용하지 않음
            break;
            
        case 35: // lw
            control->alusrc = 1;
            control->memtoreg = 1;
            control->regwrite = 1;
            control->memread = 1;
            control->rs_ch = 1;  // 주소 계산을 위해 rs 사용
            break;
            
        case 43: // sw
            control->alusrc = 1;
            control->memwrite = 1;
            control->regwrite = 0;
            control->rs_ch = 1;  // 주소 계산을 위해 rs 사용
            control->rt_ch = 1;  // 저장할 데이터를 위해 rt 사용
            break;
    }
}