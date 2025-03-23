#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define buffer 15
#define whiteSpace " "

typedef struct {
    int str_len;
    char* instruction;
    char* opcode;
    char* dst;
    char* src1;
    char* src2;

    int rArr[9];  // int 배열로 수정
} calcVars;

// 레지스터 상태 출력 함수
void printRegisters(calcVars* A) {
    printf("레지스터 상태: ");
    for (int i = 0; i < 9; i++) {
        printf("r%d=0x%X(%d) ", i, A->rArr[i], A->rArr[i]);
    }
    printf("\n");
}

int hexToDec(char* inp) {
    int sum = 0;
    for (int i = 2; i < strlen(inp); i++) {
        if (inp[i] >= '0' && inp[i] <= '9') {
            sum = sum * 16 + (inp[i] - '0');
        } else if (inp[i] >= 'A' && inp[i] <= 'F') {
            sum = sum * 16 + (inp[i] - 'A' + 10);
        }
    }
    return sum;
}

void _arithmetic(calcVars* A) {
    char temp[5];
    strcpy(temp, A->dst);
    int d = temp[1] - '0';
    strcpy(temp, A->src1);
    int s1 = temp[1] - '0';
    strcpy(temp, A->src2);
    int s2 = temp[1] - '0';

    int num1 = A->rArr[s1];
    int num2 = A->rArr[s2];

    if (strcmp(A->opcode, "ADD") == 0) {
        A->rArr[d] = num1 + num2;
        printf("%s \t# r%d(%d) + r%d(%d) → r%d(%d)\n",
               A->instruction, s1, num1, s2, num2, d, A->rArr[d]);
    } else if (strcmp(A->opcode, "MUL") == 0) {
        A->rArr[d] = num1 * num2;
        printf("%s \t# r%d(%d) × r%d(%d) → r%d(%d)\n",
               A->instruction, s1, num1, s2, num2, d, A->rArr[d]);
    }
}

void _mov(calcVars* A) {
    int d = A->dst[1] - '0';
    int s1 = A->src1[1] - '0';
    A->rArr[d] = A->rArr[s1];
    printf("%s \t# r%d(%d) → r%d\n", A->instruction, s1, A->rArr[s1], d);
}

void _rst(calcVars* A) {
    for (int i = 0; i < 9; i++) {  // 범위 수정 (0~8)
        A->rArr[i] = 0;
    }
    printf("%s \t\t# 모든 레지스터 초기화\n", A->instruction);
}

void case2char(calcVars* A) {
    char temp[5];
    strcpy(temp, A->dst);
    int d = temp[1] - '0';

    if (strcmp(A->opcode, "SW") == 0) {
        printf("%s \t# r%d → STDOUT(%d)\n", A->instruction, d, A->rArr[d]);
        printf("출력: %d\n", A->rArr[d]);  // 실제 값 출력
    } else if (strcmp(A->opcode, "LW") == 0) {
        int src = hexToDec(A->src1);
        A->rArr[d] = src;
        printf("%s \t# 0x%X → r%d\n", A->instruction, src, d);
    }
}

// ... (나머지 함수는 이전과 동일)

void _readInput(char* inLine, calcVars* inp) {
    inp->instruction = strdup(inLine);  // 입력 복사
    inp->instruction[strcspn(inp->instruction, "\n")] = 0;

    char* copy = strdup(inLine);  // 복사본 사용
    char* tokenPtr = strtok(copy, whiteSpace);
    int checkRST = 1;

    for (int counter = 1; tokenPtr != NULL; counter++) {
        switch (counter) {
            case 1:
                inp->opcode = strdup(tokenPtr);
                break;
            case 2:
                inp->dst = strdup(tokenPtr);
                checkRST = 0;
                break;
            case 3:
                inp->src1 = strdup(tokenPtr);
                break;
            case 4:
                inp->src2 = strdup(tokenPtr);
                break;
        }
        tokenPtr = strtok(NULL, whiteSpace);
    }
    free(copy);

    if (checkRST) {
        _rst(inp);
    } else {
        calculate(inp);
    }
    printRegisters(inp);  // 레지스터 상태 출력
}

int main() {
    FILE* fp = fopen("C:\\Users\\ldj23\\Desktop\\computer science\\input.txt", "r");
    if (!fp) exit(EXIT_FAILURE);

    calcVars* readInp = (calcVars*)malloc(sizeof(calcVars));
    memset(readInp, 0, sizeof(calcVars));  // 초기화

    char line[buffer];
    while (fgets(line, buffer, fp)) {
        _readInput(line, readInp);
    }

    fclose(fp);
    return 0;
}