#include "structure.h"

extern uint64_t write_reg_count;

void stage_WB(void) {
    if (!mem_wb_latch.valid) {
        return;
    }

    const Control_Signals ctrl = mem_wb_latch.control_signals;

    // LUI 명령어 특별 처리 
    if (ctrl.get_imm == 3) {
        if (mem_wb_latch.write_reg != 0) {
            registers.regs[mem_wb_latch.write_reg] = mem_wb_latch.alu_result;
        }
        return;
    }

    if (ctrl.reg_wb == 0) {        // reg_write 값 0이면 지우기 
        return;
    }

    write_reg_count++;

    if (ctrl.mem_read == 1) {       // LW의 경우
        registers.regs[mem_wb_latch.write_reg] = mem_wb_latch.rt_value;
    } else {                        // R type alu
        registers.regs[mem_wb_latch.write_reg] = mem_wb_latch.alu_result;
    }
}