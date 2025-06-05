#include "structure.h"

extern uint64_t g_branch_count;
extern uint64_t g_branch_taken;

void stage_ID(){
    // 해저드 감지 (스톨 필요한지 확인)
    HazardUnit hazard = detect_hazard();
    
    // 스톨이 필요하면 처리하고 리턴
    if (hazard.stall) {
        handle_stall();
        return;
    }
    
    // IF/ID latch가 유효한지 확인
    if(!if_id_latch.valid){
        id_ex_latch.valid = false;
        return;
    }

    uint32_t instruction = if_id_latch.instruction;
    uint32_t pc = if_id_latch.pc;

    Instruction inst;
    memset(&inst, 0, sizeof(inst));

    inst.opcode = instruction >> 26;
    inst.pc_plus_4 = pc + 4;

    // 명령어 타입에 따라 디코딩
    if(inst.opcode == 0){
        decode_rtype(instruction, &inst);
    }else if(inst.opcode == 2 || inst.opcode == 3){
        decode_jtype(instruction, &inst);
    }else{
        decode_itype(instruction, &inst);
    }

    extend_imm_val(&inst);

    // 레지스터 값 읽기
    inst.rs_value = registers.regs[inst.rs];
    inst.rt_value = registers.regs[inst.rt];

    // 제어 신호 설정
    Control_Signals ctrl;
    setup_control_signals(&inst, &ctrl);

    // 브랜치 명령어 처리 (간단한 방식 - 항상 not-taken으로 예측)
    if (inst.opcode == 4 || inst.opcode == 5) { // beq, bne
        g_branch_count++;
        
        uint32_t rs_val = inst.rs_value;
        uint32_t rt_val = inst.rt_value;
        bool equal = (rs_val == rt_val);
        bool taken = (inst.opcode == 4) ? equal : !equal; // beq : bne
        
        if (taken) {
            g_branch_taken++;
            
            // 브랜치 타겟 계산
            uint32_t branch_offset = inst.immediate;
            if ((branch_offset >> 15) == 1) { // 음수
                branch_offset = ((branch_offset << 2) | 0xfffc0000);
            } else { // 양수
                branch_offset = ((branch_offset << 2) & 0x3ffc);
            }
            uint32_t target = pc + 4 + branch_offset;
            
            registers.pc = target;
            printf("브랜치 taken: PC=0x%08x\n", registers.pc);
            
            // 파이프라인 플러시 (always not-taken 예측이므로 taken시 플러시)
            handle_branch_flush();
        } else {
            printf("브랜치 not taken\n");
        }
        
        id_ex_latch.valid = false;
        return;
    }
    else if (inst.opcode == 2) { // j
        uint32_t jump_addr = inst.jump_target << 2;
        registers.pc = jump_addr;
        printf("점프: PC=0x%08x\n", registers.pc);
        handle_branch_flush();
        id_ex_latch.valid = false;
        return;
    }
    else if (inst.opcode == 3) { // jal
        uint32_t jump_addr = inst.jump_target << 2;
        registers.regs[31] = pc + 8; // 지연 슬롯 고려
        registers.pc = jump_addr;
        printf("jal: PC=0x%08x, R31=0x%08x\n", registers.pc, registers.regs[31]);
        handle_branch_flush();
        id_ex_latch.valid = false;
        return;
    }
    else if (inst.opcode == 0 && inst.funct == 0x08) { // jr
        uint32_t jump_addr = inst.rs_value;
        registers.pc = jump_addr;
        printf("jr: PC=0x%08x\n", registers.pc);
        handle_branch_flush();
        id_ex_latch.valid = false;
        return;
    }

    // 목적지 레지스터 계산
    uint32_t dest_reg = 0;
    if (ctrl.regwrite) {
        if (ctrl.regdst) dest_reg = inst.rd;
        else dest_reg = inst.rt;
        if (inst.opcode == 3 || inst.inst_type == 5) // jal, jalr
            dest_reg = 31;
    }

    // ID/EX latch에 정보 저장
    id_ex_latch.valid = true;
    id_ex_latch.pc = pc;
    id_ex_latch.instruction = inst;
    id_ex_latch.control_signals = ctrl;
    id_ex_latch.rs_value = inst.rs_value;
    id_ex_latch.rt_value = inst.rt_value;
    id_ex_latch.write_reg = dest_reg;

    printf("디코딩: PC=0x%08x, %s\n", pc, get_instruction_name(inst.opcode, inst.funct));
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