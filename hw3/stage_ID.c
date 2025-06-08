#include "structure.h"

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

    // 명령어 정보 출력 추가
    printf("[ID] ");
    print_instruction_details(pc, instruction);
    printf("\n");

    memset(&id_ex_latch, 0, sizeof(id_ex_latch));

    Control_Signals ctrl;
    initialize_control(&ctrl);
    
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
    
    setup_control_signals(&inst, &ctrl);
    
    if (ctrl.reg_dst == 1) {
        r_count++;
    }
    if (ctrl.get_imm != 0) {
        i_count++;
    }
    if (ctrl.ex_skip == 1) {
        branch_jr_count++;
    }
    
    // write_reg 설정 
    if (ctrl.reg_dst == 1) {                                
        id_ex_latch.write_reg = (instruction >> 11) & 0x0000001f;        // rd
    }
    
    // immediate 처리
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
    
    // 브랜치/점프 처리
    if (ctrl.ex_skip == 1) {
        // 브랜치 명령어에 대해서만 예측 적용
        if (opcode == 0x4 || opcode == 0x5) {        // beq, bne
            // 브랜치 예측 수행
            bool predicted_taken = predict_branch(pc);
            
            uint32_t oper1 = (if_id_latch.forward_a >= 1) ? if_id_latch.forward_a_val : registers.regs[inst.rs];
            uint32_t oper2 = (if_id_latch.forward_b >= 1) ? if_id_latch.forward_b_val : registers.regs[inst.rt];

            int beq_bne = (opcode == 0x4) ? 1 : 0;   // beq = 1, bne = 0
            int check = (oper1 == oper2);      // 동등 여부 파악
            bool actual_taken = (check == beq_bne);
            
            printf("[ID] Branch: R%d(0x%x) %s R%d(0x%x), predicted=%s, actual=%s\n", 
                   inst.rs, oper1, 
                   (opcode == 0x4) ? "==" : "!=", 
                   inst.rt, oper2,
                   predicted_taken ? "taken" : "not_taken",
                   actual_taken ? "taken" : "not_taken");
            
            // 브랜치 예측기 업데이트
            update_branch_predictor(pc, actual_taken, predicted_taken);
            
            if (actual_taken) {
                // taken
                uint32_t sign_imm = instruction & 0xffff;                     
                uint32_t branchaddr = sign_imm;

                if ((sign_imm >> 15) == 1) { // branch-addr
                    branchaddr = ((sign_imm << 2) | 0xfffc0000);
                }
                else if ((sign_imm >> 15) == 0) {
                    branchaddr = ((sign_imm << 2) & 0x3ffc);
                }
                uint32_t new_pc = registers.pc + branchaddr;
                printf("[ID] Branch taken: PC = 0x%x -> 0x%x\n", registers.pc, new_pc);
                registers.pc = new_pc;
                
                if (!predicted_taken) {
                    printf("[ID] Branch misprediction: predicted not taken, actually taken\n");
                }
                return;
            }
            else {
                printf("[ID] Branch not taken: PC continues to 0x%x\n", registers.pc + 4);
                if (predicted_taken) {
                    printf("[ID] Branch misprediction: predicted taken, actually not taken\n");
                }
                return;
            }
        }   
        // J (점프는 예측 불필요 - 항상 taken)
        else if (opcode == 0x2) {   // j
            uint32_t jaddr = inst.jump_target << 2;
            printf("[ID] Jump: PC = 0x%x -> 0x%x\n", registers.pc, jaddr);
            registers.pc = jaddr;
            return;
        }   
        // JAL (점프는 예측 불필요 - 항상 taken)
        else if (opcode == 0x3) {   // jal
            uint32_t jaddr = inst.jump_target << 2;
            printf("[ID] Jump and Link: PC = 0x%x -> 0x%x, R31 = 0x%x\n", 
                   registers.pc, jaddr, registers.pc + 4);
            registers.regs[31] = registers.pc + 4; // pc+8 
            registers.pc = jaddr;
            return;
        }   
        // JR (레지스터 점프는 예측하기 어려움 - 일단 예측 없이)
        else {// jr
            uint32_t oper1 = (if_id_latch.forward_a >= 1) ? if_id_latch.forward_a_val : registers.regs[inst.rs];
            printf("[ID] Jump Register: PC = 0x%x -> 0x%x (from R%d)\n", 
                   registers.pc, oper1, inst.rs);
            registers.pc = oper1;
            return;
        }
    }
    
    // 레지스터 값 읽기 정보 출력
    inst.rs_value = registers.regs[inst.rs];     
    inst.rt_value = registers.regs[inst.rt];

    if (inst.rs != 0 || inst.rt != 0) {
        printf("[ID] Read: R%d=0x%x, R%d=0x%x\n", 
               inst.rs, inst.rs_value, inst.rt, inst.rt_value);
    }
    
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