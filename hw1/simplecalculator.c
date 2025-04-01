#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#define MAX_INSTRUCTIONS 1000
#define MAX_LINE_LENGTH  256
#define MAX_REG_INDEX    9

// 전역 레지스터: R0 ~ R9
int registers[10];
int comparison_value;

// 지원하는 명령어 리스트
const char* supported_opcodes[] = {"M", "+", "-", "*", "/", "C", "BEQ", "BNE", "H"};
const int num_supported_opcodes = 9;

// 레지스터 인덱스 유효성 검사
int get_reg_index(const char *reg) {
    int idx = atoi(reg + 1);
    if (idx < 0 || idx > MAX_REG_INDEX) {
        printf("Error: Register index %d out of bounds\n", idx);
        exit(1);
    }
    return idx;
}

// 0x.. -> 16진수 파싱, 아니면 10진수
int parse_value(const char *val_str) {
    char *endptr;
    long int num;

    if (val_str[0] == '0' && (val_str[1] == 'x' || val_str[1] == 'X')) {
        num = strtol(val_str, &endptr, 16);
        // strtol 이후 endptr가 문자열 끝을 가리키는지 검사하여 잘못된 입력 확인
        if (*endptr != '\0') {
            printf("Invalid hexadecimal number: %s\n", val_str);
            return 0;  // 잘못된 입력인 경우 0을 반환하거나 적절한 에러 처리를 할 수 있습니다.
        }
        return (int)num;
    } else {
        num = strtol(val_str, &endptr, 10);
        // strtol 이후 endptr가 문자열 끝을 가리키는지 검사하여 잘못된 입력 확인
        if (*endptr != '\0') {
            printf("Invalid decimal number: %s\n", val_str);
            return 0;  // 잘못된 입력인 경우 0을 반환하거나 적절한 에러 처리를 할 수 있습니다.
        }
        return (int)num;
    }
}

// 명령어 검사 함수
bool is_supported_opcode(const char* opcode) {
    for (int i = 0; i < num_supported_opcodes; i++) {
        if (strcmp(opcode, supported_opcodes[i]) == 0) {
            return true;
        }
    }
    return false;
}

// 레지스터 상태를 모두 출력하는 함수
void print_registers() {
    printf("Registers: ");
    for (int i = 0; i < 10; i++) {
        printf("R%d=%d ", i, registers[i]);
    }
    // 비교값도 표시
    printf("(comparison_value=%d)\n", comparison_value);
}

void execute_instruction(const char *instruction, int *pc, int current_line) {
    // current_line 은 "몇 번째 줄을 실행 중인지"를 표시하기 위해 받음
    // (pc는 실행 중에 변경될 수 있음)
    // 버퍼를 만들어서 strtok로 파싱
    char buffer[MAX_LINE_LENGTH];
    strcpy(buffer, instruction);

    char *opcode = strtok(buffer, " \t");
    if (!opcode) {
        (*pc)++;
        return;
    }

    // 지원하지 않는 명령어 처리
    if (!is_supported_opcode(opcode)) {
        printf("Error: Unknown opcode '%s' at line %d\n", opcode, current_line);
        (*pc)++;  // 다음 명령어로 넘어갑니다.
        return;
    }

    char *operand1 = strtok(NULL, " \t");
    char *operand2 = strtok(NULL, " \t");

    if (strcmp(opcode, "M") == 0) {
        if (operand1 && operand2) {
            int dest_idx = get_reg_index(operand1);
            if (operand2[0] == 'R' || operand2[0] == 'r') {
                int src_idx = get_reg_index(operand2);
                registers[dest_idx] = registers[src_idx];
            } else {
                registers[dest_idx] = parse_value(operand2);
            }
        }
        (*pc)++;
    }
    else if (strcmp(opcode, "+") == 0) {
        int val1 = (operand1[0] == 'R' || operand1[0] == 'r') ? registers[get_reg_index(operand1)] : parse_value(operand1);
        int val2 = (operand2[0] == 'R' || operand2[0] == 'r') ? registers[get_reg_index(operand2)] : parse_value(operand2);
        registers[0] = val1 + val2;
        (*pc)++;
    }
    else if (strcmp(opcode, "-") == 0) {
        int val1 = (operand1[0] == 'R' || operand1[0] == 'r') ? registers[get_reg_index(operand1)] : parse_value(operand1);
        int val2 = (operand2[0] == 'R' || operand2[0] == 'r') ? registers[get_reg_index(operand2)] : parse_value(operand2);
        registers[0] = val1 - val2;
        (*pc)++;
    }
    else if (strcmp(opcode, "*") == 0) {
        int val1 = (operand1[0] == 'R' || operand1[0] == 'r') ? registers[get_reg_index(operand1)] : parse_value(operand1);
        int val2 = (operand2[0] == 'R' || operand2[0] == 'r') ? registers[get_reg_index(operand2)] : parse_value(operand2);
        registers[0] = val1 * val2;
        (*pc)++;
    }
    else if (strcmp(opcode, "/") == 0) {
        int val1 = (operand1[0] == 'R' || operand1[0] == 'r') ? registers[get_reg_index(operand1)] : parse_value(operand1);
        int val2 = (operand2[0] == 'R' || operand2[0] == 'r') ? registers[get_reg_index(operand2)] : parse_value(operand2);
        if (val2 == 0) {
            printf("Runtime Error: Divide by zero at line %d\n", current_line);
            exit(1);
        }
        registers[0] = val1 / val2;
        (*pc)++;
    }
    else if (strcmp(opcode, "C") == 0) {
        // C R1 R2 => comparison_value 설정
        if (operand1 && operand2) {
            int val1 = registers[get_reg_index(operand1)];
            int val2 = registers[get_reg_index(operand2)];
            if (val1 == val2) comparison_value = 0;
            else if (val1 < val2) comparison_value = -1;
            else comparison_value = 1;
        }
        (*pc)++;
    }
    else if (strcmp(opcode, "BEQ") == 0) {
        // BEQ n => if (comparison_value == 0) => pc = n-1
        if (operand1) {
            int jump_line = parse_value(operand1) - 1;
            if (comparison_value == 0) {
                (*pc) = jump_line;
            } else {
                (*pc)++;
            }
        } else {
            (*pc)++;
        }
    }
    else if (strcmp(opcode, "BNE") == 0) {
        // BNE n => if (comparison_value != 0) => pc = n-1
        if (operand1) {
            int jump_line = parse_value(operand1) - 1;
            if (comparison_value != 0) {
                (*pc) = jump_line;
            } else {
                (*pc)++;
            }
        } else {
            (*pc)++;
        }
    }
    else if (strcmp(opcode, "H") == 0){
        printf("프로그램 종료 (조건 H 만족)");
        printf("\n=== 프로그램 종료 후 최종 레지스터 상태 ===\n");
        print_registers();
        exit(0);
    }
    else {
        // 알 수 없는 명령어
        printf("Unknown opcode: %s (line %d)\n", opcode, current_line);
        (*pc)++;
    }

    printf("\nline %d: %s\n", current_line, instruction);
    print_registers();
    printf("\n");
}

int main(void) {
    // 레지스터 초기화
    for (int i = 0; i < 10; i++) {
        registers[i] = 0;
    }
    comparison_value = 0;

    // 명령어 저장
    char instructions[MAX_INSTRUCTIONS][MAX_LINE_LENGTH];
    int instruction_count = 0;

    FILE *fp = fopen("C:\\Users\\ldj23\\Desktop\\computer science\\hw1\\gcd1.txt", "r");
    if (!fp) {
        perror("파일 열기 실패");
        return 1;
    }

    // 파일에서 한 줄씩 읽어 valid 명령만 저장
    while (!feof(fp)) {
        char line[MAX_LINE_LENGTH];
        if (!fgets(line, sizeof(line), fp)) break;

        // 앞뒤 공백 제거
        char *p = line;
        while (*p && isspace((unsigned char)*p)) p++;
        char *end = p + strlen(p) - 1;
        while (end > p && isspace((unsigned char)*end)) {
            *end = '\0';
            end--;
        }

        // 빈줄 또는 주석(;)이면 무시
        if (*p == ';' || *p == '\0') {
            continue;
        }

        // 명령어 저장
        strncpy(instructions[instruction_count], p, MAX_LINE_LENGTH - 1);
        instructions[instruction_count][MAX_LINE_LENGTH - 1] = '\0';
        instruction_count++;

        if (instruction_count >= MAX_INSTRUCTIONS) {
            printf("명령어가 너무 많습니다.\n");
            break;
        }
    }
    fclose(fp);

    // PC (Program Counter)
    int pc = 0;

    // 실행 루프
    while (pc >= 0 && pc < instruction_count) {
        // 파일 상 "줄 번호"를 보여주기 위해 (pc+1)을 current_line으로 쓸 수 있음
        int current_line = pc + 1; 
        execute_instruction(instructions[pc], &pc, current_line);
    }
    return 0;
}