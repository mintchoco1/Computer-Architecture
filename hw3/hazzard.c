#include "structure.h"

extern uint64_t g_stall_count;

ForwardingUnit detect_forwarding(void) {
    ForwardingUnit unit = {0, 0};
    
    if (!id_ex_latch.valid) {
        return unit;
    }
    
    uint32_t rs = id_ex_latch.instruction.rs;
    uint32_t rt = id_ex_latch.instruction.rt;
    
    // EX/MEM 단계에서 포워딩 (EX hazard)
    if (ex_mem_latch.valid && ex_mem_latch.control_signals.regwrite && 
        ex_mem_latch.write_reg != 0) {
        
        if (ex_mem_latch.write_reg == rs && id_ex_latch.control_signals.rs_ch) {
            unit.forward_a = 2;  // EX/MEM에서 포워딩
        }
        
        if (ex_mem_latch.write_reg == rt && id_ex_latch.control_signals.rt_ch) {
            unit.forward_b = 2;  // EX/MEM에서 포워딩
        }
    }
    
    // MEM/WB 단계에서 포워딩 (MEM hazard)
    if (mem_wb_latch.valid && mem_wb_latch.control_signals.regwrite && 
        mem_wb_latch.write_reg != 0) {
        
        if (unit.forward_a == 0 && mem_wb_latch.write_reg == rs && 
            id_ex_latch.control_signals.rs_ch) {
            unit.forward_a = 1;  // MEM/WB에서 포워딩
        }
        
        if (unit.forward_b == 0 && mem_wb_latch.write_reg == rt && 
            id_ex_latch.control_signals.rt_ch) {
            unit.forward_b = 1;  // MEM/WB에서 포워딩
        }
    }
    
    return unit;
}

HazardUnit detect_hazard(void) {
    HazardUnit unit = {false, false};
    
    // 로드-사용 해저드만 간단히 체크
    if (!id_ex_latch.valid || !id_ex_latch.control_signals.memread) {
        return unit;
    }
    
    if (!if_id_latch.valid) {
        return unit;
    }
    
    uint32_t lw_dest = id_ex_latch.write_reg;
    uint32_t next_instruction = if_id_latch.instruction;
    uint32_t next_opcode = next_instruction >> 26;
    uint32_t next_rs = (next_instruction >> 21) & 0x1f;
    uint32_t next_rt = (next_instruction >> 16) & 0x1f;
    
    if (lw_dest == 0) {
        return unit;
    }
    
    bool load_use_hazard = false;
    
    // R-type
    if (next_opcode == 0) {
        if (next_rs == lw_dest || next_rt == lw_dest) {
            load_use_hazard = true;
        }
    }
    // I-type들
    else if (next_opcode == 8 || next_opcode == 9 ||   // addi, addiu
             next_opcode == 10 || next_opcode == 11 ||  // slti, sltiu
             next_opcode == 12 || next_opcode == 13 ||  // andi, ori
             next_opcode == 35 || next_opcode == 43) {  // lw, sw
        if (next_rs == lw_dest) {
            load_use_hazard = true;
        }
        if (next_opcode == 43 && next_rt == lw_dest) { // sw
            load_use_hazard = true;
        }
    }
    // 브랜치
    else if (next_opcode == 4 || next_opcode == 5) {  // beq, bne
        if (next_rs == lw_dest || next_rt == lw_dest) {
            load_use_hazard = true;
        }
    }
    
    if (load_use_hazard) {
        unit.stall = true;
        printf("로드-사용 해저드! 스톨 (LW dest: R%d)\n", lw_dest);
        g_stall_count++;
    }
    
    return unit;
}

uint32_t get_forwarded_value(int forward_type, uint32_t original_value) {
    switch (forward_type) {
        case 1: // MEM/WB에서 포워딩
            if (mem_wb_latch.control_signals.memtoreg) {
                return mem_wb_latch.rt_value;
            } else {
                return mem_wb_latch.alu_result;
            }
            
        case 2: // EX/MEM에서 포워딩
            return ex_mem_latch.alu_result;
            
        default:
            return original_value;
    }
}

void handle_stall(void) {
    // PC 되돌리고 NOP 삽입
    registers.pc -= 4;
    id_ex_latch.valid = false;
    printf("스톨: PC=0x%08x\n", registers.pc);
}

void handle_branch_flush(void) {
    // 잘못 페치한 명령어들 무효화
    if_id_latch.valid = false;
    id_ex_latch.valid = false;
    printf("파이프라인 플러시\n");
}