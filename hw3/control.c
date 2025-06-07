#include "structure.h"

void initialize_control(Control_Signals* control) {
    control->alu_ctrl = 0;
    control->alu_op = 0;
    control->alu_src = 0;
    control->mem_read = 0;
    control->mem_write = 0;
    control->mem_to_reg = 0;
    control->reg_wb = 0;
    control->reg_dst = 0;
    control->get_imm = 0;
    control->ex_skip = 0;
    control->rs_ch = 0;
    control->rt_ch = 0;
}

// 참고 코드와 동일한 제어 신호 설정
void setup_control_signals(Instruction* inst, Control_Signals* control) {
    initialize_control(control);

    switch (inst->opcode) {
        case 0x2:  // j
            control->ex_skip = 1;
            break;
        case 0x3:  // jal
            control->ex_skip = 1;
            break;
        case 0x4:  // beq
            control->ex_skip = 1;
            control->rt_ch = 1;
            control->rs_ch = 1;
            break;
        case 0x5:  // bne
            control->ex_skip = 1;
            control->rt_ch = 1;
            control->rs_ch = 1;
            break;
        case 0x8:  // addi
            control->reg_wb = 1;
            control->get_imm = 1;
            control->alu_ctrl = 0b0010;
            control->alu_src = 1;
            control->rs_ch = 1;
            break;
        case 0x9:  // addiu
            control->reg_wb = 1;
            control->get_imm = 1;
            control->alu_ctrl = 0b0010;
            control->alu_src = 1;
            control->rs_ch = 1;
            break;
        case 0xA:  // slti
            control->get_imm = 1;
            control->alu_ctrl = 0b0111;
            control->alu_src = 1;
            control->reg_wb = 1;
            control->rs_ch = 1;
            break;
        case 0xB:  // sltiu
            control->get_imm = 1;
            control->alu_ctrl = 0b0111;
            control->alu_src = 1;
            control->reg_wb = 1;
            control->rs_ch = 1;
            break;
        case 0xC:  // andi
            control->alu_ctrl = 0b0000;
            control->alu_src = 1;
            control->reg_wb = 1;
            control->get_imm = 2;
            control->rs_ch = 1;
            break;
        case 0xD:  // ori
            control->alu_ctrl = 0b0001;
            control->alu_src = 1;
            control->reg_wb = 1;
            control->get_imm = 2;
            control->rs_ch = 1;
            break;
        case 0x23:  // lw
            control->reg_wb = 1;
            control->get_imm = 1;
            control->alu_ctrl = 0b0010;
            control->alu_src = 1;
            control->mem_read = 1;
            control->mem_to_reg = 1;
            control->rs_ch = 1;
            break;
        case 0x2B:  // sw
            control->get_imm = 1;
            control->alu_ctrl = 0b0010;
            control->alu_src = 1;
            control->mem_write = 1;
            control->rt_ch = 1;
            control->rs_ch = 1;
            break;
        case 0xF:  // lui
            control->get_imm = 3;
            control->reg_dst = 0;
            control->alu_src = 0;
            control->ex_skip = 0;
            control->reg_wb = 1;
            break;
        case 0x0:  // R-type
            switch (inst->funct) {
                case 0x20:  // Add
                    control->reg_wb = 1;
                    control->reg_dst = 1;
                    control->alu_ctrl = 0b0010;
                    control->rt_ch = 1;
                    control->rs_ch = 1;
                    break;
                case 0x21:  // Addu
                    control->reg_wb = 1;
                    control->reg_dst = 1;
                    control->alu_ctrl = 0b0010;
                    control->rt_ch = 1;
                    control->rs_ch = 1;
                    break;
                case 0x22:  // Subtract
                    control->reg_wb = 1;
                    control->reg_dst = 1;
                    control->alu_ctrl = 0b0110;
                    control->rt_ch = 1;
                    control->rs_ch = 1;
                    break;
                case 0x23:  // Subu
                    control->reg_wb = 1;
                    control->reg_dst = 1;
                    control->alu_ctrl = 0b0110;
                    control->rt_ch = 1;
                    control->rs_ch = 1;
                    break;
                case 0x24:  // And
                    control->reg_wb = 1;
                    control->reg_dst = 1;
                    control->alu_ctrl = 0b0000;
                    control->rt_ch = 1;
                    control->rs_ch = 1;
                    break;
                case 0x25:  // Or
                    control->reg_wb = 1;
                    control->reg_dst = 1;
                    control->alu_ctrl = 0b0001;
                    control->rt_ch = 1;
                    control->rs_ch = 1;
                    break;
                case 0x27:  // Nor
                    control->reg_wb = 1;
                    control->reg_dst = 1;
                    control->alu_ctrl = 0b1100;
                    control->rt_ch = 1;
                    control->rs_ch = 1;
                    break;
                case 0x00:  // SLL
                    control->reg_wb = 1;
                    control->reg_dst = 1;
                    control->alu_ctrl = 0b1110;
                    control->rt_ch = 1;
                    break;
                case 0x02:  // SRL
                    control->reg_wb = 1;
                    control->reg_dst = 1;
                    control->alu_ctrl = 0b1111;
                    control->rt_ch = 1;
                    break;
                case 0x2A:  // SLT
                    control->reg_wb = 1;
                    control->reg_dst = 1;
                    control->alu_ctrl = 0b0111;
                    control->rt_ch = 1;
                    control->rs_ch = 1;
                    break;
                case 0x2B:  // SLTU
                    control->reg_wb = 1;
                    control->reg_dst = 1;
                    control->alu_ctrl = 0b0111;
                    control->rt_ch = 1;
                    control->rs_ch = 1;
                    break;
                case 0x08:  // JR
                    control->ex_skip = 1;
                    control->reg_dst = 1;
                    control->rs_ch = 1;
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}