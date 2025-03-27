#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_INSTRUCTIONS 1000
#define MAX_LINE_LENGTH  256

// 전역 레지스터: R0 ~ R9
int registers[10];
int comparison_value;
//  comparison_value ==  0 => (operand1 == operand2)
//  comparison_value == -1 => (operand1 <  operand2)
//  comparison_value == +1 => (operand1 >  operand2)

//exception check
//unknown 오퍼레이터 f % 
//oberation format 0xz A B 
//operation input boundary check 0x99999999999
//register boundary r[99999]
//포인터 에러


// "R2"등 -> 정수(2)
int get_reg_index(const char *reg) {
    return atoi(reg + 1);
}

// 0x.. -> 16진수 파싱, 아니면 10진수
int parse_value(const char *val_str) {
    if ((val_str[0] == '0' && (val_str[1] == 'x' || val_str[1] == 'X'))) {
        return (int)strtol(val_str, NULL, 16);
    } 
    else {
        return atoi(val_str);
    }
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

// 한 줄 명령어 실행 (실행 후 레지스터 상태 출력)
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

    char *op1 = strtok(NULL, " \t");
    char *op2 = strtok(NULL, " \t");

    // 실제 명령어 처리
    if (strcmp(opcode, "M") == 0) {
        // M R1 0x64 => registers[R1] = 100
        if (op1 && op2) {
            int dest_idx = get_reg_index(op1);
            if (op2[0] == 'R' || op2[0] == 'r') {
                int src_idx = get_reg_index(op2);
                registers[dest_idx] = registers[src_idx];
            } else {
                registers[dest_idx] = parse_value(op2);
            }
        }
        (*pc)++;
    }
    else if (strcmp(opcode, "+") == 0) {
        // + R1 R2 => R0 = R1 + R2
        if (op1 && op2) {
            int r1 = get_reg_index(op1);
            int r2 = get_reg_index(op2);
            registers[0] = registers[r1] + registers[r2];
        }
        (*pc)++;
    }
    else if (strcmp(opcode, "-") == 0) {
        // - R1 R2 => R0 = R1 - R2
        if (op1 && op2) {
            int r1 = get_reg_index(op1);
            int r2 = get_reg_index(op2);
            registers[0] = registers[r1] - registers[r2];
        }
        (*pc)++;
    }
    else if (strcmp(opcode, "*") == 0) {
        // * R1 R2 => R0 = R1 * R2
        if (op1 && op2) {
            int r1 = get_reg_index(op1);
            int r2 = get_reg_index(op2);
            registers[0] = registers[r1] * registers[r2];
        }
        (*pc)++;
    }
    else if (strcmp(opcode, "/") == 0) {
        // / R1 R2 => R0 = R1 / R2
        if (op1 && op2) {
            int r1 = get_reg_index(op1);
            int r2 = get_reg_index(op2);
            if (registers[r2] == 0) {
                printf("Runtime Error: Divide by zero at line %d\n", current_line);
                exit(1);
            }
            registers[0] = registers[r1] / registers[r2];
        }
        (*pc)++;
    }
    else if (strcmp(opcode, "C") == 0) {
        // C R1 R2 => comparison_value 설정
        if (op1 && op2) {
            int val1 = registers[get_reg_index(op1)];
            int val2 = registers[get_reg_index(op2)];
            if (val1 == val2) comparison_value = 0;
            else if (val1 < val2) comparison_value = -1;
            else comparison_value = 1;
        }
        (*pc)++;
    }
    else if (strcmp(opcode, "BEQ") == 0) {
        // BEQ n => if (comparison_value == 0) => pc = n-1
        if (op1) {
            int jump_line = parse_value(op1) - 1;
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
        if (op1) {
            int jump_line = parse_value(op1) - 1;
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

    // --- 여기서 명령어 실행 직후, 레지스터 상태를 출력 ---
    // current_line+1 은 다음 줄이 아니라 "현재 실행한 줄의 번호"로 보이고 싶다면 current_line 그대로 사용.
    // (파일 내 실제 줄번호와 pc+1의 차이가 있을 수 있지만, 대략 맞춰서 표현)
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

    FILE *fp = fopen("C:\\Users\\ldj23\\Desktop\\computer science\\hw1\\gcd.txt", "r");
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
        // 실제 실행할 명령어
        // 파일 상 "줄 번호"를 보여주기 위해 (pc+1)을 current_line으로 쓸 수 있음
        int current_line = pc + 1; 
        execute_instruction(instructions[pc], &pc, current_line);
    }
    return 0;
}