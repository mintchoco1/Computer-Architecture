#include "structure.h"

uint32_t alu_operate(uint32_t operand1, uint32_t operand2, int aluop, Instruction *inst) {
    switch (aluop) {
        case 0:  return operand1 + operand2;                          /* add */
        case 1:  return operand1 - operand2;                          /* sub */
        case 3:  return ((int32_t)operand1 < (int32_t)operand2);      /* slt */
        case 4:  return operand1 & operand2;                          /* and */
        case 5:  return operand1 | operand2;                          /* or  */
        case 6:  return operand2 << 16;                               /* lui */
        case 2: {                                                     /* R-type */
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
    // ID/EX latch에 명령어 없으면 EX/MEM latch 무효화
    if (!id_ex_latch.valid) {
        ex_mem_latch.valid = false;
        return;
    }

    Instruction inst = id_ex_latch.instruction;
    Control_Signals ctrl = id_ex_latch.control_signals;

    // 포워딩 감지
    ForwardingUnit forwarding = detect_forwarding();
    
    // 포워딩된 값들 가져오기
    uint32_t operand1 = get_forwarded_value(forwarding.forward_a, id_ex_latch.rs_value);
    uint32_t rt_value = get_forwarded_value(forwarding.forward_b, id_ex_latch.rt_value);
    
    // ALU 두 번째 피연산자 선택 (immediate vs rt)
    uint32_t operand2 = ctrl.alusrc ? inst.immediate : rt_value;

    // 포워딩 디버그 출력
    if (forwarding.forward_a != 0) {
        printf("포워딩 A: 타입 %d, 0x%08x → 0x%08x\n", 
               forwarding.forward_a, id_ex_latch.rs_value, operand1);
    }
    if (forwarding.forward_b != 0) {
        printf("포워딩 B: 타입 %d, 0x%08x → 0x%08x\n", 
               forwarding.forward_b, id_ex_latch.rt_value, rt_value);
    }

    // ALU 연산 수행
    uint32_t alu_result = alu_operate(operand1, operand2, ctrl.aluop, &inst);

    // 목적지 레지스터 선택
    uint32_t dest_reg = 0;
    if (ctrl.regwrite) {
        if (ctrl.regdst) dest_reg = inst.rd;          /* R-type */
        else dest_reg = inst.rt;          /* I-type */
        /* jal / jalr → $ra(31) */
        if (inst.opcode == 3 || inst.inst_type == 5)
            dest_reg = 31;
    }

    // 다음 스테이지(EX/MEM) 래치 채우기
    ex_mem_latch.valid = true;
    ex_mem_latch.pc = id_ex_latch.pc;
    ex_mem_latch.instruction = inst;
    ex_mem_latch.control_signals = ctrl;
    ex_mem_latch.alu_result = alu_result;
    ex_mem_latch.rt_value = rt_value; /* SW용, 포워딩된 값 사용 */
    ex_mem_latch.write_reg = dest_reg;

    printf("실행: PC=0x%08x, ALU결과=0x%08x\n", 
           id_ex_latch.pc, alu_result);
}