#include "structure.h"

// 포워딩 감지 함수
ForwardingUnit detect_forwarding(void) {
    ForwardingUnit unit = {0, 0};
    
    // ID/EX 래치가 유효하지 않으면 포워딩 불필요
    if (!id_ex_latch.valid) {
        return unit;
    }
    
    uint32_t rs = id_ex_latch.instruction.rs;
    uint32_t rt = id_ex_latch.instruction.rt;
    
    // EX/MEM 단계에서 포워딩 (1사이클 뒤)
    if (ex_mem_latch.valid && ex_mem_latch.control_signals.regwrite && ex_mem_latch.write_reg != 0) {
        
        // rs 포워딩 확인
        if (ex_mem_latch.write_reg == rs) {
            unit.forward_a = 1;
        }
        
        // rt 포워딩 확인 (SW는 제외 - SW에서 rt는 저장할 데이터)
        if (ex_mem_latch.write_reg == rt && 
            !ex_mem_latch.control_signals.memwrite) {
            unit.forward_b = 1;
        }
    }
    
    // MEM/WB 단계에서 포워딩 (2사이클 뒤)
    // EX/MEM 포워딩이 없을 때만 적용 (우선순위)
    if (mem_wb_latch.valid && mem_wb_latch.control_signals.regwrite && mem_wb_latch.write_reg != 0) {
        
        // rs 포워딩 확인 (EX/MEM이 우선순위 높음)
        if (unit.forward_a == 0 && mem_wb_latch.write_reg == rs) {
            unit.forward_a = 2;
        }
        
        // rt 포워딩 확인
        if (unit.forward_b == 0 && mem_wb_latch.write_reg == rt && !id_ex_latch.control_signals.memwrite) {
            unit.forward_b = 2;
        }
    }
    
    return unit;
}

// 로드-사용 해저드 감지 (스톨 필요한 경우)
HazardUnit detect_hazard(void) {
    HazardUnit unit = {false, false};
    
    // IF/ID와 ID/EX 래치가 모두 유효해야 함
    if (!if_id_latch.valid || !id_ex_latch.valid) {
        return unit;
    }
    
    // ID/EX 단계에서 메모리 읽기(LW)가 있는지 확인
    if (id_ex_latch.control_signals.memread) {
        uint32_t lw_dest = id_ex_latch.write_reg;
        
        // 다음 명령어 디코딩
        uint32_t next_instruction = if_id_latch.instruction;
        uint32_t next_opcode = next_instruction >> 26;
        uint32_t next_rs = (next_instruction >> 21) & 0x1f;
        uint32_t next_rt = (next_instruction >> 16) & 0x1f;
        
        // 로드-사용 해저드 확인
        bool load_use_hazard = false;
        
        // 다음 명령어가 LW의 결과를 바로 사용하는지 확인
        if (lw_dest != 0) {
            // R-type 명령어 (opcode = 0)
            if (next_opcode == 0) {
                if (next_rs == lw_dest || next_rt == lw_dest) {
                    load_use_hazard = true;
                }
            }
            // I-type 명령어들
            else if (next_opcode == 8 ||   // addi
                     next_opcode == 9 ||   // addiu
                     next_opcode == 10 ||  // slti
                     next_opcode == 11 ||  // sltiu
                     next_opcode == 12 ||  // andi
                     next_opcode == 13 ||  // ori
                     next_opcode == 35 ||  // lw
                     next_opcode == 43) {  // sw
                // rs를 사용하는 명령어들
                if (next_rs == lw_dest) {
                    load_use_hazard = true;
                }
                // sw에서 저장할 데이터가 lw 결과인 경우
                if (next_opcode == 43 && next_rt == lw_dest) {
                    load_use_hazard = true;
                }
            }
            // 분기 명령어들
            else if (next_opcode == 4 || next_opcode == 5) { // beq, bne
                if (next_rs == lw_dest || next_rt == lw_dest) {
                    load_use_hazard = true;
                }
            }
        }
        
        if (load_use_hazard) {
            unit.stall = true;
            printf("로드-사용 해저드 감지! 스톨 발생 (LW dest: R%d)\n", lw_dest);
        }
    }
    
    return unit;
}

// 포워딩된 값 가져오기
uint32_t get_forwarded_value(int forward_type, uint32_t original_value) {
    switch (forward_type) {
        case 1: // EX/MEM에서 포워딩
            return ex_mem_latch.alu_result;
            
        case 2: // MEM/WB에서 포워딩
            // 로드 명령어면 메모리 데이터, 아니면 ALU 결과
            if (mem_wb_latch.control_signals.memtoreg) {
                return mem_wb_latch.rt_value;  // 로드된 데이터
            } else {
                return mem_wb_latch.alu_result; // ALU 결과
            }
            
        default: // 포워딩 없음
            return original_value;
    }
}

// 스톨 처리 함수
void handle_stall(void) {
    // PC 업데이트 중단 (IF에서 이미 +4 했으므로 되돌림)
    registers.pc -= 4;
    
    // IF/ID 래치는 그대로 유지 (같은 명령어를 다시 페치하도록)
    // ID/EX 래치에 NOP(bubble) 삽입
    id_ex_latch.valid = false;
    
    printf("스톨 처리: PC 되돌림 (0x%08x), NOP 삽입\n", registers.pc);
}

// 분기/점프 플러시 처리
void handle_branch_flush(void) {
    // 분기나 점프가 taken되면 IF/ID, ID/EX 무효화
    if_id_latch.valid = false;
    id_ex_latch.valid = false;
    
    printf("분기/점프 플러시: IF/ID, ID/EX 무효화\n");
}

// 디버그용: 포워딩 상태 출력
void print_forwarding_status(ForwardingUnit unit) {
    if (unit.forward_a != 0 || unit.forward_b != 0) {
        printf("포워딩 - ");
        if (unit.forward_a == 1) printf("A: EX/MEM→EX ");
        else if (unit.forward_a == 2) printf("A: MEM/WB→EX ");
        
        if (unit.forward_b == 1) printf("B: EX/MEM→EX ");
        else if (unit.forward_b == 2) printf("B: MEM/WB→EX ");
        
        printf("\n");
    }
}