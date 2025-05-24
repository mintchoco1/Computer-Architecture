#include "structure.h"
#include <stdlib.h>

// 전역 변수들
uint8_t memory[MEMORY_SIZE] = {0};
Registers registers = {{0}, 0};
IF_ID_Latch if_id_latch = {0};
ID_EX_Latch id_ex_latch = {0};
EX_MEM_Latch ex_mem_latch = {0};
MEM_WB_Latch mem_wb_latch = {0};

uint64_t g_num_wb_commit = 0;

// 프로그램 파일을 메모리에 로드
int load_program(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        printf("파일을 열 수 없습니다: %s\n", filename);
        return -1;
    }

    // 파일 크기 확인
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size >= MEMORY_SIZE) {
        printf("파일이 너무 큽니다\n");
        fclose(file);
        return -1;
    }

    // 메모리에 로드
    size_t read_size = fread(memory, 1, file_size, file);
    fclose(file);

    if (read_size != file_size) {
        printf("파일 읽기 실패\n");
        return -1;
    }

    printf("프로그램 로드 완료: %ld 바이트\n", file_size);
    return 0;
}

// 레지스터 상태 출력
void print_registers() {
    printf("\n=== 레지스터 상태 ===\n");
    for (int i = 0; i < 32; i++) {
        if (i % 4 == 0) printf("\n");
        printf("R%d: 0x%08x  ", i, registers.regsp[i]);
    }
    printf("\nPC: 0x%08x\n", registers.pc);
    printf("완료된 명령어 수: %llu\n", (unsigned long long)g_num_wb_commit);
}

// 파이프라인 한 사이클 실행
bool run_one_cycle() {
    // 역순으로 실행 (데이터 충돌 방지)
    stage_WB();
    stage_MEM();
    stage_EX();
    stage_ID();
    stage_IF();

    // 종료 조건: PC가 0xFFFFFFFF이고 파이프라인이 비어있음
    bool pc_stopped = (registers.pc == 0xFFFFFFFF);
    bool pipeline_empty = !if_id_latch.valid && !id_ex_latch.valid &&
                         !ex_mem_latch.valid && !mem_wb_latch.valid;
    
    return !(pc_stopped && pipeline_empty);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("사용법: %s <프로그램파일.bin>\n", argv[0]);
        return 1;
    }

    // 초기화
    memset(&registers, 0, sizeof(registers));
    memset(&if_id_latch, 0, sizeof(if_id_latch));
    memset(&id_ex_latch, 0, sizeof(id_ex_latch));
    memset(&ex_mem_latch, 0, sizeof(ex_mem_latch));
    memset(&mem_wb_latch, 0, sizeof(mem_wb_latch));
    
    // RA 레지스터를 종료 주소로 설정
    registers.regsp[31] = 0xFFFFFFFF;
    registers.pc = 0;

    // 프로그램 로드
    if (load_program(argv[1]) != 0) {
        return 1;
    }

    printf("MIPS 파이프라인 시뮬레이터 시작\n");
    printf("초기 PC: 0x%08x\n", registers.pc);

    // 시뮬레이션 실행
    uint64_t cycle_count = 0;
    while (run_one_cycle()) {
        cycle_count++;
        
        // 너무 많은 사이클이면 무한루프 방지
        if (cycle_count > 1000000) {
            printf("너무 많은 사이클 실행됨. 중단합니다.\n");
            break;
        }
    }

    // 결과 출력
    printf("\n=== 시뮬레이션 완료 ===\n");
    printf("총 사이클 수: %llu\n", (unsigned long long)cycle_count);
    printf("최종 결과값 (v0): 0x%08x\n", registers.regsp[2]);
    
    print_registers();

    return 0;
}