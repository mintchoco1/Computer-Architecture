#include "structure.h"

extern uint64_t g_num_wb_commit;
extern uint64_t g_r_type_count;
extern uint64_t g_i_type_count;
extern uint64_t g_j_type_count;
extern uint64_t g_branch_jr_count;
extern uint64_t g_lw_count;
extern uint64_t g_sw_count;
extern uint64_t g_write_reg_count;

void stage_WB(void)
{
    /* NOP 전파 */
    if (!mem_wb_latch.valid) return;

    const Control_Signals ctrl = mem_wb_latch.control_signals;
    const Instruction inst = mem_wb_latch.instruction;

    /* 기록할 값 선택 */
    uint32_t write_val = ctrl.memtoreg ? mem_wb_latch.rt_value : mem_wb_latch.alu_result;

    /* 레지스터 파일 쓰기 */
    if (ctrl.regwrite && mem_wb_latch.write_reg != 0) {  /* $0 보호 */
        registers.regs[mem_wb_latch.write_reg] = write_val;
        g_write_reg_count++;
        printf("WB: R[%d] ← 0x%08x (PC=0x%08x)\n",
               mem_wb_latch.write_reg, write_val, mem_wb_latch.pc);
    }

    /* 통계 카운터 업데이트 (참고 코드 방식) */
    g_num_wb_commit++;
    
    // 명령어 타입별 통계
    if (inst.opcode == 0) { // R-type
        g_r_type_count++;
    }
    else if (inst.opcode == 2 || inst.opcode == 3 || 
             inst.opcode == 4 || inst.opcode == 5 ||
             (inst.opcode == 0 && inst.funct == 0x08)) { // j, jal, beq, bne, jr
        g_branch_jr_count++;
    }
    else if (inst.opcode == 35) { // lw
        g_lw_count++;
        g_i_type_count++;
    }
    else if (inst.opcode == 43) { // sw
        g_sw_count++;
        g_i_type_count++;
    }
    else { // 기타 I-type
        g_i_type_count++;
    }
}