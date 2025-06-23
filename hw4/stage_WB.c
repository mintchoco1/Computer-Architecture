#include "structure.h"

extern uint64_t write_reg_count;

void stage_WB(void) {
    if (!mem_wb_latch.valid) {
        return;
    }

    const Control_Signals ctrl = mem_wb_latch.control_signals;

    if (ctrl.get_imm == 3) {
        if (mem_wb_latch.write_reg != 0) {
            registers.regs[mem_wb_latch.write_reg] = mem_wb_latch.alu_result;
            printf("[WB] LUI: R%d = 0x%x\n", 
                   mem_wb_latch.write_reg, mem_wb_latch.alu_result);
        } else {
            printf("[WB] LUI: write to R0 (ignored)\n");
        }
        return;
    }

    if (ctrl.reg_wb == 0) {       
        printf("[WB] PC=0x%08x, %s: no write back\n", 
               mem_wb_latch.pc,
               get_instruction_name(mem_wb_latch.instruction.opcode, mem_wb_latch.instruction.funct));
        return;
    }

    write_reg_count++;

    if (ctrl.mem_read == 1) {       // LW의 경우
        registers.regs[mem_wb_latch.write_reg] = mem_wb_latch.rt_value;
        printf("[WB] LW: R%d = 0x%x (from memory)\n", 
               mem_wb_latch.write_reg, mem_wb_latch.rt_value);
    } else {                        // R type alu
        registers.regs[mem_wb_latch.write_reg] = mem_wb_latch.alu_result;
        printf("[WB] %s: R%d = 0x%x\n", 
               get_instruction_name(mem_wb_latch.instruction.opcode, mem_wb_latch.instruction.funct),
               mem_wb_latch.write_reg, mem_wb_latch.alu_result);
    }
}