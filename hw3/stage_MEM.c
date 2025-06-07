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

    // 제어 신호 복사
    mem_wb_latch.control_signals = ctrl;

    // LUI 명령어 특별 처리
    if (ctrl.get_imm == 3) {
        mem_wb_latch.valid = true;
        mem_wb_latch.pc = ex_mem_latch.pc;
        mem_wb_latch.instruction = inst;
        mem_wb_latch.alu_result = ex_mem_latch.alu_result;
        mem_wb_latch.rt_value = 0;
        mem_wb_latch.write_reg = ex_mem_latch.write_reg;
        return;
    }

    if (ctrl.ex_skip != 0) {
        mem_wb_latch.valid = false;
        return;
    }

    // 기본 값들 설정
    mem_wb_latch.alu_result = ex_mem_latch.alu_result;
    mem_wb_latch.write_reg = ex_mem_latch.write_reg;

    // lw sw 외에는 return 
    if ((ctrl.mem_read == 0) && (ctrl.mem_write == 0)) {
        mem_wb_latch.valid = true;
        mem_wb_latch.pc = ex_mem_latch.pc;
        mem_wb_latch.instruction = inst;
        return;
    }

    // 메모리 읽기 (LW)
    if (ctrl.mem_read) {
        lw_count++;
        if (address + 4 > MEMORY_SIZE) {
            printf("MEM: address 0x%08x out of bounds (LW)\n", address);
            mem_read_data = 0;
        } else {
            uint32_t temp = 0;
            for (int j = 3; j >= 0; j--) {
                temp <<= 8;
                temp |= memory[address + j];
            }
            mem_read_data = temp;
        }
    }

    // 메모리 쓰기 (SW) 
    if (ctrl.mem_write) {
        sw_count++;
        if (address + 4 > MEMORY_SIZE) {
            printf("MEM: address 0x%08x out of bounds (SW)\n", address);
        } else {
            for (int i = 0; i < 4; i++) {
                memory[address + i] = (write_data >> (8 * i)) & 0xFF;
            }
        }
    }

    // 다음 단계로 정보 전달
    mem_wb_latch.valid = true;
    mem_wb_latch.pc = ex_mem_latch.pc;
    mem_wb_latch.instruction = inst;
    mem_wb_latch.alu_result = ex_mem_latch.alu_result;
    mem_wb_latch.rt_value = mem_read_data;  // LW의 경우 메모리에서 읽은 값
    mem_wb_latch.write_reg = ex_mem_latch.write_reg;
}