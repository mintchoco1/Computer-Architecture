#include "structure.h"

/* ──────────────────────────────
 *  간단 ALU 함수
 *  aluop (control_signals.aluop)
 *   0 = ADD  (기본, LW‧SW 등)
 *   1 = SUB  (BEQ·BNE 비교용)
 *   2 = R-type (funct 필드로 추가 구분)
 *   3 = SLT
 *   4 = AND
 *   5 = OR
 *   6 = LUI
 *  반환값: 계산 결과
 * ──────────────────────────────*/
uint32_t alu_operate(uint32_t operand1, uint32_t operand2, int aluop, Instruction *inst) {
    switch (aluop) {
        case 0:  return operand1 + operand2;                          /* add */
        case 1:  return operand1 - operand2;                          /* sub */
        case 3:  return ((int32_t)operand1 < (int32_t)operand2);      /* slt */
        case 4:  return operand1 & operand2;                          /* and */
        case 5:  return operand1 | operand2;                          /* or  */
        case 6:  return operand2 << 16;                           /* lui */
        case 2: {                                             /* R-type */
            switch (inst->funct) {
                case 0x20: /* add  */ return operand1 + operand2;
                case 0x21: /* addu */ return operand1 + operand2;
                case 0x22: /* sub  */ return operand1 - operand2;
                case 0x23: /* subu */ return operand1 - operand2;
                case 0x24: /* and  */ return operand1 & operand2;
                case 0x25: /* or   */ return operand1 | operand2;
                case 0x27: /* nor  */ return ~(operand1 | operand2);
                case 0x2a: /* slt  */ return ((int32_t)operand1 < (int32_t)operand2);
                case 0x2b: /* sltu */ return (operand1 < operand2);
                case 0x00: /* sll  */ return operand2 << inst->shamt;
                case 0x02: /* srl  */ return operand2 >> inst->shamt;
                default:            return 0;                 /* 미지원 */
            }
        }
        default: return 0;
    }
}

void stage_EX(void) {

    //ID/EX latch에 명령어 없으면 EX/MEMlatch 무효화
    if (!id_ex_latch.valid) {
        ex_mem_latch.valid = false;
        return;
    }

    Instruction inst = id_ex_latch.instruction;
    Control_Signals ctrl = id_ex_latch.control_signals;

    uint32_t operand1 = id_ex_latch.rs_value;
    uint32_t operand2 = ctrl.alusrc ? inst.immediate : id_ex_latch.rt_value;

    //제어 신호로 주소를 계산할지 아니면 값만 계산할지 정함
    uint32_t alu_result = alu_operate(operand1, operand2, ctrl.aluop, &inst);

    /* ④ 브랜치 판정 (BEQ/BNE) */
    bool take_branch = false;
    if (ctrl.branch) {
        bool zero = (alu_result == 0);          /* BEQ용 zero flag */
        if ((inst.opcode == 4 && zero) || (inst.opcode == 5 && !zero))     /* BNE */
            take_branch = true;
    }

    /* ⑤ 목적지 레지스터 선택 */
    uint32_t dest_reg = 0;
    if (ctrl.regwrite) {
        if (ctrl.regdst) dest_reg = inst.rd;          /* R-type */
        else dest_reg = inst.rt;          /* I-type */
        /* jal / jalr → $ra(31) */
        if (inst.opcode == 3 || inst.inst_type == 5)
            dest_reg = 31;
    }

    /* ⑥ 다음 스테이지(EX/MEM) 래치 채우기 */
    ex_mem_latch.valid = true;
    ex_mem_latch.pc = id_ex_latch.pc;
    ex_mem_latch.instruction = inst;
    ex_mem_latch.control_signals = ctrl;
    ex_mem_latch.alu_result = alu_result;
    ex_mem_latch.rt_value = id_ex_latch.rt_value; /* SW용 */
    ex_mem_latch.write_reg = dest_reg;

    /* ⑦ 분기/점프 반영 (간단 버전 – flush 로직은 run_loop 쪽에서) */
    if (ctrl.branch && take_branch) {
        registers.pc = inst.pc_plus_4 + inst.immediate;
        /* flush 단계는 MEM or run_pipeline 에서 처리 */
    }
    else if (ctrl.jump) {
        /* J / JAL / JR / JALR */
        if (inst.inst_type == 1)                 /* J, JAL : 26-bit target */
            registers.pc = (id_ex_latch.pc & 0xF0000000) | inst.jump_target;
        else if (inst.inst_type == 3)            /* JR      */
            registers.pc = operand1;
        else if (inst.inst_type == 5)            /* JALR    */
            registers.pc = operand1;
    }
}