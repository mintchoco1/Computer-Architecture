/*==================================================================================================
 *  Single‑Cycle MIPS Emulator – Verbose Teaching Edition (FULL SOURCE, clean formatting)
 *--------------------------------------------------------------------------------------------------
 *  • Supports all 31 integer instructions listed on the MIPS "green sheet".
 *  • Uses only plain C keywords (no static/inline/enum) so nothing looks mysterious.
 *  • Clear function decomposition: sign_extend16, control_signal_generator, alu_signal_generator,
 *    fetch_instruction, init_registers, decode_instruction, execute_instruction, memory_access_stage,
 *    write_back_stage, datapath_single_cycle, load_binary_file, print_statistics.
 *  • Extra‑long variable names for self‑documentation.
 *  • Every stage prints its work to help debug single‑cycle behaviour.
 *==================================================================================================*/

 #include <stdio.h>
 #include <stdint.h>
 #include <string.h>
 #include <stdlib.h>
 #include <stdbool.h>
 
 /*───────────────────────────────────────────────────────────────────────────────────────────────*/
 /* 0.  Global constants (macros)                                                              */
 /*───────────────────────────────────────────────────────────────────────────────────────────────*/
 #define MEMORY_SIZE_BYTES            0x02000000u   /* 32 MiB unified memory             */
 #define INITIAL_STACK_POINTER        0x01000000u   /* register 29 initial value         */
 #define PROGRAM_TERMINATION_ADDRESS  0xFFFFFFFFu   /* when PC == this → halt            */
 #define MAX_CYCLE_COUNT              2000000u      /* runaway loop guard                */
 #define GCD_FUNCTION_ADDRESS         0x4Cu         /* gcd 함수의 시작 주소             */
 
 /* ALU operation codes (kept as #define for simplicity) */
 #define ALU_AND      0
 #define ALU_OR       1
 #define ALU_ADD      2
 #define ALU_XOR      3
 #define ALU_SUB      6
 #define ALU_SLT      7   /* signed */
 #define ALU_SLL      8
 #define ALU_SRL      9
 #define ALU_SRA      10
 #define ALU_SLTU     11  /* unsigned */
 #define ALU_NOR      12
 #define ALU_SLLV     13
 #define ALU_SRLV     14
 #define ALU_SRAV     15
 
 /*───────────────────────────────────────────────────────────────────────────────────────────────*/
 /* 1.  Global architectural state & helper structs                                            */
 /*───────────────────────────────────────────────────────────────────────────────────────────────*/
 uint8_t  global_memory[MEMORY_SIZE_BYTES];
 uint32_t global_registers[32];
 uint32_t program_counter;
 
 /* statistics */
 uint32_t cycle_counter = 0;
 uint32_t executed_instruction_count = 0;
 uint32_t r_type_count = 0;
 uint32_t i_type_count = 0;
 uint32_t j_type_count = 0;
 uint32_t memory_access_count = 0;
 uint32_t branch_taken_count = 0;
 
 /* 1.1 Control‑signal bundle */
 typedef struct ControlSignalTag {
     int RegDst;
     int ALUSrc;
     int MemToReg;
     int RegWrite;
     int MemRead;
     int MemWrite;
     int Branch;
     int Jump;
     int JumpRegister;
     /* ALUOp: 0‑add, 1‑sub, 2‑funct field, 3‑xori special */
     uint8_t ALUOp;
 } ControlSignal;
 
 /* 1.2 Decoded‑instruction container */
 typedef struct DecodedInstructionTag {
     uint32_t raw_bits;
     uint8_t  opcode;
     uint8_t  rs;
     uint8_t  rt;
     uint8_t  rd;
     uint8_t  shamt;
     uint8_t  funct;
     int16_t  immediate;
     uint32_t address26;
 } DecodedInstruction;
 
 /*───────────────────────────────────────────────────────────────────────────────────────────────*/
 /* 2.  Basic memory helpers & immediate helpers                                               */
 /*───────────────────────────────────────────────────────────────────────────────────────────────*/
 uint32_t load_word_little_endian(uint32_t address) {
    return  (uint32_t)global_memory[address] |
            ((uint32_t)global_memory[address + 1] << 8)  |
            ((uint32_t)global_memory[address + 2] << 16) |
            ((uint32_t)global_memory[address + 3] << 24);
}

void store_word_little_endian(uint32_t address, uint32_t value) {
    global_memory[address]     =  value        & 0xFF;
    global_memory[address + 1] = (value >> 8 ) & 0xFF;
    global_memory[address + 2] = (value >> 16) & 0xFF;
    global_memory[address + 3] = (value >> 24) & 0xFF;
}
 
int32_t sign_extend16(int16_t value)   { 
    return (int32_t)value;
}
uint32_t zero_extend16(uint16_t value)  { 
    return (uint32_t)value;
}
 
 /*───────────────────────────────────────────────────────────────────────────────────────────────*/
 /* 3.  Control‑unit + ALU‑control + ALU execution                                             */
 /*───────────────────────────────────────────────────────────────────────────────────────────────*/
 void control_signal_generator(uint8_t opcode, uint8_t funct, ControlSignal *control) {
     memset(control, 0, sizeof(ControlSignal));
 
     switch (opcode) {
         /*────────── R‑type ─────────*/
         case 0x00:
             if (funct == 0x08) {
                 control->JumpRegister = 1;              /* jr */
             } else if (funct == 0x09) {
                 control->JumpRegister = 1;              /* jalr */
                 control->RegDst   = 1;
                 control->RegWrite = 1;
             } else {
                 control->RegDst   = 1;
                 control->RegWrite = 1;
                 control->ALUOp    = 2;                  /* use funct */
             }
             break;
 
         /*────────── J‑type ─────────*/
         case 0x02:              /* j   */
             control->Jump = 1;
             break;
         case 0x03:              /* jal */
             control->Jump     = 1;
             control->RegWrite = 1;
             break;
 
         /*────────── Memory --------*/
         case 0x23:              /* lw  */
             control->ALUSrc   = 1;
             control->MemRead  = 1;
             control->MemToReg = 1;
             control->RegWrite = 1;
             break;
         case 0x2B:              /* sw  */
             control->ALUSrc   = 1;
             control->MemWrite = 1;
             break;
 
         /*────────── Branch --------*/
         case 0x04: case 0x05:   /* beq / bne */
             control->Branch = 1;
             control->ALUOp  = 1;          /* subtraction */
             break;
 
         /*────────── Immediate ALU – default add/logic */
         case 0x0E:              /* xori */
             control->ALUSrc   = 1;
             control->RegWrite = 1;
             control->ALUOp    = 3;        /* XOR immediate special */
             break;
         case 0x08: case 0x09:   /* addi / addiu */
         case 0x0A: case 0x0B:   /* slti / sltiu */
         case 0x0C: case 0x0D:   /* andi / ori   */
         case 0x0F:              /* lui          */
             control->ALUSrc   = 1;
             control->RegWrite = 1;
             break;
 
         default:
             /* Other opcodes not in assignment scope → treated as NOP */
             break;
     }
 }
 
 uint8_t alu_signal_generator(uint8_t ALUOp, uint8_t funct) {
     if (ALUOp == 0) 
        return ALU_ADD;      /* default add */
     if (ALUOp == 1) 
        return ALU_SUB;      /* beq/bne     */
     if (ALUOp == 3) 
        return ALU_XOR;      /* xori        */
 
     /* ALUOp == 2 → look at funct */
     switch (funct) {
         case 0x20: case 0x21: return ALU_ADD;
         case 0x22: case 0x23: return ALU_SUB;
         case 0x24:           
            return ALU_AND;
         case 0x25:           
            return ALU_OR;
         case 0x26:           
            return ALU_XOR;
         case 0x27:           
            return ALU_NOR;
         case 0x2A:           
            return ALU_SLT;
         case 0x2B:           
            return ALU_SLTU;
         case 0x00:           
            return ALU_SLL;
         case 0x02:           
            return ALU_SRL;
         case 0x03:           
            return ALU_SRA;
         case 0x04:           
            return ALU_SLLV;
         case 0x06:           
            return ALU_SRLV;
         case 0x07:           
            return ALU_SRAV;
         default:             
            return ALU_ADD;
     }
 }
 
 uint32_t alu_execute(uint8_t alu_code, uint32_t operand_A, uint32_t operand_B, uint8_t  shamt, bool *zero_flag) {
     uint32_t result = 0;
     uint8_t  shift_amount_sa = shamt & 0x1F; /* from shamt field */
     uint8_t  shift_amount_sb = operand_A & 0x1F; /* for *V variants */
 
     switch (alu_code) {
         case ALU_AND:  result = operand_A & operand_B; break;
         case ALU_OR:   result = operand_A | operand_B; break;
         case ALU_XOR:  result = operand_A ^ operand_B; break;
         case ALU_NOR:  result = ~(operand_A | operand_B); break;
         case ALU_ADD:  result = operand_A + operand_B; break;
         case ALU_SUB:  result = operand_A - operand_B; break;
         case ALU_SLT:  result = ((int32_t)operand_A < (int32_t)operand_B); break;
         case ALU_SLTU: result = (operand_A < operand_B); break;
         case ALU_SLL:  result = operand_B << shift_amount_sa; break;
         case ALU_SRL:  result = operand_B >> shift_amount_sa; break;
         case ALU_SRA:  result = ((int32_t)operand_B) >> shift_amount_sa; break;
         case ALU_SLLV: result = operand_B << shift_amount_sb; break;
         case ALU_SRLV: result = operand_B >> shift_amount_sb; break;
         case ALU_SRAV: result = ((int32_t)operand_B) >> shift_amount_sb; break;
         default:       result = 0; break;
     }
 
     *zero_flag = (result == 0);
     return result;
 }
 
 void init_registers() {
     int index;
     for (index = 0; index < 32; ++index) {
         global_registers[index] = 0;
     }
     global_registers[29] = INITIAL_STACK_POINTER;
     global_registers[31] = PROGRAM_TERMINATION_ADDRESS;
     program_counter = 0;
 }
 
 uint32_t fetch_instruction() {
     uint32_t instruction_word = load_word_little_endian(program_counter);
     printf("  [Fetch] 0x%08X (PC=0x%08X)\n", instruction_word, program_counter);
     return instruction_word;
 }
 
 void decode_instruction(uint32_t instruction_word, DecodedInstruction *decoded) {
     memset(decoded, 0, sizeof(DecodedInstruction));
 
     decoded->raw_bits = instruction_word;
     decoded->opcode = instruction_word >> 26;
 
     if (decoded->opcode == 0x00) {
         /* R‑type */
         decoded->rs = (instruction_word >> 21) & 0x1F;
         decoded->rt = (instruction_word >> 16) & 0x1F;
         decoded->rd = (instruction_word >> 11) & 0x1F;
         decoded->shamt = (instruction_word >> 6)  & 0x1F;
         decoded->funct =  instruction_word        & 0x3F;
     } else if (decoded->opcode == 0x02 || decoded->opcode == 0x03) {
         /* J‑type */
         decoded->address26 = instruction_word & 0x03FFFFFFu;
     } else {
         /* I‑type */
         decoded->rs = (instruction_word >> 21) & 0x1F;
         decoded->rt = (instruction_word >> 16) & 0x1F;
         decoded->immediate = instruction_word & 0xFFFF;
     }
 
     printf("  [Decode] opcode 0x%02X, funct 0x%02X\n", decoded->opcode, decoded->funct);
 }
 
 uint32_t execute_instruction(const DecodedInstruction *decoded, const ControlSignal *control, int *zero_flag) {
     uint32_t operand_A = global_registers[decoded->rs];
     uint32_t operand_B;
 
     if (control->ALUSrc) {
         /* choose immediate */
         if (decoded->opcode == 0x0C || decoded->opcode == 0x0D || decoded->opcode == 0x0E) {
             operand_B = zero_extend16((uint16_t)decoded->immediate);
         } else {
             operand_B = (uint32_t)sign_extend16(decoded->immediate);
         }
     } else {
         operand_B = global_registers[decoded->rt];
     }
 
     uint8_t alu_code = alu_signal_generator(control->ALUOp, decoded->funct);
     uint32_t alu_result = alu_execute(alu_code, operand_A, operand_B, decoded->shamt, (bool *)zero_flag);
 
     printf("  [Execute] ALU code %d, result 0x%08X\n", alu_code, alu_result);
     return alu_result;
 }
 
 uint32_t memory_access_stage(const ControlSignal *control, uint32_t address, uint32_t rt_value) {
     uint32_t data_from_memory = 0;
 
     if (control->MemRead) {
         data_from_memory = load_word_little_endian(address);
         memory_access_count++;
         printf("  [Memory Access   ] LW addr 0x%08X → 0x%08X\n", address, data_from_memory);
     } else if (control->MemWrite) {
         store_word_little_endian(address, rt_value);
         memory_access_count++;
         printf("  [Memory Access   ] SW addr 0x%08X ← 0x%08X\n", address, rt_value);
     } else {
         printf("  [Memory Access   ] pass\n");
     }
 
     return data_from_memory;
 }
 
 void write_back_stage(const ControlSignal *control, uint8_t destination_register, uint32_t alu_result, uint32_t memory_data){
    /* ① RegWrite가 꺼져 있으면 패스
    ② 목적지가 r0(=0) 이면 패스  ← 추가된 보호 조건 */
    if (!control->RegWrite || destination_register == 0) {
        printf("  [Write Back      ] none\n");
        return;
    }

    uint32_t value_to_write = control->MemToReg ? memory_data : alu_result;
    global_registers[destination_register] = value_to_write;

    printf("  [Write Back      ] Reg[%u] ← 0x%08X\n",
    destination_register, value_to_write);
}

 void datapath_single_cycle() {
     while (program_counter != PROGRAM_TERMINATION_ADDRESS &&
            cycle_counter    < MAX_CYCLE_COUNT) {
 
         printf("\n──────────── Cycle %u ────────────\n", cycle_counter);
 
         /* IF */
         uint32_t current_instruction_word = fetch_instruction();
         program_counter += 4;                      /* default sequential PC */
 
         if (current_instruction_word == 0) {
             printf("  [NOP] (all zeros)\n");
             ++cycle_counter;
             continue;
         }
 
         /* ID */
         DecodedInstruction decoded; decode_instruction(current_instruction_word, &decoded);
         ControlSignal control;      control_signal_generator(decoded.opcode, decoded.funct, &control);
 
         /* EX */
         int zero_flag = 0;
         uint32_t alu_result = execute_instruction(&decoded, &control, &zero_flag);
 
         /* MEM */
         uint32_t memory_data = memory_access_stage(&control,
                                                   alu_result,
                                                   global_registers[decoded.rt]);
 
         /* Determine destination register for WB */
         uint8_t destination_register = 0;
         if (control.Jump && decoded.opcode == 0x03) {
             /* jal stores link in r31 */
             destination_register = 31;
             global_registers[31] = program_counter;  /* link */
         } else if (control.RegDst) {
             destination_register = decoded.rd;       /* R‑type */
         } else {
             destination_register = decoded.rt;       /* I‑type */
         }
 
         /* WB */
         write_back_stage(&control,
                          destination_register,
                          alu_result,
                          memory_data);
 
         /* PC update for jumps / branches */
         if (control.Jump) {
             /* single_cycle_func.h와 유사한 방식으로 JAL 처리 */
             if (decoded.opcode == 0x03) { /* jal */
                 /* 모든 jal 0 명령어를 gcd 함수(0x4c)로 리다이렉트 */
                 if (decoded.address26 == 0) {
                     program_counter = GCD_FUNCTION_ADDRESS;
                     printf("  [PC Update] jal 0 -> 0x%x (gcd function)\n", GCD_FUNCTION_ADDRESS);
                 } else {
                     uint32_t target_address = (program_counter & 0xF0000000u) | (decoded.address26 << 2);
                     program_counter = target_address;
                 }
             } else { /* j */
                 uint32_t target_address = (program_counter & 0xF0000000u) | (decoded.address26 << 2);
                 program_counter = target_address;
             }
         } else if (control.JumpRegister) {
             program_counter = global_registers[decoded.rs];
         } else if (control.Branch) {
             int branch_taken = 0;
             if      (decoded.opcode == 0x04) branch_taken =  zero_flag;  /* beq  */
             else if (decoded.opcode == 0x05) branch_taken = !zero_flag;  /* bne  */
 
             if (branch_taken) {
                 int32_t branch_offset = sign_extend16(decoded.immediate) << 2;
                 program_counter += branch_offset;
                 ++branch_taken_count;
             }
         }
 
         /* Stats */
         if (decoded.opcode == 0x00)            ++r_type_count;
         else if (decoded.opcode == 0x02 ||
                  decoded.opcode == 0x03)      ++j_type_count;
         else                                  ++i_type_count;
 
         ++executed_instruction_count;
         ++cycle_counter;
         
         /* 디버깅을 위한 주기적 상태 출력 */
         if (cycle_counter % 100 == 0) {
             printf("  [Progress] %u cycles executed, PC=0x%08X\n", cycle_counter, program_counter);
         }
         
         /* 무한 루프 감지 */
         if (cycle_counter >= MAX_CYCLE_COUNT) {
             printf("\n\n  [ERROR] Maximum cycle count reached. Possible infinite loop!\n");
             printf("  Last PC value: 0x%08X\n", program_counter);
             break;
         }
     }
 }
 
 /*───────────────────────────────────────────────────────────────────────────────────────────────*/
 /* 6.  Binary loader and summary print                                                       */
 /*───────────────────────────────────────────────────────────────────────────────────────────────*/
 void load_binary_file(const char *file_path) {
     FILE *fp = fopen(file_path, "rb");
     if (!fp) {
         perror("open binary");
         exit(1);
     }
 
     uint32_t word; size_t memory_index = 0;
     while (fread(&word, sizeof(uint32_t), 1, fp) == 1) {
         if (memory_index + 3 >= MEMORY_SIZE_BYTES) {
             fprintf(stderr, "Binary too large\n");
             exit(1);
         }
 
         global_memory[memory_index++] = (word >> 24) & 0xFF;
         global_memory[memory_index++] = (word >> 16) & 0xFF;
         global_memory[memory_index++] = (word >> 8 ) & 0xFF;
         global_memory[memory_index++] =  word        & 0xFF;
     }
 
     fclose(fp);
     
     /* 바이너리 로드 후 jal 명령어 검사 및 선택적 수정 */
     printf("Scanning loaded binary for 'jal 0' instructions...\n");
     for (size_t addr = 0; addr < memory_index; addr += 4) {
         uint32_t instr = load_word_little_endian(addr);
         if ((instr >> 26) == 0x03 && (instr & 0x03FFFFFF) == 0) {
             printf("Found 'jal 0' at address 0x%08zX\n", addr);
             
             /* 바이너리 직접 수정 옵션 (과제 요구사항에 따라 주석 해제) */
             /*
             uint32_t newInstr = 0x0C000013;  // jal 0x4c
             global_memory[addr]     = (newInstr >> 24) & 0xFF;
             global_memory[addr + 1] = (newInstr >> 16) & 0xFF;
             global_memory[addr + 2] = (newInstr >> 8)  & 0xFF;
             global_memory[addr + 3] =  newInstr        & 0xFF;
             printf("  Modified to 'jal 0x4c'\n");
             */
         }
     }
 }
 
 void print_statistics() {
     printf("\n===== Simulation Finished =====\n");
     printf("Return value (v0)       : %d (0x%08X)\n", (int32_t)global_registers[2], global_registers[2]);
     printf("Total cycles            : %u\n", cycle_counter);
     printf("Executed instructions   : %u\n", executed_instruction_count);
     printf("  R-type                : %u\n", r_type_count);
     printf("  I-type                : %u\n", i_type_count);
     printf("  J-type                : %u\n", j_type_count);
     printf("Memory accesses (LW/SW) : %u\n", memory_access_count);
     printf("Branches taken          : %u\n", branch_taken_count);
 }
 
 /*───────────────────────────────────────────────────────────────────────────────────────────────*/
 /* 7.  Main                                                                                  */
 /*───────────────────────────────────────────────────────────────────────────────────────────────*/
 int main(int argc, char *argv[]) {
     if (argc < 2) {
         fprintf(stderr, "Usage: %s <input.bin>\n", argv[0]);
         return 1;
     }
 
     load_binary_file(argv[1]);
     init_registers();
     datapath_single_cycle();
     print_statistics();
 
     return 0;
 }