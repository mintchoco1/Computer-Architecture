// hazard.c의 개선된 함수들

#include "structure.h"

static uint64_t stall_count = 0;

ForwardingUnit detect_forwarding(void) {
    ForwardingUnit unit = {0, 0};
    
    if (!id_ex_latch.valid) {
        return unit;
    }

    // EX 단계 포워딩 검출 (참고 코드와 동일한 로직)
    if (id_ex_latch.control_signals.rs_ch == 1 && id_ex_latch.control_signals.ex_skip == 0) {
        int temp1 = 0;
        
        // EX/MEM에서 포워딩
        if (ex_mem_latch.valid && ex_mem_latch.control_signals.reg_wb == 1 && 
            ex_mem_latch.write_reg != 0 && id_ex_latch.instruction.rs == ex_mem_latch.write_reg) {
            id_ex_latch.forward_a = 0b10;
            if (ex_mem_latch.control_signals.mem_read != 1) {
                id_ex_latch.forward_a_val = ex_mem_latch.alu_result;
            }
            temp1 = 1;
            printf("[HAZARD] EX forwarding: R%d from EX/MEM\n", id_ex_latch.instruction.rs);
        }
        
        // MEM/WB에서 포워딩 (EX/MEM에서 포워딩이 없을 때만)
        if ((temp1 == 0) && mem_wb_latch.valid && mem_wb_latch.control_signals.reg_wb == 1 && 
            id_ex_latch.instruction.rs == mem_wb_latch.write_reg) {
            id_ex_latch.forward_a = 0b01;
            id_ex_latch.forward_a_val = (mem_wb_latch.control_signals.mem_read == 1) ? 
                                        mem_wb_latch.rt_value : mem_wb_latch.alu_result;
            printf("[HAZARD] MEM forwarding: R%d from MEM/WB\n", id_ex_latch.instruction.rs);
        }
    }

    if (id_ex_latch.control_signals.rt_ch == 1 && id_ex_latch.control_signals.ex_skip == 0) {
        int temp2 = 0;
        
        // EX/MEM에서 포워딩
        if (ex_mem_latch.valid && ex_mem_latch.control_signals.reg_wb == 1 && 
            ex_mem_latch.write_reg != 0 && id_ex_latch.instruction.rt == ex_mem_latch.write_reg) {
            id_ex_latch.forward_b = 0b10;
            if (ex_mem_latch.control_signals.mem_read != 1) {
                id_ex_latch.forward_b_val = ex_mem_latch.alu_result;
            }
            temp2 = 1;
            printf("[HAZARD] EX forwarding: R%d from EX/MEM\n", id_ex_latch.instruction.rt);
        }
        
        // MEM/WB에서 포워딩
        if ((temp2 == 0) && mem_wb_latch.valid && mem_wb_latch.control_signals.reg_wb == 1 && 
            id_ex_latch.instruction.rt == mem_wb_latch.write_reg) {
            id_ex_latch.forward_b = 0b01;
            id_ex_latch.forward_b_val = (mem_wb_latch.control_signals.mem_read == 1) ? 
                                        mem_wb_latch.rt_value : mem_wb_latch.alu_result;
            printf("[HAZARD] MEM forwarding: R%d from MEM/WB\n", id_ex_latch.instruction.rt);
        }
    }

    return unit;
}

ForwardingUnit detect_branch_forwarding(void) {
    ForwardingUnit unit = {0, 0};
    
    if (!if_id_latch.valid) {
        return unit;
    }

    uint32_t opcode = if_id_latch.opcode;
    uint32_t funct = if_id_latch.funct;
    
    // 브랜치 또는 JR 명령어가 아니면 리턴
    if (!((opcode == 0x4 || opcode == 0x5) || (opcode == 0x0 && funct == 0x08))) {
        return unit;
    }

    // EX/MEM 단계에서 포워딩
    if (ex_mem_latch.valid && ex_mem_latch.control_signals.reg_wb && ex_mem_latch.write_reg != 0) {
        
        if (ex_mem_latch.write_reg == if_id_latch.reg_src) {
            if_id_latch.forward_a = 0b01;
            printf("[HAZARD] Branch forwarding: R%d from EX/MEM\n", if_id_latch.reg_src);
        }
        
        if ((opcode == 0x4 || opcode == 0x5) && ex_mem_latch.write_reg == if_id_latch.reg_tar) {
            if_id_latch.forward_b = 0b01;
            printf("[HAZARD] Branch forwarding: R%d from EX/MEM\n", if_id_latch.reg_tar);
        }
    }

    // ID/EX 단계에서 포워딩
    if (id_ex_latch.valid && id_ex_latch.control_signals.reg_wb && id_ex_latch.write_reg != 0) {
        
        if (id_ex_latch.write_reg == if_id_latch.reg_src) {
            if_id_latch.forward_a = 0b10;
            printf("[HAZARD] Branch forwarding: R%d from ID/EX\n", if_id_latch.reg_src);
        }
        
        if ((opcode == 0x4 || opcode == 0x5) && id_ex_latch.write_reg == if_id_latch.reg_tar) {
            if_id_latch.forward_b = 0b10;
            printf("[HAZARD] Branch forwarding: R%d from ID/EX\n", if_id_latch.reg_tar);
        }
    }

    return unit;
}

HazardUnit detect_hazard(void) {
    HazardUnit unit = {false, false};
    
    // Load 명령어가 ID/EX에 없으면 해저드 없음
    if (!id_ex_latch.valid || !id_ex_latch.control_signals.mem_read) {
        return unit;
    }
    
    // 다음 명령어가 없으면 해저드 없음
    if (!if_id_latch.valid) {
        return unit;
    }
    
    uint32_t lw_dest = id_ex_latch.write_reg;
    uint32_t next_rs = if_id_latch.reg_src;
    uint32_t next_rt = if_id_latch.reg_tar;
    uint32_t next_opcode = if_id_latch.opcode;
    uint32_t next_funct = if_id_latch.funct;
    
    if (lw_dest == 0) {
        return unit;
    }
    
    bool load_use_hazard = false;
    
    // R-type 명령어
    if (next_opcode == 0) {
        if ((lw_dest == next_rs) || (lw_dest == next_rt)) {
            load_use_hazard = true;
        }
    } 
    // 브랜치 명령어
    else if (next_opcode == 0x4 || next_opcode == 0x5) {
        if ((lw_dest == next_rs) || (lw_dest == next_rt)) {
            load_use_hazard = true;
        }
    } 
    // JR 명령어
    else if (next_opcode == 0x0 && next_funct == 0x08) {
        if (lw_dest == next_rs) {
            load_use_hazard = true;
        }
    } 
    // I-type 명령어 (rs만 사용)
    else {
        if (lw_dest == next_rs) {
            load_use_hazard = true;
        }
    }
    
    if (load_use_hazard) {
        unit.stall = true;
        printf("[HAZARD] Load-use hazard detected! LW dest: R%d\n", lw_dest);
        stall_count++;
    }
    
    return unit;
}

uint32_t get_forwarded_value(int forward_type, uint32_t original_value) {
    switch (forward_type) {
        case 1: // MEM/WB에서 포워딩
            if (!mem_wb_latch.valid) {
                return original_value;
            }
            if (mem_wb_latch.control_signals.mem_read == 1) {
                return mem_wb_latch.rt_value;
            } else {
                return mem_wb_latch.alu_result;
            }
            
        case 2: // EX/MEM에서 포워딩
            if (!ex_mem_latch.valid) {
                return original_value;
            }
            return ex_mem_latch.alu_result;
            
        default:
            return original_value;
    }
}

void handle_stall(void) {
    // PC를 되돌려서 같은 명령어를 다시 페치하도록 함
    registers.pc -= 4;
    printf("[HAZARD] Pipeline stall: PC rolled back to 0x%08x\n", registers.pc);
}

void handle_branch_flush(void) {
    // 브랜치 미스예측 시 파이프라인 플러시
    if_id_latch.valid = false;
    id_ex_latch.valid = false;
    printf("[HAZARD] Pipeline flush due to branch misprediction\n");
}