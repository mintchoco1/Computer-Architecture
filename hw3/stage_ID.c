// stage_ID.c - 참고 코드와 일치하도록 수정
#include "structure.h"

// 외부 변수들을 참고 코드와 동일하게 변경
extern uint64_t r_count;
extern uint64_t i_count;
extern uint64_t branch_jr_count;

void stage_ID() {
    if (!if_id_latch.valid) {
        id_ex_latch.valid = false;
        return;
    }

    uint32_t instruction = if_id_latch.instruction;
    uint32_t pc = if_id_latch.pc;
    uint32_t opcode = if_id_latch.opcode;
    uint32_t funct = if_id_latch.funct;

    // 참고 코드처럼 초기화
    memset(&id_ex_latch, 0, sizeof(id_ex_latch));

    // Control signals 설정
    Control_Signals ctrl;
    initialize_control(&ctrl);
    
    // Instruction 구조체 생성
    Instruction inst;
    memset(&inst, 0, sizeof(inst));
    inst.opcode = opcode;
    inst.rs = if_id_latch.reg_src;
    inst.rt = if_id_latch.reg_tar;
    inst.funct = funct;
    
    // R-type
    if (opcode == 0) {
        inst.rd = (instruction >> 11) & 0x1f;
        inst.shamt = (instruction >> 6) & 0x1f;
    }
    
    // J-type
    if (opcode == 2 || opcode == 3) {
        inst.jump_target = instruction & 0x3ffffff;
    }
    
    // setup_control_signals 호출
    setup_control_signals(&inst, &ctrl);
    
    // 참고 코드와 동일한 방식으로 카운팅
    if (ctrl.reg_dst == 1) {
        r_count++;
    }
    if (ctrl.get_imm != 0) {
        i_count++;
    }
    if (ctrl.ex_skip == 1) {
        branch_jr_count++;
    }
    
    // write_reg 설정 (참고 코드 순서대로)
    if (ctrl.reg_dst == 1) {                                
        id_ex_latch.write_reg = (instruction >> 11) & 0x0000001f;        // rd
    }
    
    // immediate 처리 (참고 코드와 동일)
    if (ctrl.get_imm != 0) {
        inst.immediate = instruction & 0xffff;
        id_ex_latch.write_reg = inst.rt;
        
        if (ctrl.get_imm == 1) {
            if ((inst.immediate >> 15) == 0) {
                inst.immediate = (inst.immediate & 0x0000ffff);
            } else {
                inst.immediate = (inst.immediate | 0xffff0000);
            }
        } else if (ctrl.get_imm == 2) {
            inst.immediate = (inst.immediate & 0x0000ffff);
        } else if (ctrl.get_imm == 3) {
            inst.immediate = inst.immediate << 16;
        }
    }
    
    // 브랜치/점프 처리 (참고 코드와 완전히 동일)
    if (ctrl.ex_skip == 1) {
        // 브랜치
        if (opcode == 0x4 || opcode == 0x5) {        // beq, bne
            uint32_t oper1 = (if_id_latch.forward_a >= 1) ? if_id_latch.forward_a_val : registers.regs[inst.rs];
            uint32_t oper2 = (if_id_latch.forward_b >= 1) ? if_id_latch.forward_b_val : registers.regs[inst.rt];

            int beq_bne = (opcode == 0x4) ? 1 : 0;   // beq = 1, bne = 0
            int check = (oper1 == oper2);      // 동등 여부 파악
            int taken = (check == beq_bne);                                      
            
            if (taken) {
                // taken
                uint32_t sign_imm = instruction & 0xffff;                     // imm
                uint32_t branchaddr = sign_imm;

                if ((sign_imm >> 15) == 1) { // branch-addr
                    branchaddr = ((sign_imm << 2) | 0xfffc0000);
                }
                else if ((sign_imm >> 15) == 0) {
                    branchaddr = ((sign_imm << 2) & 0x3ffc);
                }
                registers.pc = registers.pc + branchaddr;        // pc = pc + 4 + branchaddr
                return;
            }
            else {
                // not taken
                return;
            }
        }   
        // J
        else if (opcode == 0x2) {   // j
            uint32_t jaddr = inst.jump_target << 2;
            registers.pc = jaddr;
            return;
        }   
        // JAL
        else if (opcode == 0x3) {   // jal
            uint32_t jaddr = inst.jump_target << 2;
            registers.regs[31] = registers.pc + 4; // pc+8 (참고 코드와 동일)
            registers.pc = jaddr;
            return;
        }   
        // JR
        else {                      // jr
            uint32_t oper1 = (if_id_latch.forward_a >= 1) ? if_id_latch.forward_a_val : registers.regs[inst.rs];
            registers.pc = oper1;
            return;
        }
    }
    
    // 일반 명령어 처리 - 레지스터 값 읽기 (참고 코드와 동일)
    inst.rs_value = registers.regs[inst.rs];      // rs 값 read data1
    inst.rt_value = registers.regs[inst.rt];      // rt 값 read data2
    
    // ID/EX 래치 업데이트
    id_ex_latch.valid = true;
    id_ex_latch.pc = pc;
    id_ex_latch.instruction = inst;
    id_ex_latch.control_signals = ctrl;
    id_ex_latch.rs_value = inst.rs_value;
    id_ex_latch.rt_value = inst.rt_value;
    id_ex_latch.sign_imm = inst.immediate;
    id_ex_latch.shamt = inst.shamt;
    id_ex_latch.forward_a = 0;
    id_ex_latch.forward_b = 0;
    id_ex_latch.forward_a_val = 0;
    id_ex_latch.forward_b_val = 0;
}

void decode_rtype(uint32_t instruction, Instruction* inst) {
    inst->rs = (instruction >> 21) & 0x1f;
    inst->rt = (instruction >> 16) & 0x1f;
    inst->rd = (instruction >> 11) & 0x1f;
    inst->shamt = (instruction >> 6) & 0x1f;
    inst->funct = instruction & 0x3f;
    inst->inst_type = 0;
}

void decode_itype(uint32_t instruction, Instruction* inst) {
    inst->rs = (instruction >> 21) & 0x1f;
    inst->rt = (instruction >> 16) & 0x1f;
    inst->immediate = instruction & 0xffff;
    inst->inst_type = 2;
}

void decode_jtype(uint32_t instruction, Instruction* inst) {
    inst->jump_target = instruction & 0x3ffffff;
    inst->inst_type = 1;
}