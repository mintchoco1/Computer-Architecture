#include "structure.h"
#include <stdlib.h>
#include <time.h>

/*******************************************************
 *  Global definitions (storage actually allocated here)
 *******************************************************/
uint8_t   memory[MEMORY_SIZE]   = {0};
Registers registers            = {{0}, 0};
IF_ID_Latch  if_id_latch  = {0};
ID_EX_Latch  id_ex_latch  = {0};
EX_MEM_Latch ex_mem_latch = {0};
MEM_WB_Latch mem_wb_latch = {0};

/* 통계용 전역 변수 (stage_WB.c에서 extern) */
uint64_t g_num_wb_commit = 0;

/****************************************************************
 *  초기화 헬퍼들
 ****************************************************************/
static void clear_latches(void)
{
    memset(&if_id_latch,  0, sizeof(if_id_latch));
    memset(&id_ex_latch,  0, sizeof(id_ex_latch));
    memset(&ex_mem_latch, 0, sizeof(ex_mem_latch));
    memset(&mem_wb_latch, 0, sizeof(mem_wb_latch));
}

static void init_registers(uint32_t entry_pc)
{
    memset(registers.regsp, 0, sizeof(registers.regsp));
    registers.pc = entry_pc;
}

static int load_program(const char *filename, uint32_t load_addr)
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

    size_t nread = fread(&memory[load_addr], 1, fsize, fp);
    fclose(fp);

    if (nread != (size_t)fsize) {
        fprintf(stderr, "Failed to read entire program\n");
        return -1;
    }

    printf("Loaded %ld bytes at 0x%08x\n", fsize, load_addr);
    return 0;
}

/****************************************************************
 *  Utility : dump register state (optional)
 ****************************************************************/
static void dump_registers(void)
{
    printf("\n==== Register File ====");
    for (int i = 0; i < 32; i++) {
        if (i % 4 == 0) printf("\nR%02d:", i);
        printf(" %08x", registers.regsp[i]);
    }
    printf("\nPC : %08x\n", registers.pc);
    printf("Committed WB: %llu\n", (unsigned long long)g_num_wb_commit);
}

/****************************************************************
 *  메인 파이프라인 루프 (한 사이클 단위)
 ****************************************************************/
static bool step_pipeline(void)
{
    /* 1) Write‑back부터 차례로 호출하여 데이터 경쟁 방지  */
    stage_WB();   /* previous cycle results commit */
    stage_MEM();  /* access data memory            */
    stage_EX();   /* execute / ALU                 */
    stage_ID();   /* decode & regfile read         */
    stage_IF();   /* fetch next instruction        */

    /* 2) 종료 조건:   PC==0xFFFFFFFF 이고 모든 latch가 empty */
    bool pc_halt = (registers.pc == 0xFFFFFFFF);
    bool pipeline_empty = !if_id_latch.valid && !id_ex_latch.valid &&
                          !ex_mem_latch.valid && !mem_wb_latch.valid;
    return !(pc_halt && pipeline_empty);
}

/****************************************************************
 *  main
 ****************************************************************/
int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <program.bin> [entry_pc (hex)]\n", argv[0]);
        return 1;
    }

    uint32_t entry_pc = (argc >= 3) ? strtoul(argv[2], NULL, 16) : 0x00000000;

    /* 0. 초기화 */
    clear_latches();
    init_registers(entry_pc);

    if (load_program(argv[1], entry_pc) != 0)
        return 1;

    /* 1. 메인 시뮬레이션 루프 */
    uint64_t cycles = 0;
    clock_t t0 = clock();
    while (step_pipeline()) {
        cycles++;
    }
    clock_t t1 = clock();

    /* 2. 결과 출력 */
    dump_registers();
    printf("\nFinished in %llu cycles (%.3f s)\n", (unsigned long long)cycles,
           (double)(t1 - t0) / CLOCKS_PER_SEC);

    return 0;
}
