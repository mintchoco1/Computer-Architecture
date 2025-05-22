#include "structure.h"

void stage_MEM() {
    
    //래치가 비어있으면 nop 전달해서 MEM 스테이지의 메모리나 레지스터 못 건들이게 함
    if (!ex_mem_latch.valid) {
        mem_wb_latch.valid = false;
        return;
    }

    //EX 단계에서 계산한 alu 결과 복사
    Instruction inst = ex_mem_latch.instruction;
    Control_Signals ctrl = ex_mem_latch.control_signals;
    uint32_t address = ex_mem_latch.alu_result;//lw/rw 주소
    uint32_t write_data = ex_mem_latch.rt_value;//sw일 때 데이터
    uint32_t mem_read_data = 0;//lw 결과

    //메모리에서 데이터 읽기(lw)
    if (ctrl.memread) {
        if (address + 3 >= MEMORY_SIZE) {
            printf("MEM stage: address 0x%08x out of bounds\n", address);
            mem_read_data = 0;
        }
    }

    /* ③ 메모리 읽기 (LW) */
    if (ctrl.memread) {
        if (address + 3 >= MEMORY_SIZE) {    
            printf("MEM STAGE: address 0x%08x out of bounds (LW)\n", address);
            mem_read_data = 0;
        } else {
            mem_read_data = (memory[address] << 24);
            mem_read_data |= (memory[address + 1] << 16);
            mem_read_data |= (memory[address + 2] << 8);
            mem_read_data |= (memory[address + 3]);
        }
    }

    /* ④ 메모리 쓰기 (SW) */
    if (ctrl.memwrite) {
        if (address + 3 >= MEMORY_SIZE) {
            printf("MEM STAGE: address 0x%08x out of bounds (SW)\n", address);
        } else {
            memory[address] = (write_data >> 24) & 0xFF;
            memory[address + 1] = (write_data >> 16) & 0xFF;
            memory[address + 2] = (write_data >> 8) & 0xFF;
            memory[address + 3] = write_data & 0xFF;
        }
    }

    /* ⑤ MEM/WB 래치 채우기 */
    mem_wb_latch.valid           = true;
    mem_wb_latch.pc              = ex_mem_latch.pc;
    mem_wb_latch.instruction     = inst;
    mem_wb_latch.control_signals = ctrl;
    mem_wb_latch.alu_result      = ex_mem_latch.alu_result;   // ALU 결과 or 주소
    /*  ⬇ 이 필드는 pipeline 설계마다 다릅니다.
     *     여기서는 load 데이터(LW) 전달 용도로 rt_value를 재사용! */
    mem_wb_latch.rt_value        = mem_read_data;
    mem_wb_latch.write_reg       = ex_mem_latch.write_reg;
}