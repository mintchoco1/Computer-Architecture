#include "structure.h"

extern uint64_t g_num_wb_commit;

/* ─────────────────────────────────────────
 *  WB Stage : Write-Back
 *  ① MEM/WB 래치 valid 확인
 *  ② write_val 선택
 *     - memtoreg==1 → 메모리에서 읽어온 값(rt_value)
 *     - memtoreg==0 → ALU 결과(alu_result)
 *  ③ regwrite==1 이면 목적지 레지스터에 기록
 * ───────────────────────────────────────── */
void stage_WB(void)
{
    /* ① NOP 전파 */
    if (!mem_wb_latch.valid) return;

    const Control_Signals ctrl = mem_wb_latch.control_signals;

    /* ② 기록할 값 선택 */
    uint32_t write_val = ctrl.memtoreg
                         ? mem_wb_latch.rt_value      /* LW 데이터 */
                         : mem_wb_latch.alu_result;   /* ALU 결과 */

    /* ③ 레지스터 파일 쓰기 */
    if (ctrl.regwrite && mem_wb_latch.write_reg != 0) {  /* $0 보호 */
        registers.regsp[mem_wb_latch.write_reg] = write_val;
#ifdef DEBUG_WB
        printf("WB  : R[%d] ← 0x%08x (PC=0x%08x)\n",
               mem_wb_latch.write_reg, write_val, mem_wb_latch.pc);
#endif
    }

    /* ④ 통계 카운터(옵션) */
    g_num_wb_commit++;
}
