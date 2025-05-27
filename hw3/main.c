#include "structure.h"
#include <stdlib.h>
#include <time.h>

uint8_t memory[MEMORY_SIZE] = {0};
Registers registers = {{0}, 0};
IF_ID_Latch if_id_latch = {0};
ID_EX_Latch id_ex_latch = {0};
EX_MEM_Latch ex_mem_latch = {0};
MEM_WB_Latch mem_wb_latch = {0};

/* 통계용 전역 변수 */
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

    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (load_addr + fsize >= MEMORY_SIZE) {
        fprintf(stderr, "Program too big (size=%ld bytes) for memory.\n", fsize);
        fclose(fp);
        return -1;
    }

    // 참고 코드 방식으로 로드 (리틀 엔디안)
    uint32_t temp;
    size_t bytesRead;
    size_t memoryIndex = load_addr;
    
    while ((bytesRead = fread(&temp, sizeof(uint32_t), 1, fp)) > 0 && 
           memoryIndex < (MEMORY_SIZE - 3)) {
        // 리틀 엔디안으로 저장
        memory[memoryIndex++] = (temp >> 24) & 0xFF;
        memory[memoryIndex++] = (temp >> 16) & 0xFF;
        memory[memoryIndex++] = (temp >> 8) & 0xFF;
        memory[memoryIndex++] = temp & 0xFF;
    }
    
    fclose(fp);
    printf("Loaded program at 0x%08x\n", load_addr);
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
    printf("================================================================================\n");
}

bool step_pipeline(void)
{
    g_cycle_count++;
    
    printf("\n=== Cycle %llu ===\n", (unsigned long long)g_cycle_count);
    
    /* Write‑back부터 차례로 호출하여 데이터 경쟁 방지 */
    stage_WB();   /* previous cycle results commit */
    stage_MEM();  /* access data memory            */
    stage_EX();   /* execute / ALU                 */
    stage_ID();   /* decode & regfile read         */
    stage_IF();   /* fetch next instruction        */

    /* 종료 조건: PC==0xFFFFFFFF 이고 모든 latch가 empty */
    bool pc_halt = (registers.pc == 0xFFFFFFFF);
    bool pipeline_empty = !if_id_latch.valid && !id_ex_latch.valid &&
                          !ex_mem_latch.valid && !mem_wb_latch.valid;
    
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

    /* 초기화 */
    clear_latches();
    init_registers(entry_pc);

    if (load_program(argv[1], entry_pc) != 0)
        return 1;

    printf("Starting simulation at PC=0x%08x\n", entry_pc);
    printf("Termination condition: PC reaches 0xFFFFFFFF and pipeline is empty\n\n");

    /* 메인 시뮬레이션 루프 */
    clock_t t0 = clock();
    int max_cycles = 10000; // 무한 루프 방지
    
    while (step_pipeline() && g_cycle_count < max_cycles) {
        // 정상 실행
    }
    
    if (g_cycle_count >= max_cycles) {
        printf("WARNING: 최대 사이클 수 도달. 무한 루프 가능성.\n");
    }
    
    clock_t t1 = clock();

    /* 결과 출력 */
    dump_registers();
    print_statistics();
    
    printf("\nSimulation completed in %.3f seconds\n",
           (double)(t1 - t0) / CLOCKS_PER_SEC);

    return 0;
}