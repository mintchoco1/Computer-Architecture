#include "structure.h"

void stage_ID(){
    printf("ID stage");
    
    // IF/ID latch가 유효하진 확인
    if(!id_ex_latch.valid){
        printf("No valid instruction in ID stage\n");
        id_ex_latch.valid = false;
        return;
    }

    // IF_IF latch에서 instruction과 pc를 가져옴
    uint32_t instruction = if_id_latch.instruction;
    uint32_t pc = if_id_latch.pc;

    Instruction inst;
    memset(&inst, 0, sizeof(inst));

    inst.opcode = instruction >> 26;//opcode 추출

    extend_imm_val(&inst);

    Control_Signals ctrl;
    setup_control_signals(&inst, &ctrl);

    if(inst.opcode == 0){
        decode_rtype(instruction, &inst);
    }else if(inst.opcode == 2 || inst.opcode == 3){
        decode_jtype(instruction, &inst);
    }else{
        decode_itype(instruction, &inst);
    }

    // id/ex latch에 정보를 저장
    id_ex_latch.valid = true;
    id_ex_latch.pc = pc;
    id_ex_latch.instruction = inst;      // 통째 복사
    id_ex_latch.control_signals = ctrl;
    id_ex_latch.rs_value = inst.rs_value;
    id_ex_latch.rt_value = inst.rt_value;
}

void extend_imm_val(Instruction* inst)
{
    switch (inst->inst_type)            // 명령어 대분류(R/I/J/Branch…)로 분기
    {
    /* ──────────────────────────────────
       ① 일반 I-type  (inst_type == 2)
       ex) addi, slti, lw, sw, andi, ori …
       ────────────────────────────────── */
    case 2: {
        /* andi(0x0C), ori(0x0D)는 16-bit 즉시값을 **제로-확장**해야 한다.      */
        if (inst->opcode == 0x0C ||      // 0x0C = andi
            inst->opcode == 0x0D) {      // 0x0D = ori
            inst->immediate &= 0x0000FFFF;   // 상위 16비트를 0으로
        }
        /* 나머지 I-type(opcode 8·9·A·B·23·2B…)은 **부호-확장**이 필요 */
        else {
            /* sign bit(비트 15)가 1이면 상위 16비트를 1로 채움 */
            if (inst->immediate & 0x8000)
                inst->immediate |= 0xFFFF0000;
        }
        break;
    }

    /* ──────────────────────────────────
       ② Branch (inst_type == 4) : beq, bne
       ────────────────────────────────── */
    case 4: {
        /* 16-bit branch offset을 32-bit 부호-확장.
           한 줄 캐스팅: (int16_t) → 부호 유지하며 32-bit로 승격          */
        int32_t off = (int16_t)inst->immediate;

        /* MIPS 분기 오프셋은 ‘워드 주소’ → 실제 바이트 주소 = ×4(<<2) */
        inst->immediate = (uint32_t)(off << 2);
        break;
    }

    /* ──────────────────────────────────
       ③ J-type (inst_type == 1) : j, jal
       ────────────────────────────────── */
    case 1: {
        /* 26-bit jump target 역시 워드 단위 => ×4 */
        inst->jump_target <<= 2;
        break;
    }

    /* ──────────────────────────────────
       ④ R-type / JR / JALR 등 : 변환 없음
       ────────────────────────────────── */
    default:
        break;
    }
}


void decode_rtype(uint32_t instruction, Instruction* inst) {
    inst->rs = (instruction >> 21) & 0x1f;     // rs [25-21]
    inst->rt = (instruction >> 16) & 0x1f;     // rt [20-16]
    inst->rd = (instruction >> 11) & 0x1f;     // rd [15-11]
    inst->shamt = (instruction >> 6) & 0x1f;   // shamt [10-6]
    inst->funct = instruction & 0x3f;          // funct [5-0]
    inst->inst_type = 0;                       // R-type
    
    if (inst->funct == 0x08) {
        inst->inst_type = 3;                   // jr
    }
    else if (inst->funct == 0x09) {
        inst->inst_type = 5;                   // jalr
    }
}

void decode_itype(uint32_t instruction, Instruction* inst) {
    inst->rs = (instruction >> 21) & 0x1f;     // rs [25-21]
    inst->rt = (instruction >> 16) & 0x1f;     // rt [20-16]
    inst->immediate = instruction & 0xffff;    // immediate [15-0]
    inst->inst_type = 2;                       // I-type
    
    if (inst->opcode == 4 || inst->opcode == 5) {
        inst->inst_type = 4;                   // branch
    }
}

void decode_jtype(uint32_t instruction, Instruction* inst) {
    inst->jump_target = instruction & 0x3ffffff; // target [25-0]
    inst->inst_type = 1;                         // J-type
}

void display_rtype(Instruction* inst) {
    printf("R, Inst: ");
    
    // funct 코드에 따라 명령어 출력
    switch (inst->funct) {
        case 0x08: printf("jr r%d", inst->rs); break;
        case 0x09: printf("jalr r%d r%d", inst->rd, inst->rs); break;
        case 0x20: printf("add r%d r%d r%d", inst->rd, inst->rs, inst->rt); break;
        case 0x22: printf("sub r%d r%d r%d", inst->rd, inst->rs, inst->rt); break;
        case 0x24: printf("and r%d r%d r%d", inst->rd, inst->rs, inst->rt); break;
        case 0x25: printf("or r%d r%d r%d", inst->rd, inst->rs, inst->rt); break;
        case 0x27: printf("nor r%d r%d r%d", inst->rd, inst->rs, inst->rt); break;
        case 0x00: printf("sll r%d r%d %d", inst->rd, inst->rt, inst->shamt); break;
        case 0x02: printf("srl r%d r%d %d", inst->rd, inst->rt, inst->shamt); break;
        case 0x2a: printf("slt r%d r%d r%d", inst->rd, inst->rs, inst->rt); break;
        case 0x2b: printf("sltu r%d r%d r%d", inst->rd, inst->rs, inst->rt); break;
        case 0x23: printf("subu r%d r%d r%d", inst->rd, inst->rs, inst->rt); break;
        case 0x21: printf("addu r%d r%d r%d", inst->rd, inst->rs, inst->rt); break;
        case 0x18: printf("mult r%d r%d", inst->rs, inst->rt); break;
        case 0x12: printf("mflo r%d", inst->rd); break;
        default: printf("unknown R-type (funct: 0x%x)", inst->funct); break;
    }
}

void display_itype(Instruction* inst) {
    printf("I, Inst: ");
    
    switch (inst->opcode) {
        case 4: printf("beq r%d r%d %x", inst->rs, inst->rt, inst->immediate); break;
        case 5: printf("bne r%d r%d %x", inst->rs, inst->rt, inst->immediate); break;
        case 8: printf("addi r%d r%d %d", inst->rt, inst->rs, inst->immediate); break;
        case 9: printf("addiu r%d r%d %d", inst->rt, inst->rs, inst->immediate); break;
        case 10: printf("slti r%d r%d %d", inst->rt, inst->rs, inst->immediate); break;
        case 11: printf("sltiu r%d r%d %d", inst->rt, inst->rs, inst->immediate); break;
        case 12: printf("andi r%d r%d %d", inst->rt, inst->rs, inst->immediate); break;
        case 13: printf("ori r%d r%d %d", inst->rt, inst->rs, inst->immediate); break;
        case 35: printf("lw r%d %d(r%d)", inst->rt, inst->immediate, inst->rs); break;
        case 43: printf("sw r%d %d(r%d)", inst->rt, inst->immediate, inst->rs); break;
        case 15: printf("lui r%d %d", inst->rt, inst->immediate); break;
        default: printf("unknown I-type (opcode: 0x%x)", inst->opcode); break;
    }
}

void display_jtype(Instruction* inst) {
    printf("J, Inst: ");
    
    if (inst->opcode == 2) {
        printf("j 0x%x", inst->jump_target);
    } else if (inst->opcode == 3) {
        printf("jal 0x%x", inst->jump_target);
    } else {
        printf("unknown J-type (opcode: 0x%x)", inst->opcode);
    }
}