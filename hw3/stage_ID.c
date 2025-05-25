#include "structure.h"

void stage_ID(){
    // 해저드 감지 (스톨 필요한지 확인)
    HazardUnit hazard = detect_hazard();
    
    // 스톨이 필요하면 처리하고 리턴
    if (hazard.stall) {
        handle_stall();
        return;  // 이번 사이클은 디코딩하지 않음
    }
    
    // IF/ID latch가 유효한지 확인
    if(!if_id_latch.valid){
        id_ex_latch.valid = false;
        return;
    }

    // IF/ID latch에서 instruction과 pc를 가져옴
    uint32_t instruction = if_id_latch.instruction;
    uint32_t pc = if_id_latch.pc;

    Instruction inst;
    memset(&inst, 0, sizeof(inst));

    inst.opcode = instruction >> 26; // opcode 추출
    inst.pc_plus_4 = pc + 4;

    // 명령어 타입에 따라 디코딩
    if(inst.opcode == 0){
        decode_rtype(instruction, &inst);
    }else if(inst.opcode == 2 || inst.opcode == 3){
        decode_jtype(instruction, &inst);
    }else{
        decode_itype(instruction, &inst);
    }

    // immediate 값 확장
    extend_imm_val(&inst);

    // 레지스터 값 읽기
    inst.rs_value = registers.regs[inst.rs];
    inst.rt_value = registers.regs[inst.rt];

    // 제어 신호 설정
    Control_Signals ctrl;
    setup_control_signals(&inst, &ctrl);

    // 목적지 레지스터 미리 계산 (포워딩 감지용)
    uint32_t dest_reg = 0;
    if (ctrl.regwrite) {
        if (ctrl.regdst) dest_reg = inst.rd;          /* R-type */
        else dest_reg = inst.rt;          /* I-type */
        /* jal / jalr → $ra(31) */
        if (inst.opcode == 3 || inst.inst_type == 5)
            dest_reg = 31;
    }

    // ID/EX latch에 정보를 저장
    id_ex_latch.valid = true;
    id_ex_latch.pc = pc;
    id_ex_latch.instruction = inst;
    id_ex_latch.control_signals = ctrl;
    id_ex_latch.rs_value = inst.rs_value;
    id_ex_latch.rt_value = inst.rt_value;
    id_ex_latch.write_reg = dest_reg;

    // 디버그 출력
    printf("디코딩: PC=0x%08x, opcode=%d, rs=%d, rt=%d, rd=%d\n",
           pc, inst.opcode, inst.rs, inst.rt, inst.rd);
}

void extend_imm_val(Instruction* inst)
{
    switch (inst->inst_type)
    {
    case 2: {
        // andi, ori는 제로 확장
        if (inst->opcode == 0x0C || inst->opcode == 0x0D) {
            inst->immediate &= 0x0000FFFF;
        }
        // 나머지 I-type은 부호 확장
        else {
            if (inst->immediate & 0x8000)
                inst->immediate |= 0xFFFF0000;
        }
        break;
    }

    case 4: {
        // Branch: 16-bit를 32-bit로 부호 확장 후 4배
        int32_t off = (int16_t)inst->immediate;
        inst->immediate = (uint32_t)(off << 2);
        break;
    }

    case 1: {
        // J-type: 26-bit를 4배
        inst->jump_target <<= 2;
        break;
    }

    default:
        break;
    }
}

void decode_rtype(uint32_t instruction, Instruction* inst) {
    inst->rs = (instruction >> 21) & 0x1f;
    inst->rt = (instruction >> 16) & 0x1f;
    inst->rd = (instruction >> 11) & 0x1f;
    inst->shamt = (instruction >> 6) & 0x1f;
    inst->funct = instruction & 0x3f;
    inst->inst_type = 0;
    
    if (inst->funct == 0x08) {
        inst->inst_type = 3; // jr
    }
    else if (inst->funct == 0x09) {
        inst->inst_type = 5; // jalr
    }
}

void decode_itype(uint32_t instruction, Instruction* inst) {
    inst->rs = (instruction >> 21) & 0x1f;
    inst->rt = (instruction >> 16) & 0x1f;
    inst->immediate = instruction & 0xffff;
    inst->inst_type = 2;
    
    if (inst->opcode == 4 || inst->opcode == 5) {
        inst->inst_type = 4; // branch
    }
}

void decode_jtype(uint32_t instruction, Instruction* inst) {
    inst->jump_target = instruction & 0x3ffffff;
    inst->inst_type = 1;
}