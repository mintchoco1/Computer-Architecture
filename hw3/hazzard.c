#include "structure.h"

// 포워딩 감지 함수 - 참고 코드 스타일로 수정
ForwardingUnit detect_forwarding(void) {
    ForwardingUnit unit = {0, 0};
    
    // ID/EX 단계가 유효하지 않으면 포워딩 불필요
    if (!id_ex_latch.valid) {
        return unit;
    }
    
    uint32_t rs = id_ex_latch.instruction.rs;
    uint32_t rt = id_ex_latch.instruction.rt;
    
    // EX/MEM 단계에서 포워딩 (EX hazard)
    if (ex_mem_latch.valid && ex_mem_latch.control_signals.regwrite && 
        ex_mem_latch.write_reg != 0) {
        
        // rs 포워딩 확인
        if (ex_mem_latch.write_reg == rs && id_ex_latch.control_signals.rs_ch) {
            unit.forward_a = 2;  // EX/MEM에서 포워딩
        }
        
        // rt 포워딩 확인
        if (ex_mem_latch.write_reg == rt && id_ex_latch.control_signals.rt_ch) {
            unit.forward_b = 2;  // EX/MEM에서 포워딩
        }
    }
    
    // MEM/WB 단계에서 포워딩 (MEM hazard) - EX 포워딩이 없을 때만
    if (mem_wb_latch.valid && mem_wb_latch.control_signals.regwrite && 
        mem_wb_latch.write_reg != 0) {
        
        // rs 포워딩 확인 (EX/MEM이 우선순위)
        if (unit.forward_a == 0 && mem_wb_latch.write_reg == rs && 
            id_ex_latch.control_signals.rs_ch) {
            unit.forward_a = 1;  // MEM/WB에서 포워딩
        }
        
        // rt 포워딩 확인
        if (unit.forward_b == 0 && mem_wb_latch.write_reg == rt && 
            id_ex_latch.control_signals.rt_ch) {
            unit.forward_b = 1;  // MEM/WB에서 포워딩
        }
    }
    
    return unit;
}

// 브랜치/점프 포워딩 감지 (참고 코드 스타일)
ForwardingUnit detect_branch_forwarding(void) {
    ForwardingUnit unit = {0, 0};
    
    if (!if_id_latch.valid) {
        return unit;
    }
    
    uint32_t opcode = if_id_latch.instruction >> 26;
    uint32_t rs = (if_id_latch.instruction >> 21) & 0x1f;
    uint32_t rt = (if_id_latch.instruction >> 16) & 0x1f;
    uint32_t funct = if_id_latch.instruction & 0x3f;
    
    // 브랜치나 jr 명령어만 처리
    bool is_branch = (opcode == 4 || opcode == 5);  // beq, bne
    bool is_jr = (opcode == 0 && funct == 0x8);     // jr
    
    if (!is_branch && !is_jr) {
        return unit;
    }
    
    // EX/MEM 단계에서 포워딩
    if (ex_mem_latch.valid && ex_mem_latch.control_signals.regwrite && 
        ex_mem_latch.write_reg != 0) {
        
        if (ex_mem_latch.write_reg == rs) {
            unit.forward_a = 2;  // EX/MEM에서 포워딩
        }
        
        if (is_branch && ex_mem_latch.write_reg == rt) {
            unit.forward_b = 2;  // EX/MEM에서 포워딩
        }
    }
    
    // MEM/WB 단계에서 포워딩
    if (mem_wb_latch.valid && mem_wb_latch.control_signals.regwrite && 
        mem_wb_latch.write_reg != 0) {
        
        if (unit.forward_a == 0 && mem_wb_latch.write_reg == rs) {
            unit.forward_a = 1;  // MEM/WB에서 포워딩
        }
        
        if (unit.forward_b == 0 && is_branch && mem_wb_latch.write_reg == rt) {
            unit.forward_b = 1;  // MEM/WB에서 포워딩
        }
    }
    
    return unit;
}

// 로드-사용 해저드 감지
HazardUnit detect_hazard(void) {
    HazardUnit unit = {false, false};
    
    // ID/EX 단계에서 로드 명령어가 있는지 확인
    if (!id_ex_latch.valid || !id_ex_latch.control_signals.memread) {
        return unit;
    }
    
    // IF/ID 단계가 유효하지 않으면 해저드 없음
    if (!if_id_latch.valid) {
        return unit;
    }
    
    uint32_t lw_dest = id_ex_latch.write_reg;
    uint32_t next_instruction = if_id_latch.instruction;
    uint32_t next_opcode = next_instruction >> 26;
    uint32_t next_rs = (next_instruction >> 21) & 0x1f;
    uint32_t next_rt = (next_instruction >> 16) & 0x1f;
    uint32_t next_funct = next_instruction & 0x3f;
    
    // 로드 목적지 레지스터가 0이면 해저드 없음
    if (lw_dest == 0) {
        return unit;
    }
    
    // 다음 명령어가 로드 결과를 바로 사용하는지 확인
    bool load_use_hazard = false;
    
    // R-type 명령어
    if (next_opcode == 0) {
        if (next_rs == lw_dest || next_rt == lw_dest) {
            load_use_hazard = true;
        }
    }
    // I-type 명령어들
    else if (next_opcode == 8 || next_opcode == 9 ||   // addi, addiu
             next_opcode == 10 || next_opcode == 11 ||  // slti, sltiu
             next_opcode == 12 || next_opcode == 13 ||  // andi, ori
             next_opcode == 35) {                       // lw
        if (next_rs == lw_dest) {
            load_use_hazard = true;
        }
    }
    // sw 명령어
    else if (next_opcode == 43) {  // sw
        if (next_rs == lw_dest || next_rt == lw_dest) {
            load_use_hazard = true;
        }
    }
    // 브랜치 명령어
    else if (next_opcode == 4 || next_opcode == 5) {  // beq, bne
        if (next_rs == lw_dest || next_rt == lw_dest) {
            load_use_hazard = true;
        }
    }
    
    if (load_use_hazard) {
        unit.stall = true;
        printf("로드-사용 해저드 감지! 스톨 발생 (LW dest: R%d)\n", lw_dest);
    }
    
    return unit;
}

// 포워딩된 값 가져오기
uint32_t get_forwarded_value(int forward_type, uint32_t original_value) {
    switch (forward_type) {
        case 1: // MEM/WB에서 포워딩
            if (mem_wb_latch.control_signals.memtoreg) {
                return mem_wb_latch.rt_value;  // 로드된 데이터
            } else {
                return mem_wb_latch.alu_result; // ALU 결과
            }
            
        case 2: // EX/MEM에서 포워딩
            if (ex_mem_latch.control_signals.memread) {
                // 로드 명령어의 경우 메모리 단계 완료 후 값 사용
                // 이 경우는 실제로는 스톨이 발생해야 하지만, 
                // 여기서는 ALU 결과(주소)를 반환
                return ex_mem_latch.alu_result;
            } else {
                return ex_mem_latch.alu_result;
            }
            
        default: // 포워딩 없음
            return original_value;
    }
}

// 브랜치/점프 포워딩된 값 가져오기
uint32_t get_branch_forwarded_value(int forward_type, uint32_t reg_index) {
    switch (forward_type) {
        case 1: // MEM/WB에서 포워딩
            if (mem_wb_latch.control_signals.memtoreg) {
                return mem_wb_latch.rt_value;  // 로드된 데이터
            } else {
                return mem_wb_latch.alu_result; // ALU 결과
            }
            
        case 2: // EX/MEM에서 포워딩
            return ex_mem_latch.alu_result;
            
        default: // 포워딩 없음
            return registers.regs[reg_index];
    }
}

// 스톨 처리 함수
void handle_stall(void) {
    // PC를 현재 위치로 되돌림 (같은 명령어를 다시 페치)
    registers.pc -= 4;
    
    // IF/ID 래치는 그대로 유지
    // ID/EX 래치에 NOP 삽입
    id_ex_latch.valid = false;
    
    printf("스톨 처리: PC=0x%08x로 되돌림, NOP 삽입\n", registers.pc);
}

// 분기/점프 플러시 처리
void handle_branch_flush(void) {
    // 분기나 점프가 taken되면 잘못 페치한 명령어들을 무효화
    if_id_latch.valid = false;
    id_ex_latch.valid = false;
    
    printf("분기/점프 플러시: 파이프라인 무효화\n");
}