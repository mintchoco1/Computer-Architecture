#include "structure.h"
#include <stdlib.h>

uint8_t memory[MEMORY_SIZE] = {0};
Registers registers = {{0}, 0};
IF_ID_Latch if_id_latch = {0};
ID_EX_Latch id_ex_latch = {0};
EX_MEM_Latch ex_mem_latch = {0};
MEM_WB_Latch mem_wb_latch = {0};

uint64_t g_num_wb_commit = 0;
uint64_t g_cycle_count = 0;
uint64_t g_r_type_count = 0;
uint64_t g_i_type_count = 0;
uint64_t g_j_type_count = 0;
uint64_t g_branch_jr_count = 0;
uint64_t g_lw_count = 0;
uint64_t g_sw_count = 0;
uint64_t g_nop_count = 0;
uint64_t g_write_reg_count = 0;
uint64_t g_branch_count = 0;
uint64_t g_branch_taken = 0;
uint64_t g_stall_count = 0;

void clear_latches(void)
{
    memset(&if_id_latch, 0, sizeof(if_id_latch));
    memset(&id_ex_latch, 0, sizeof(id_ex_latch));
    memset(&ex_mem_latch, 0, sizeof(ex_mem_latch));
    memset(&mem_wb_latch, 0, sizeof(mem_wb_latch));
}

void init_registers(uint32_t entry_pc)
{
    memset(registers.regs, 0, sizeof(registers.regs));
    registers.pc = entry_pc;
    registers.regs[31] = 0xFFFFFFFF;  // 종료 조건
    registers.regs[29] = 0x1000000;   // 스택 포인터
}

int load_program(const char *filename, uint32_t load_addr)
{
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
        
        // 빅 엔디안으로 저장
        memory[memoryIndex++] = (temp >> 24) & 0xFF;
        memory[memoryIndex++] = (temp >> 16) & 0xFF;
        memory[memoryIndex++] = (temp >> 8) & 0xFF;
        memory[memoryIndex++] = temp & 0xFF;
    }
    
    fclose(fp);
    printf("Loaded program at 0x%08x, size: %zu bytes\n", load_addr, memoryIndex - load_addr);
    return 0;
}

void dump_registers(void)
{
    printf("\n==== Register File ====");
    for (int i = 0; i < 32; i++) {
        if (i % 4 == 0) printf("\nR%02d:", i);
        printf(" %08x", registers.regs[i]);
    }
    printf("\nPC : %08x\n", registers.pc);
}

const char* get_instruction_name(uint32_t opcode, uint32_t funct) {
    switch (opcode) {
        case 0: // R-type
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

void print_pipeline_state(void) {
    printf("Pipeline: ");
    
    // IF
    printf("IF[");
    if (if_id_latch.valid) {
        printf("0x%x", if_id_latch.pc);
    } else {
        printf("NOP");
    }
    printf("] ");
    
    // ID
    printf("ID[");
    if (id_ex_latch.valid) {
        printf("%s", get_instruction_name(id_ex_latch.instruction.opcode, id_ex_latch.instruction.funct));
    } else {
        printf("NOP");
    }
    printf("] ");
    
    // EX
    printf("EX[");
    if (ex_mem_latch.valid) {
        printf("%s", get_instruction_name(ex_mem_latch.instruction.opcode, ex_mem_latch.instruction.funct));
    } else {
        printf("NOP");
    }
    printf("] ");
    
    // MEM
    printf("MEM[");
    if (mem_wb_latch.valid) {
        printf("%s", get_instruction_name(mem_wb_latch.instruction.opcode, mem_wb_latch.instruction.funct));
    } else {
        printf("NOP");
    }
    printf("]\n");
}

void print_statistics(void)
{
    printf("\n================================================================================\n");
    printf("Return register (r2)                 : %d\n", registers.regs[2]);
    printf("Total clock cycle                    : %llu\n", (unsigned long long)g_cycle_count);
    printf("R-type count                         : %llu\n", (unsigned long long)g_r_type_count);
    printf("I-type count                         : %llu\n", (unsigned long long)g_i_type_count);
    printf("Branch, j-type count, jr             : %llu\n", (unsigned long long)g_branch_jr_count);
    printf("LW count                             : %llu\n", (unsigned long long)g_lw_count);
    printf("SW count                             : %llu\n", (unsigned long long)g_sw_count);
    printf("NOP count                            : %llu\n", (unsigned long long)g_nop_count);
    printf("Register write count                 : %llu\n", (unsigned long long)g_write_reg_count);
    printf("Stall count                          : %llu\n", (unsigned long long)g_stall_count);
    printf("Branch count                         : %llu\n", (unsigned long long)g_branch_count);
    printf("Branch taken                         : %llu\n", (unsigned long long)g_branch_taken);
    printf("================================================================================\n");
}

bool step_pipeline(void)
{
    g_cycle_count++;
    
    printf("\n=== Cycle %llu ===\n", (unsigned long long)g_cycle_count);
    
    stage_WB();   
    stage_MEM();  
    stage_EX();   
    stage_ID();   
    stage_IF();   

    print_pipeline_state();

    bool pc_halt = (registers.pc == 0xFFFFFFFF);
    bool pipeline_empty = !if_id_latch.valid && !id_ex_latch.valid && !ex_mem_latch.valid && !mem_wb_latch.valid;
    
    return !(pc_halt && pipeline_empty);
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <program.bin> [entry_pc (hex)]\n", argv[0]);
        return 1;
    }

    uint32_t entry_pc = (argc >= 3) ? strtoul(argv[2], NULL, 16) : 0x00000000;

    printf("MIPS 5-Stage Pipeline Simulator\n");
    printf("================================\n");

    clear_latches();
    init_registers(entry_pc);

    if (load_program(argv[1], entry_pc) != 0)
        return 1;

    printf("Starting simulation at PC=0x%08x\n", entry_pc);

    int max_cycles = 10000; 
    
    while (step_pipeline() && g_cycle_count < max_cycles) {
        // getchar(); // 주석 해제하면 단계별 실행
    }
    
    if (g_cycle_count >= max_cycles) {
        printf("WARNING: 최대 사이클 수 도달. 무한 루프 가능성.\n");
    }

    dump_registers();
    print_statistics();
    
    printf("\nSimulation completed.\n");

    return 0;
}