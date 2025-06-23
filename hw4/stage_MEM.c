#include "structure.h"

extern uint64_t lw_count;
extern uint64_t sw_count;

void stage_MEM() {
    if (!ex_mem_latch.valid) {
        mem_wb_latch.valid = false;
        return;
    }

    memset(&mem_wb_latch, 0, sizeof(mem_wb_latch));
    
    Instruction inst = ex_mem_latch.instruction;
    Control_Signals ctrl = ex_mem_latch.control_signals;
    uint32_t address = ex_mem_latch.alu_result;
    uint32_t write_data = ex_mem_latch.rt_value;
    uint32_t mem_read_data = 0;

    mem_wb_latch.control_signals = ctrl;

    if (ctrl.get_imm == 3) {
        mem_wb_latch.valid = true;
        mem_wb_latch.pc = ex_mem_latch.pc;
        mem_wb_latch.instruction = inst;
        mem_wb_latch.alu_result = ex_mem_latch.alu_result;
        mem_wb_latch.rt_value = 0;
        mem_wb_latch.write_reg = ex_mem_latch.write_reg;
        
        printf("[MEM] PC=0x%08x, lui: pass through\n", ex_mem_latch.pc);
        return;
    }

    if (ctrl.ex_skip != 0) {
        mem_wb_latch.valid = false;
        return;
    }

    mem_wb_latch.alu_result = ex_mem_latch.alu_result;
    mem_wb_latch.write_reg = ex_mem_latch.write_reg;

    // 메모리 읽기 
    if (ctrl.mem_read) {
        lw_count++;
        if (address + 4 > MEMORY_SIZE) {
            printf("[MEM] LW: address 0x%08x out of bounds\n", address);
            mem_read_data = 0;
        } else {
            uint32_t temp = 0;
            for (int j = 3; j >= 0; j--) {
                temp <<= 8;
                temp |= memory[address + j];
            }
            mem_read_data = temp;
            printf("[MEM] LW: Mem[0x%x] = 0x%x -> R%d\n", 
                   address, mem_read_data, ex_mem_latch.write_reg);
        }
    }

    if (ctrl.mem_write) {
        sw_count++;
        if (address + 4 > MEMORY_SIZE) {
            printf("[MEM] SW: address 0x%08x out of bounds\n", address);
        } else {
            for (int i = 0; i < 4; i++) {
                memory[address + i] = (write_data >> (8 * i)) & 0xFF;
            }
            printf("[MEM] SW: R%d(0x%x) -> Mem[0x%x]\n", 
                   ex_mem_latch.instruction.rt, write_data, address);
        }
    }

    // lw sw 외의 일반 명령어는 pass through 표시
    if ((ctrl.mem_read == 0) && (ctrl.mem_write == 0)) {
        printf("[MEM] PC=0x%08x, %s: pass through\n", 
               ex_mem_latch.pc,
               get_instruction_name(ex_mem_latch.instruction.opcode, ex_mem_latch.instruction.funct));
    }

    mem_wb_latch.valid = true;
    mem_wb_latch.pc = ex_mem_latch.pc;
    mem_wb_latch.instruction = inst;
    mem_wb_latch.alu_result = ex_mem_latch.alu_result;
    mem_wb_latch.rt_value = mem_read_data;  
    mem_wb_latch.write_reg = ex_mem_latch.write_reg;
}