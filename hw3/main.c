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
    
    // 해저드 검출을 위한 변수들
    int hazard_cnt = 0;
    int lw_ex_hazardA = 0;
    int lw_ex_hazardB = 0;
    int bj_lw_hazard = 0;
    
    // inst_count 증가 
    inst_count++;
    
    // IF 단계에서 NOP 처리
    if (if_id_latch.valid && if_id_latch.instruction == 0) {
        ctrl_flow[0] = 0;
        nop_count++;
    }
    
    // 1. EX 단계 포워딩 검출
    if (id_ex_latch.valid && id_ex_latch.control_signals.rs_ch == 1 && id_ex_latch.control_signals.ex_skip == 0) {
        int temp1 = 0;
        
        if (ex_mem_latch.valid && ex_mem_latch.control_signals.reg_wb == 1 && 
            ex_mem_latch.write_reg != 0 && id_ex_latch.instruction.rs == ex_mem_latch.write_reg) {
            id_ex_latch.forward_a = 0b10;
            hazard_cnt++;
            if (ex_mem_latch.control_signals.mem_read != 1) {
                id_ex_latch.forward_a_val = ex_mem_latch.alu_result;
            } else {
                lw_ex_hazardA = 1;
            }
            temp1 = 1;
        }
        
        if ((temp1 == 0) && mem_wb_latch.valid && mem_wb_latch.control_signals.reg_wb == 1 && 
            id_ex_latch.instruction.rs == mem_wb_latch.write_reg) {
            id_ex_latch.forward_a = 0b01;
            hazard_cnt++;
            id_ex_latch.forward_a_val = (mem_wb_latch.control_signals.mem_read == 1) ? 
                                        mem_wb_latch.rt_value : mem_wb_latch.alu_result;
        }
    }
    
    if (id_ex_latch.valid && id_ex_latch.control_signals.rt_ch == 1 && id_ex_latch.control_signals.ex_skip == 0) {
        int temp2 = 0;
        
        if (ex_mem_latch.valid && ex_mem_latch.control_signals.reg_wb == 1 && 
            ex_mem_latch.write_reg != 0 && id_ex_latch.instruction.rt == ex_mem_latch.write_reg) {
            id_ex_latch.forward_b = 0b10;
            hazard_cnt++;
            if (ex_mem_latch.control_signals.mem_read != 1) {
                id_ex_latch.forward_b_val = ex_mem_latch.alu_result;
            } else {
                lw_ex_hazardB = 1;
            }
            temp2 = 1;
        }
        
        if ((temp2 == 0) && mem_wb_latch.valid && mem_wb_latch.control_signals.reg_wb == 1 && 
            id_ex_latch.instruction.rt == mem_wb_latch.write_reg) {
            id_ex_latch.forward_b = 0b01;
            id_ex_latch.forward_b_val = (mem_wb_latch.control_signals.mem_read == 1) ? 
                                        mem_wb_latch.rt_value : mem_wb_latch.alu_result;
            hazard_cnt++;
        }
    }
    
    // 2. 브랜치/JR 포워딩 검출 
    if (if_id_latch.valid) {
        uint32_t opcode = if_id_latch.opcode;
        uint32_t funct = if_id_latch.funct;
        
        if ((opcode == 0x4 || opcode == 0x5) || (opcode == 0x0 && funct == 0x08)) {
            
            // MA 단계 포워딩
            if (ex_mem_latch.valid && ex_mem_latch.control_signals.reg_wb == 1 && ex_mem_latch.write_reg != 0) {
                if (ex_mem_latch.write_reg == if_id_latch.reg_src) {
                    if_id_latch.forward_a = 0b01;
                    hazard_cnt++;
                }
                
                if ((ex_mem_latch.write_reg == if_id_latch.reg_tar) && (opcode == 0x4 || opcode == 0x5)) {
                    if_id_latch.forward_b = 0b01;
                    hazard_cnt++;
                }
                
                if (ex_mem_latch.control_signals.mem_read == 1) {
                    bj_lw_hazard = 1;
                    hazard_cnt++;
                }
            }
            
            // EX 단계 포워딩
            if (id_ex_latch.valid && id_ex_latch.control_signals.reg_wb == 1 && id_ex_latch.write_reg != 0) {
                if (id_ex_latch.write_reg == if_id_latch.reg_src) {
                    if_id_latch.forward_a = 0b10;
                    hazard_cnt++;
                }
                
                if ((id_ex_latch.write_reg == if_id_latch.reg_tar) && (opcode == 0x4 || opcode == 0x5)) {
                    if_id_latch.forward_b = 0b10;
                    hazard_cnt++;
                }
            }
        }
    }
    
    // 1. WB 단계
    if (ctrl_flow[3] == 1) {
        if (mem_wb_latch.valid) {
            stage_WB();
        }
    } else if (ctrl_flow[3] == 0) {
        // NOP은 출력하지 않음 
    }
    
    // 2. MEM 단계  
    if (ctrl_flow[2] == 1) {
        if (ex_mem_latch.valid) {
            stage_MEM();
        }
    } else if (ctrl_flow[2] == 0) {
        // NOP 처리
        mem_wb_latch.valid = false;
        // 참고 코드에서는 init_memwb 호출
        memset(&mem_wb_latch, 0, sizeof(mem_wb_latch));
    }
    
    // 3. LW-EX hazard 값 업데이트
    if (lw_ex_hazardA == 1 && mem_wb_latch.valid) {
        id_ex_latch.forward_a_val = mem_wb_latch.rt_value;
    }
    if (lw_ex_hazardB == 1 && mem_wb_latch.valid) {
        id_ex_latch.forward_b_val = mem_wb_latch.rt_value;
    }
    
    // 4. 브랜치 포워딩 값 업데이트 (MA)
    if (if_id_latch.forward_a == 0b01 && mem_wb_latch.valid) {
        if_id_latch.forward_a_val = (bj_lw_hazard == 1) ? mem_wb_latch.rt_value : mem_wb_latch.alu_result;
    }
    if (if_id_latch.forward_b == 0b01 && mem_wb_latch.valid) {
        if_id_latch.forward_b_val = (bj_lw_hazard == 1) ? mem_wb_latch.rt_value : mem_wb_latch.alu_result;
    }
    
    // 5. EX 단계
    if (ctrl_flow[1] == 1) {
        if (id_ex_latch.valid) {
            stage_EX();
        }
    } else if (ctrl_flow[1] == 0) {
        // NOP 처리
        ex_mem_latch.valid = false;
        memset(&ex_mem_latch, 0, sizeof(ex_mem_latch));
    }
    
    // 6. 브랜치 포워딩 값 업데이트 (EX)
    if (if_id_latch.forward_a == 0b10 && ex_mem_latch.valid) {
        if_id_latch.forward_a_val = ex_mem_latch.alu_result;
    }
    if (if_id_latch.forward_b == 0b10 && ex_mem_latch.valid) {
        if_id_latch.forward_b_val = ex_mem_latch.alu_result;
    }
    
    // 7. ID 단계
    if (ctrl_flow[0] == 1) {
        if (if_id_latch.valid) {
            stage_ID();
        }
    } else if (ctrl_flow[0] == 0) {
        // NOP 처리
        id_ex_latch.valid = false;
        memset(&id_ex_latch, 0, sizeof(id_ex_latch));
    }
    
    // 8. IF 단계 (항상 실행)
    if (registers.pc != 0xffffffff) {
        stage_IF();
    }
    
    // 9. PC 업데이트 및 ctrl_flow 시프트 
    if (registers.pc != 0xffffffff) {
        registers.pc = registers.pc + 4;
        
        // ctrl_flow 시프트
        for (int i = 3; i > 0; i--) {
            ctrl_flow[i] = ctrl_flow[i-1];
        }
        
        ctrl_flow[0] = 1;  // stall_cnt 고려하지 않음 
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