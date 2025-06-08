#include "structure.h"
#include <stdlib.h>

uint8_t memory[MEMORY_SIZE] = {0};
Registers registers = {{0}, 0};
IF_ID_Latch if_id_latch = {0};
ID_EX_Latch id_ex_latch = {0};
EX_MEM_Latch ex_mem_latch = {0};
MEM_WB_Latch mem_wb_latch = {0};

uint64_t inst_count = 0;        
uint64_t r_count = 0;
uint64_t i_count = 0; 
uint64_t branch_jr_count = 0;
uint64_t lw_count = 0;
uint64_t sw_count = 0;
uint64_t nop_count = 0;
uint64_t write_reg_count = 0;
uint64_t g_stall_count = 0;  
uint64_t branch_predictions = 0;
uint64_t branch_correct_predictions = 0;
uint64_t branch_mispredictions = 0;

// 명령어 이름 반환 함수
const char* get_instruction_name(uint32_t opcode, uint32_t funct) {
    switch (opcode) {
        case 0:
            switch (funct) {
                case 0x20: return "add";
                case 0x21: return "addu";
                case 0x22: return "sub";
                case 0x23: return "subu";
                case 0x24: return "and";
                case 0x25: return "or";
                case 0x27: return "nor";
                case 0x2a: return "slt";
                case 0x2b: return "sltu";
                case 0x00: return "sll";
                case 0x02: return "srl";
                case 0x08: return "jr";
                case 0x09: return "jalr";
                default: return "unknown_r";
            }
        case 2: return "j";
        case 3: return "jal";
        case 4: return "beq";
        case 5: return "bne";
        case 8: return "addi";
        case 9: return "addiu";
        case 10: return "slti";
        case 11: return "sltiu";
        case 12: return "andi";
        case 13: return "ori";
        case 15: return "lui";
        case 35: return "lw";
        case 43: return "sw";
        default: return "unknown";
    }
}

// 명령어 상세 정보 출력 함수
void print_instruction_details(uint32_t pc, uint32_t instruction) {
    uint32_t opcode = instruction >> 26;
    uint32_t rs = (instruction >> 21) & 0x1f;
    uint32_t rt = (instruction >> 16) & 0x1f;
    uint32_t rd = (instruction >> 11) & 0x1f;
    uint32_t shamt = (instruction >> 6) & 0x1f;
    uint32_t funct = instruction & 0x3f;
    uint32_t immediate = instruction & 0xffff;
    uint32_t jump_target = instruction & 0x3ffffff;
    
    const char* inst_name = get_instruction_name(opcode, funct);
    
    printf("PC=0x%08x, Inst=0x%08x, %s ", pc, instruction, inst_name);
    
    // 명령어 타입별 상세 출력
    if (opcode == 0) { // R-type
        if (funct == 0x08) { // jr
            printf("$%d", rs);
        } else if (funct == 0x00 || funct == 0x02) { // sll, srl
            printf("$%d, $%d, %d", rd, rt, shamt);
        } else {
            printf("$%d, $%d, $%d", rd, rs, rt);
        }
    } else if (opcode == 2 || opcode == 3) { // j, jal
        printf("0x%x", jump_target << 2);
    } else if (opcode == 4 || opcode == 5) { // beq, bne
        int16_t signed_imm = (int16_t)immediate;
        printf("$%d, $%d, %d", rs, rt, signed_imm);
    } else if (opcode == 35 || opcode == 43) { // lw, sw
        int16_t signed_imm = (int16_t)immediate;
        printf("$%d, %d($%d)", rt, signed_imm, rs);
    } else if (opcode == 15) { // lui
        printf("$%d, 0x%x", rt, immediate);
    } else { // I-type
        if (opcode == 12 || opcode == 13) { // andi, ori (zero-extended)
            printf("$%d, $%d, 0x%x", rt, rs, immediate);
        } else { // sign-extended
            int16_t signed_imm = (int16_t)immediate;
            printf("$%d, $%d, %d", rt, rs, signed_imm);
        }
    }
}

void clear_latches(void) {
    memset(&if_id_latch, 0, sizeof(if_id_latch));
    memset(&id_ex_latch, 0, sizeof(id_ex_latch));
    memset(&ex_mem_latch, 0, sizeof(ex_mem_latch));
    memset(&mem_wb_latch, 0, sizeof(mem_wb_latch));
}

void init_registers(uint32_t entry_pc) {
    memset(registers.regs, 0, sizeof(registers.regs));
    registers.pc = entry_pc;
    registers.regs[31] = 0xFFFFFFFF;
    registers.regs[29] = 0x1000000;

    init_branch_predictor();
}

int load_program(const char *filename, uint32_t load_addr) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        perror("fopen");
        return -1;
    }

    uint32_t temp;
    size_t bytesRead;
    size_t memoryIndex = load_addr;
    
    while ((bytesRead = fread(&temp, sizeof(uint32_t), 1, fp)) > 0) {
        if (memoryIndex >= MEMORY_SIZE - 3) {
            fprintf(stderr, "Program too big for memory.\n");
            fclose(fp);
            return -1;
        }
        
        memory[memoryIndex++] = (temp >> 24) & 0xFF;
        memory[memoryIndex++] = (temp >> 16) & 0xFF;
        memory[memoryIndex++] = (temp >> 8) & 0xFF;
        memory[memoryIndex++] = temp & 0xFF;
    }
    
    fclose(fp);
    printf("Loaded program at 0x%08x, size: %zu bytes\n", load_addr, memoryIndex - load_addr);
    return 0;
}

bool step_pipeline(void) {
    static int exit_proc = 0;
    static int ctrl_flow[4] = {-1, -1, -1, -1};  
    
    // 사이클 정보 출력
    printf("\n========== Cycle %llu ==========\n", (unsigned long long)inst_count + 1);
    
    // inst_count 증가 
    inst_count++;
    
    // IF 단계에서 NOP 처리
    if (if_id_latch.valid && if_id_latch.instruction == 0) {
        ctrl_flow[0] = 0;
        nop_count++;
    }
    
    // ===== hazard.c의 함수들을 사용하여 해저드 검출 =====
    
    // 1. 로드-사용 해저드 검출
    HazardUnit hazard_unit = detect_hazard();
    if (hazard_unit.stall) {
        handle_stall();
        g_stall_count++;
        // 스톨 시 IF와 ID 단계를 멈춤
        ctrl_flow[0] = 0; // IF 스톨
        // ID는 이미 처리된 명령어를 유지
        return true; // 스톨 상태에서 계속 실행
    }
    
    // 2. 포워딩 검출 및 설정
    detect_forwarding();
    detect_branch_forwarding();
    
    // ===== 파이프라인 스테이지 실행 =====
    
    // 1. WB 단계
    if (ctrl_flow[3] == 1) {
        if (mem_wb_latch.valid) {
            stage_WB();
        }
    } else if (ctrl_flow[3] == 0) {
        printf("[WB] NOP\n");
    }
    
    // 2. MEM 단계  
    if (ctrl_flow[2] == 1) {
        if (ex_mem_latch.valid) {
            stage_MEM();
        }
    } else if (ctrl_flow[2] == 0) {
        printf("[MEM] NOP\n");
        mem_wb_latch.valid = false;
        memset(&mem_wb_latch, 0, sizeof(mem_wb_latch));
    }
    
    // 3. LW-EX hazard 값 업데이트 (브랜치 포워딩에서 필요)
    if (if_id_latch.forward_a == 0b01 && mem_wb_latch.valid) {
        if_id_latch.forward_a_val = (mem_wb_latch.control_signals.mem_read == 1) ? 
                                    mem_wb_latch.rt_value : mem_wb_latch.alu_result;
    }
    if (if_id_latch.forward_b == 0b01 && mem_wb_latch.valid) {
        if_id_latch.forward_b_val = (mem_wb_latch.control_signals.mem_read == 1) ? 
                                    mem_wb_latch.rt_value : mem_wb_latch.alu_result;
    }
    
    // 4. EX 단계
    if (ctrl_flow[1] == 1) {
        if (id_ex_latch.valid) {
            stage_EX();
        }
    } else if (ctrl_flow[1] == 0) {
        printf("[EX] NOP\n");
        ex_mem_latch.valid = false;
        memset(&ex_mem_latch, 0, sizeof(ex_mem_latch));
    }
    
    // 5. 브랜치 포워딩 값 업데이트 (EX)
    if (if_id_latch.forward_a == 0b10 && ex_mem_latch.valid) {
        if_id_latch.forward_a_val = ex_mem_latch.alu_result;
    }
    if (if_id_latch.forward_b == 0b10 && ex_mem_latch.valid) {
        if_id_latch.forward_b_val = ex_mem_latch.alu_result;
    }
    
    // 6. ID 단계
    if (ctrl_flow[0] == 1) {
        if (if_id_latch.valid) {
            stage_ID();
        }
    } else if (ctrl_flow[0] == 0) {
        printf("[ID] NOP\n");
        id_ex_latch.valid = false;
        memset(&id_ex_latch, 0, sizeof(id_ex_latch));
    }
    
    // 7. IF 단계
    if (registers.pc != 0xffffffff) {
        stage_IF();
    }
    
    // 8. PC 업데이트 및 ctrl_flow 시프트
    if (registers.pc != 0xffffffff) {
        registers.pc = registers.pc + 4;
        
        for (int i = 3; i > 0; i--) {
            ctrl_flow[i] = ctrl_flow[i-1];
        }
        
        ctrl_flow[0] = 1;
    } else if (registers.pc == 0xffffffff) {
        exit_proc++;
        for (int i = 3; i > 0; i--) {
            ctrl_flow[i] = ctrl_flow[i-1];
        }
        if (exit_proc >= 3) {
            ctrl_flow[0] = -1;
        }
    }
    
    return !(exit_proc > 5);
}

void print_statistics(void) {
    printf("================================================================================\n");
    printf("Return register (r2)                 : %d\n", registers.regs[2]);
    printf("Total clock cycle                    : %llu\n", (unsigned long long)inst_count);  
    printf("r-type count                         : %llu\n", (unsigned long long)r_count);
    printf("i-type count                         : %llu\n", (unsigned long long)i_count);
    printf("branch, j-type count, jr             : %llu\n", (unsigned long long)branch_jr_count);
    printf("lw count                             : %llu\n", (unsigned long long)lw_count);
    printf("sw count                             : %llu\n", (unsigned long long)sw_count);
    printf("nop count                            : %llu\n", (unsigned long long)nop_count);
    printf("register write count                 : %llu\n", (unsigned long long)write_reg_count);
    print_branch_prediction_stats();
    printf("=================================================================================\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "사용법: %s <program.bin> [entry_pc (hex)]\n", argv[0]);
        return 1;
    }

    uint32_t entry_pc = (argc >= 3) ? strtoul(argv[2], NULL, 16) : 0x00000000;

    printf("MIPS 5-Stage Pipeline Simulator\n");
    printf("==============================\n");

    clear_latches();
    init_registers(entry_pc);

    if (load_program(argv[1], entry_pc) != 0)
        return 1;

    printf("Starting simulation at PC=0x%08x\n", entry_pc);

    int max_cycles = 10000;
    
    while (step_pipeline() && inst_count < max_cycles) {
        // 실행
    }
    
    if (inst_count >= max_cycles) {
        printf("WARNING: 최대 사이클 수 도달. 무한 루프 가능성.\n");
    }

    print_statistics();
    printf("\nSimulation completed.\n");

    return 0;
}