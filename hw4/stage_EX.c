#include "structure.h"

uint32_t alu_operate(uint32_t operand1, uint32_t operand2, int alu_ctrl, Instruction *inst) {
    uint32_t temp = 0;
    
    if (alu_ctrl == 0b0000) {             // and
        temp = operand1 & operand2;
    }
    else if (alu_ctrl == 0b0001) {        // or
        temp = operand1 | operand2;
    }
    else if (alu_ctrl == 0b0010) {        // add
        temp = operand1 + operand2;
    }
    else if (alu_ctrl == 0b0110) {        // sub
        temp = operand1 - operand2;
    }
    else if (alu_ctrl == 0b1100) {        // nor
        temp = ~(operand1 | operand2);
    }
    else if (alu_ctrl == 0b0111) {        // slt 
        temp = (operand2 > operand1) ? 1 : 0; 
    }
    else if (alu_ctrl == 0b1110) {        // sll
        temp = operand1 << operand2;
    }
    else if (alu_ctrl == 0b1111) {        // srl
        temp = operand1 >> operand2;
    }

    return temp;
}

void stage_EX(void) {
    if (!id_ex_latch.valid) {
        ex_mem_latch.valid = false;
        return;
    }

    memset(&ex_mem_latch, 0, sizeof(ex_mem_latch));

    Instruction inst = id_ex_latch.instruction;
    Control_Signals ctrl = id_ex_latch.control_signals;

    ex_mem_latch.control_signals = ctrl;

    if (ctrl.get_imm == 3) {
        ex_mem_latch.valid = true;
        ex_mem_latch.pc = id_ex_latch.pc;
        ex_mem_latch.instruction = inst;
        ex_mem_latch.alu_result = id_ex_latch.sign_imm;
        ex_mem_latch.rt_value = 0;
        ex_mem_latch.write_reg = id_ex_latch.write_reg;
        
        printf("[EX] PC=0x%08x, lui: immediate = 0x%08x\n", 
               id_ex_latch.pc, id_ex_latch.sign_imm);
        return;
    }

    if (ctrl.ex_skip != 0) {
        ex_mem_latch.valid = false;
        return;
    }

    if (ctrl.reg_wb == 1) {         
        ex_mem_latch.write_reg = id_ex_latch.write_reg;
    }

    uint32_t operand1 = (id_ex_latch.forward_a >= 1) ? id_ex_latch.forward_a_val : id_ex_latch.rs_value;

    uint32_t alu_result = 0;

    if (ctrl.reg_dst == 1) {
        uint32_t operand2 = (id_ex_latch.forward_b >= 1) ? id_ex_latch.forward_b_val : id_ex_latch.rt_value;

        if (ctrl.alu_ctrl >= 0b1110) {
            alu_result = alu_operate(operand2, id_ex_latch.shamt, ctrl.alu_ctrl, &inst);
        } else {
            alu_result = alu_operate(operand1, operand2, ctrl.alu_ctrl, &inst);
        }
    }
    // SW 명령어 
    else if (ctrl.mem_write == 1) {
        alu_result = alu_operate(operand1, id_ex_latch.sign_imm, ctrl.alu_ctrl, &inst);
        ex_mem_latch.rt_value = (id_ex_latch.forward_b >= 1) ? id_ex_latch.forward_b_val : id_ex_latch.rt_value;
    }
    // I-type 명령어
    else {
        alu_result = alu_operate(operand1, id_ex_latch.sign_imm, ctrl.alu_ctrl, &inst);
    }

    ex_mem_latch.valid = true;
    ex_mem_latch.pc = id_ex_latch.pc;
    ex_mem_latch.instruction = inst;
    ex_mem_latch.alu_result = alu_result;

    printf("[EX] PC=0x%08x, %s: ALU result = 0x%08x\n", 
           id_ex_latch.pc, 
           get_instruction_name(id_ex_latch.instruction.opcode, id_ex_latch.instruction.funct),
           alu_result);
}