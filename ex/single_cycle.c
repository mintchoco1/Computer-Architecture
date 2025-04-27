#include "single_cycle_func.h"


int main(void) {


    FILE* file = fopen("C:\\Users\\ldj23\\Desktop\\computer science\\hw2\\test_prog\\fib.bin", "rb"); // "filename.bin" => filename 변경해서 다른 파일 넣기.
    // fopen함수의 첫번째 매개변수에 아래 값을 넣어주면 실행가능.
    // "sum.bin"
    // "func.bin"
    // "fibonacci.bin"
    // "factorial.bin"
    // "power.bin"

    if (!file) {
        perror("File opening failed");
        return 1;
    }

    uint32_t temp; // temp 변수에 32bit를 받고 각각 넣어주기 위한 변수 : buffer 역할.
    size_t bytesRead; // 읽어 드린 개수 확인하는 변수 : 더 이상 읽어 들일 binary 값이 없으면 종료
    size_t mem_idx = 0; // Instruction Memory의 총 개수.

    while ((bytesRead = fread(&temp, sizeof(uint32_t), 1, file)) > 0 && mem_idx < (MAX_MEM - 3)) {
        // 파일에서 uint32_t 크기만큼 읽고, Memory 배열에 값을 리틀엔디안 식으로 할당.
        mem[mem_idx++] = (temp >> 24) & 0xFF; // 상위 8비트
        mem[mem_idx++] = (temp >> 16) & 0xFF; // 다음 8비트
        mem[mem_idx++] = (temp >> 8) & 0xFF; // 다음 8비트
        mem[mem_idx++] = temp & 0xFF; // 하위 8비트
    }

    fclose(file);

    Regs regs;
    init_regs(&regs);
    datapath(&regs, mem);

    return 0;
}
