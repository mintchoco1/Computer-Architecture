#include "structure.h"

static uint64_t stall_count = 0;

ForwardingUnit detect_forwarding(void) {
    ForwardingUnit unit = {0, 0};
    
    if (!id_ex_latch.valid) {
        return unit;
    }

    if (id_ex_latch.control_signals.rs_ch == 1 && id_ex_latch.control_signals.ex_skip == 0) {
        if (ex_mem_latch.control_signals.reg_wb == 1 && ex_mem_latch.write_reg != 0 && 
            id_ex_latch.instruction.rs == ex_mem_latch.write_reg) {
            id_ex_latch.forward_a = 0b10;
            if (ex_mem_latch.control_signals.mem_read != 1) {
                id_ex_latch.forward_a_val = ex_mem_latch.alu_result;
            }
        }

        else if (mem_wb_latch.control_signals.reg_wb == 1 && mem_wb_latch.write_reg != 0 &&
                 id_ex_latch.instruction.rs == mem_wb_latch.write_reg) {
            id_ex_latch.forward_a = 0b01;
            id_ex_latch.forward_a_val = (mem_wb_latch.control_signals.mem_read == 1) ? 
                                         mem_wb_latch.rt_value : mem_wb_latch.alu_result;
        }
    }

    if (id_ex_latch.control_signals.rt_ch == 1 && id_ex_latch.control_signals.ex_skip == 0) {
        if (ex_mem_latch.control_signals.reg_wb == 1 && ex_mem_latch.write_reg != 0 && 
            id_ex_latch.instruction.rt == ex_mem_latch.write_reg) {
            id_ex_latch.forward_b = 0b10;
            if (ex_mem_latch.control_signals.mem_read != 1) {
                id_ex_latch.forward_b_val = ex_mem_latch.alu_result;
            }
        }

        else if (mem_wb_latch.control_signals.reg_wb == 1 && mem_wb_latch.write_reg != 0 &&
                 id_ex_latch.instruction.rt == mem_wb_latch.write_reg) {
            id_ex_latch.forward_b = 0b01;
            id_ex_latch.forward_b_val = (mem_wb_latch.control_signals.mem_read == 1) ? 
                                         mem_wb_latch.rt_value : mem_wb_latch.alu_result;
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
    
    if (!((opcode == 0x4 || opcode == 0x5) || (opcode == 0x0 && funct == 0x08))) {
        return unit;
    }

    if (ex_mem_latch.valid && ex_mem_latch.control_signals.reg_wb && 
        ex_mem_latch.write_reg != 0) {
        
        if (ex_mem_latch.write_reg == if_id_latch.reg_src) {
            if_id_latch.forward_a = 0b01;
            if (ex_mem_latch.control_signals.mem_read == 1) {
                // LW의 경우 다음 단계에서 값 업데이트
            } else {
                if_id_latch.forward_a_val = ex_mem_latch.alu_result;
            }
        }
        
        if ((opcode == 0x4 || opcode == 0x5) && ex_mem_latch.write_reg == if_id_latch.reg_tar) {
            if_id_latch.forward_b = 0b01;
            if (ex_mem_latch.control_signals.mem_read == 1) {
                // LW의 경우 다음 단계에서 값 업데이트
            } else {
                if_id_latch.forward_b_val = ex_mem_latch.alu_result;
            }
        }
    }

    // ID/EX 단계에서 포워딩
    if (id_ex_latch.valid && id_ex_latch.control_signals.reg_wb && 
        id_ex_latch.write_reg != 0) {
        
        if (id_ex_latch.write_reg == if_id_latch.reg_src) {
            if_id_latch.forward_a = 0b10;
        }
        
        if ((opcode == 0x4 || opcode == 0x5) && id_ex_latch.write_reg == if_id_latch.reg_tar) {
            if_id_latch.forward_b = 0b10;
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
        printf("로드-사용 해저드! 스톨 (LW dest: R%d)\n", lw_dest);
        stall_count++;  // 로컬 변수 사용
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
    registers.pc -= 4;
    printf("스톨: PC=0x%08x\n", registers.pc);
}

void handle_branch_flush(void) {
    // 브랜치 미스예측 시 파이프라인 플러시
    if_id_latch.valid = false;
    id_ex_latch.valid = false;
    printf("파이프라인 플러시 (브랜치/점프)\n");
}