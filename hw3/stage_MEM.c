#include "structure.h"

void stage_MEM() {
    
    // 래치가 비어있으면 nop 전달해서 MEM 스테이지의 메모리나 레지스터 못 건들이게 함
    if (!ex_mem_latch.valid) {
        mem_wb_latch.valid = false;
        return;
    }

    // EX 단계에서 계산한 alu 결과 복사
    Instruction inst = ex_mem_latch.instruction;
    Control_Signals ctrl = ex_mem_latch.control_signals;
    uint32_t address = ex_mem_latch.alu_result;  // lw/sw 주소
    uint32_t write_data = ex_mem_latch.rt_value; // sw일 때 데이터
    uint32_t mem_read_data = 0;                  // lw 결과

    // 메모리 읽기 (LW) - 참고 코드 스타일로 수정
    if (ctrl.memread) {
        if (address + 3 >= MEMORY_SIZE) {    
            printf("MEM: address 0x%08x out of bounds (LW)\n", address);
            mem_read_data = 0;
        } else {
            // 리틀 엔디안으로 읽기 (참고 코드 방식)
            uint32_t temp = 0;
            for (int j = 3; j >= 0; j--) {
                temp <<= 8;
                temp |= memory[address + j];
            }
            mem_read_data = temp;
            printf("MEM: LW - Mem[0x%08x] = 0x%08x\n", address, mem_read_data);
        }
    }

    // 메모리 쓰기 (SW) - 참고 코드 스타일로 수정
    if (ctrl.memwrite) {
        if (address + 3 >= MEMORY_SIZE) {
            printf("MEM: address 0x%08x out of bounds (SW)\n", address);
        } else {
            // 리틀 엔디안으로 쓰기 (참고 코드 방식)
            for (int i = 0; i < 4; i++) {
                memory[address + i] = (write_data >> (8 * i)) & 0xFF;
            }
            printf("MEM: SW - 0x%08x → Mem[0x%08x]\n", write_data, address);
        }
    }

    // MEM/WB 래치 채우기
    mem_wb_latch.valid           = true;
    mem_wb_latch.pc              = ex_mem_latch.pc;
    mem_wb_latch.instruction     = inst;
    mem_wb_latch.control_signals = ctrl;
    mem_wb_latch.alu_result      = ex_mem_latch.alu_result;   // ALU 결과 or 주소
    mem_wb_latch.rt_value        = mem_read_data;             // 로드된 데이터
    mem_wb_latch.write_reg       = ex_mem_latch.write_reg;
}