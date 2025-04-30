/*==================================================================================================
 *  Single‑Cycle MIPS Emulator – Verbose Teaching Edition (FIXED VERSION)
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
#define MEMORY_SIZE_BYTES 0x01000000u           /* 16 MiB unified memory             */
#define INITIAL_STACK_POINTER 0x01000000u       /* register 29 initial value         */
#define PROGRAM_TERMINATION_ADDRESS 0xFFFFFFFFu /* when PC == this → halt            */
#define MAX_CYCLE_COUNT 2000000u                /* runaway loop guard                */

/* ALU operation codes (kept as #define for simplicity) */
#define ALU_AND 0
#define ALU_OR 1
#define ALU_ADD 2
#define ALU_XOR 3
#define ALU_SUB 6
#define ALU_SLT 7 /* signed */
#define ALU_SLL 8
#define ALU_SRL 9
#define ALU_SRA 10
#define ALU_SLTU 11 /* unsigned */
#define ALU_NOR 12
#define ALU_SLLV 13
#define ALU_SRLV 14
#define ALU_SRAV 15

/* MIPS instruction code definitions */
#define OPCODE_RTYPE 0x00
#define OPCODE_J 0x02
#define OPCODE_JAL 0x03
#define OPCODE_BEQ 0x04
#define OPCODE_BNE 0x05
#define OPCODE_ADDI 0x08
#define OPCODE_ADDIU 0x09
#define OPCODE_SLTI 0x0A
#define OPCODE_SLTIU 0x0B
#define OPCODE_ANDI 0x0C
#define OPCODE_ORI 0x0D
#define OPCODE_XORI 0x0E
#define OPCODE_LUI 0x0F
#define OPCODE_LW 0x23
#define OPCODE_SW 0x2B

/* MIPS R-type function code definitions */
#define FUNCT_SLL 0x00
#define FUNCT_SRL 0x02
#define FUNCT_SRA 0x03
#define FUNCT_SLLV 0x04
#define FUNCT_SRLV 0x06
#define FUNCT_SRAV 0x07
#define FUNCT_JR 0x08
#define FUNCT_JALR 0x09
#define FUNCT_ADD 0x20
#define FUNCT_ADDU 0x21
#define FUNCT_SUB 0x22
#define FUNCT_SUBU 0x23
#define FUNCT_AND 0x24
#define FUNCT_OR 0x25
#define FUNCT_XOR 0x26
#define FUNCT_NOR 0x27
#define FUNCT_SLT 0x2A
#define FUNCT_SLTU 0x2B

/*───────────────────────────────────────────────────────────────────────────────────────────────*/
/* 1.  Global architectural state & helper structs                                            */
/*───────────────────────────────────────────────────────────────────────────────────────────────*/
uint8_t global_memory[MEMORY_SIZE_BYTES];
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
typedef struct ControlSignalTag
{
    int RegDst;
    int ALUSrc;
    int MemToReg;
    int RegWrite;
    int MemRead;
    int MemWrite;
    int Branch;
    int Jump;
    int JumpRegister;
    uint8_t ALUOp;
} ControlSignal;

/* 1.2 Decoded‑instruction container */
typedef struct DecodedInstructionTag
{
    uint32_t raw_bits;
    uint8_t opcode;
    uint8_t rs;
    uint8_t rt;
    uint8_t rd;
    uint8_t shamt;
    uint8_t funct;
    int16_t immediate;
    uint32_t address26;
} DecodedInstruction;

uint32_t load_word_little_endian(uint32_t address)
{
    if (address >= MEMORY_SIZE_BYTES - 3)
    {
        printf("  [ERROR] Memory access out of bounds at address 0x%08X\n", address);
        return 0;
    }

    /* Correct little-endian implementation */
    return (uint32_t)global_memory[address] |
           ((uint32_t)global_memory[address + 1] << 8) |
           ((uint32_t)global_memory[address + 2] << 16) |
           ((uint32_t)global_memory[address + 3] << 24);
}

void store_word_little_endian(uint32_t address, uint32_t value)
{
    if (address >= MEMORY_SIZE_BYTES - 3)
    {
        printf("  [ERROR] Memory access out of bounds at address 0x%08X\n", address);
        return;
    }
    global_memory[address] = value & 0xFF;
    global_memory[address + 1] = (value >> 8) & 0xFF;
    global_memory[address + 2] = (value >> 16) & 0xFF;
    global_memory[address + 3] = (value >> 24) & 0xFF;
}

int32_t sign_extend16(int16_t value)
{
    return (int32_t)value;
}

uint32_t zero_extend16(uint16_t value)
{
    return (uint32_t)value & 0x0000FFFF;
}

// Jump address calculation function
uint32_t calculate_jump_address(uint32_t pc, uint32_t address26)
{
    /* Fixed calculation for jump address */
    return ((pc & 0xF0000000) | (address26 << 2));
}

// Branch address calculation function
uint32_t calculate_branch_address(uint32_t pc, int16_t immediate)
{
    /* PC+4 is implicit in this calculation */
    return pc + 4 + (sign_extend16(immediate) << 2);
}

void control_signal_generator(uint8_t opcode, uint8_t funct, ControlSignal *control)
{
    // Initialize all control signals
    memset(control, 0, sizeof(ControlSignal));

    switch (opcode)
    {
    /*────────── R‑type ─────────*/
    case OPCODE_RTYPE:
        if (funct == FUNCT_JR)
        {
            control->JumpRegister = 1; /* jr */
        }
        else if (funct == FUNCT_JALR)
        {
            control->JumpRegister = 1; /* jalr */
            control->RegDst = 1;
            control->RegWrite = 1;
        }
        else
        {
            control->RegDst = 1;
            control->RegWrite = 1;
            control->ALUOp = 2; /* use funct */
        }
        break;

    /*────────── J‑type ─────────*/
    case OPCODE_J: /* j   */
        control->Jump = 1;
        break;
    case OPCODE_JAL: /* jal */
        control->Jump = 1;
        control->RegWrite = 1;
        break;

    /*────────── Memory --------*/
    case OPCODE_LW: /* lw  */
        control->ALUSrc = 1;
        control->MemRead = 1;
        control->MemToReg = 1;
        control->RegWrite = 1;
        break;
    case OPCODE_SW: /* sw  */
        control->ALUSrc = 1;
        control->MemWrite = 1;
        break;

    /*────────── Branch --------*/
    case OPCODE_BEQ:
    case OPCODE_BNE: /* beq / bne */
        control->Branch = 1;
        control->ALUOp = 1; /* subtraction */
        break;

    /*────────── Immediate ALU – default add/logic */
    case OPCODE_XORI: /* xori */
        control->ALUSrc = 1;
        control->RegWrite = 1;
        control->ALUOp = 3; /* XOR immediate special */
        break;
    case OPCODE_ADDI:
    case OPCODE_ADDIU: /* addi / addiu */
    case OPCODE_SLTI:
    case OPCODE_SLTIU: /* slti / sltiu */
    case OPCODE_ANDI:
    case OPCODE_ORI: /* andi / ori   */
    case OPCODE_LUI: /* lui          */
        control->ALUSrc = 1;
        control->RegWrite = 1;
        break;

    default:
        /* Unknown instruction - treat as NOP */
        printf("  [WARNING] Unknown opcode 0x%02X - treated as NOP\n", opcode);
        break;
    }
}

uint8_t alu_signal_generator(uint8_t ALUOp, uint8_t funct)
{
    if (ALUOp == 0)
        return ALU_ADD; /* default add */
    if (ALUOp == 1)
        return ALU_SUB; /* beq/bne     */
    if (ALUOp == 3)
        return ALU_XOR; /* xori        */

    /* ALUOp == 2 → look at funct */
    switch (funct)
    {
    case FUNCT_ADD:
    case FUNCT_ADDU:
        return ALU_ADD;
    case FUNCT_SUB:
    case FUNCT_SUBU:
        return ALU_SUB;
    case FUNCT_AND:
        return ALU_AND;
    case FUNCT_OR:
        return ALU_OR;
    case FUNCT_XOR:
        return ALU_XOR;
    case FUNCT_NOR:
        return ALU_NOR;
    case FUNCT_SLT:
        return ALU_SLT;
    case FUNCT_SLTU:
        return ALU_SLTU;
    case FUNCT_SLL:
        return ALU_SLL;
    case FUNCT_SRL:
        return ALU_SRL;
    case FUNCT_SRA:
        return ALU_SRA;
    case FUNCT_SLLV:
        return ALU_SLLV;
    case FUNCT_SRLV:
        return ALU_SRLV;
    case FUNCT_SRAV:
        return ALU_SRAV;
    default:
        printf("  [WARNING] Unknown function code 0x%02X for R-type - using ADD\n", funct);
        return ALU_ADD;
    }
}

uint32_t alu_execute(uint8_t alu_code, uint32_t operand_A, uint32_t operand_B, uint8_t shamt, bool *zero_flag)
{
    uint32_t result = 0;
    uint8_t shift_amount_sa = shamt & 0x1F;     /* from shamt field */
    uint8_t shift_amount_sb = operand_A & 0x1F; /* for *V variants */

    switch (alu_code)
    {
    case ALU_AND:
        result = operand_A & operand_B;
        break;
    case ALU_OR:
        result = operand_A | operand_B;
        break;
    case ALU_XOR:
        result = operand_A ^ operand_B;
        break;
    case ALU_NOR:
        result = ~(operand_A | operand_B);
        break;
    case ALU_ADD:
        result = operand_A + operand_B;
        break;
    case ALU_SUB:
        result = operand_A - operand_B;
        break;
    case ALU_SLT:
        result = ((int32_t)operand_A < (int32_t)operand_B) ? 1 : 0;
        break;
    case ALU_SLTU:
        result = (operand_A < operand_B) ? 1 : 0;
        break;
    case ALU_SLL:
        result = operand_B << shift_amount_sa;
        break;
    case ALU_SRL:
        result = operand_B >> shift_amount_sa;
        break;
    case ALU_SRA:
        result = ((int32_t)operand_B) >> shift_amount_sa;
        break;
    case ALU_SLLV:
        result = operand_B << shift_amount_sb;
        break;
    case ALU_SRLV:
        result = operand_B >> shift_amount_sb;
        break;
    case ALU_SRAV:
        result = ((int32_t)operand_B) >> shift_amount_sb;
        break;
    default:
        printf("  [WARNING] Unknown ALU code %d - result set to 0\n", alu_code);
        result = 0;
        break;
    }

    *zero_flag = (result == 0);
    return result;
}

void init_registers()
{
    int index;
    for (index = 0; index < 32; ++index)
    {
        global_registers[index] = 0;
    }

    /* Initialize stack pointer and link register */
    global_registers[29] = INITIAL_STACK_POINTER;       /* $sp */
    global_registers[31] = PROGRAM_TERMINATION_ADDRESS; /* $ra */

    program_counter = 0;

    /* Initialize statistics variables */
    cycle_counter = 0;
    executed_instruction_count = 0;
    r_type_count = 0;
    i_type_count = 0;
    j_type_count = 0;
    memory_access_count = 0;
    branch_taken_count = 0;

    printf("Registers initialized. Stack pointer: 0x%08X, Link register: 0x%08X\n",
           global_registers[29], global_registers[31]);
}

uint32_t fetch_instruction()
{
    if (program_counter >= MEMORY_SIZE_BYTES - 3 || program_counter % 4 != 0)
    {
        printf("  [ERROR] Invalid PC 0x%08X - Cannot fetch instruction\n", program_counter);
        return 0;
    }

    uint32_t instruction_word = load_word_little_endian(program_counter);
    printf("  [Fetch] 0x%08X (PC=0x%08X)\n", instruction_word, program_counter);
    return instruction_word;
}

void decode_instruction(uint32_t instruction_word, DecodedInstruction *decoded)
{
    memset(decoded, 0, sizeof(DecodedInstruction));

    decoded->raw_bits = instruction_word;
    decoded->opcode = (instruction_word >> 26) & 0x3F;

    if (decoded->opcode == OPCODE_RTYPE)
    {
        /* R‑type */
        decoded->rs = (instruction_word >> 21) & 0x1F;
        decoded->rt = (instruction_word >> 16) & 0x1F;
        decoded->rd = (instruction_word >> 11) & 0x1F;
        decoded->shamt = (instruction_word >> 6) & 0x1F;
        decoded->funct = instruction_word & 0x3F;
        printf("  [Decode] R-type: opcode 0x%02X, rs=%d, rt=%d, rd=%d, shamt=%d, funct=0x%02X\n",
               decoded->opcode, decoded->rs, decoded->rt, decoded->rd, decoded->shamt, decoded->funct);
    }
    else if (decoded->opcode == OPCODE_J || decoded->opcode == OPCODE_JAL)
    {
        /* J‑type */
        decoded->address26 = instruction_word & 0x03FFFFFF;
        printf("  [Decode] J-type: opcode 0x%02X, address=0x%07X\n",
               decoded->opcode, decoded->address26);
    }
    else
    {
        /* I‑type */
        decoded->rs = (instruction_word >> 21) & 0x1F;
        decoded->rt = (instruction_word >> 16) & 0x1F;
        decoded->immediate = instruction_word & 0xFFFF;
        printf("  [Decode] I-type: opcode 0x%02X, rs=%d, rt=%d, immediate=0x%04X\n",
               decoded->opcode, decoded->rs, decoded->rt, (uint16_t)decoded->immediate);
    }
}

uint32_t execute_instruction(const DecodedInstruction *decoded, const ControlSignal *control, int *zero_flag)
{
    uint32_t operand_A = global_registers[decoded->rs];
    uint32_t operand_B;

    /* Address reference check */
    if (decoded->rs >= 32)
    {
        printf("  [ERROR] Invalid register index rs=%d\n", decoded->rs);
        return 0;
    }

    if (decoded->rt >= 32 && !control->ALUSrc)
    {
        printf("  [ERROR] Invalid register index rt=%d\n", decoded->rt);
        return 0;
    }

    printf("  [Execute] rs(r%d)=0x%08X", decoded->rs, operand_A);

    if (control->ALUSrc)
    {
        /* Use immediate value */
        if (decoded->opcode == OPCODE_ANDI || decoded->opcode == OPCODE_ORI || decoded->opcode == OPCODE_XORI)
        {
            /* Logical operations use zero-extend */
            operand_B = zero_extend16((uint16_t)decoded->immediate);
            printf(", immediate(zero-extended)=0x%08X\n", operand_B);
        }
        else if (decoded->opcode == OPCODE_LUI)
        {
            /* LUI loads immediate to upper 16 bits */
            operand_B = ((uint32_t)decoded->immediate) << 16;
            printf(", immediate(left-shifted)=0x%08X\n", operand_B);
        }
        else
        {
            /* Other instructions use sign-extend */
            operand_B = (uint32_t)sign_extend16(decoded->immediate);
            printf(", immediate(sign-extended)=0x%08X\n", operand_B);
        }
    }
    else
    {
        /* Use register value */
        operand_B = global_registers[decoded->rt];
        printf(", rt(r%d)=0x%08X\n", decoded->rt, operand_B);
    }

    uint8_t alu_code = alu_signal_generator(control->ALUOp, decoded->funct);
    uint32_t alu_result = alu_execute(alu_code, operand_A, operand_B, decoded->shamt, (bool *)zero_flag);

    printf("  [Execute] ALU operation %d, result 0x%08X, zero_flag=%d\n",
           alu_code, alu_result, *zero_flag);
    return alu_result;
}

uint32_t memory_access_stage(const ControlSignal *control, uint32_t address, uint32_t rt_value)
{
    uint32_t data_from_memory = 0;

    if (control->MemRead)
    {
        data_from_memory = load_word_little_endian(address);
        memory_access_count++;
        printf("  [Memory Access   ] LW addr 0x%08X → 0x%08X\n", address, data_from_memory);
    }
    else if (control->MemWrite)
    {
        store_word_little_endian(address, rt_value);
        memory_access_count++;
        printf("  [Memory Access   ] SW addr 0x%08X ← 0x%08X\n", address, rt_value);
    }
    else
    {
        printf("  [Memory Access   ] No memory operation\n");
    }

    return data_from_memory;
}

void write_back_stage(const ControlSignal *control, uint8_t destination_register, uint32_t alu_result, uint32_t memory_data, uint32_t pc_plus_4)
{
    /* Skip if RegWrite is off or destination is r0(=0) */
    if (!control->RegWrite || destination_register == 0)
    {
        printf("  [Write Back      ] No write-back operation\n");
        return;
    }

    uint32_t value_to_write;

    /* JAL instruction stores PC+4 */
    if (control->Jump && destination_register == 31)
    {
        value_to_write = pc_plus_4;
        printf("  [Write Back      ] JAL: Reg[%u] ← 0x%08X (PC+4)\n",
               destination_register, value_to_write);
    }
    /* Use value read from memory */
    else if (control->MemToReg)
    {
        value_to_write = memory_data;
        printf("  [Write Back      ] Reg[%u] ← 0x%08X (from memory)\n",
               destination_register, value_to_write);
    }
    /* Use ALU result */
    else
    {
        value_to_write = alu_result;
        printf("  [Write Back      ] Reg[%u] ← 0x%08X (from ALU)\n",
               destination_register, value_to_write);
    }

    /* Update register */
    global_registers[destination_register] = value_to_write;
}

void datapath_single_cycle()
{
    printf("\n===== Starting MIPS Single-Cycle Execution =====\n");

    while (program_counter != PROGRAM_TERMINATION_ADDRESS &&
           cycle_counter < MAX_CYCLE_COUNT)
    {
        printf("\n──────────── Cycle %u ────────────\n", cycle_counter);

        /* IF stage */
        uint32_t current_instruction_word = fetch_instruction();
        uint32_t pc_plus_4 = program_counter + 4; /* Next instruction address (default) */

        /* All-zero instruction treated as NOP */
        if (current_instruction_word == 0)
        {
            printf("  [NOP detected] Skipping\n");
            program_counter = pc_plus_4;
            ++cycle_counter;
            continue;
        }

        /* ID stage */
        DecodedInstruction decoded;
        decode_instruction(current_instruction_word, &decoded);
        ControlSignal control;
        control_signal_generator(decoded.opcode, decoded.funct, &control);

        /* EX stage */
        int zero_flag = 0;
        uint32_t alu_result = execute_instruction(&decoded, &control, &zero_flag);

        /* MEM stage */
        uint32_t memory_data = memory_access_stage(&control, alu_result, global_registers[decoded.rt]);

        /* Determine destination register */
        uint8_t destination_register = 0;
        if (control.Jump && decoded.opcode == OPCODE_JAL)
        {
            /* jal saves return address to $ra(31) */
            destination_register = 31;
        }
        else if (control.RegDst)
        {
            /* R-type uses rd field */
            destination_register = decoded.rd;
        }
        else
        {
            /* I-type uses rt field */
            destination_register = decoded.rt;
        }

        /* WB stage */
        write_back_stage(&control, destination_register, alu_result, memory_data, pc_plus_4);

        /* PC update */
        uint32_t next_pc = pc_plus_4; /* Default value */

        if (control.Jump)
        {
            /* J-type jump instruction */
            next_pc = calculate_jump_address(program_counter, decoded.address26);
            printf("  [PC Update] Jump to 0x%08X\n", next_pc);
        }
        else if (control.JumpRegister)
        {
            /* Register-based jump (jr, jalr) */
            next_pc = global_registers[decoded.rs];
            printf("  [PC Update] Jump to register rs(r%d) value: 0x%08X\n",
                   decoded.rs, next_pc);

            /* jalr instruction - save return address */
            if (decoded.funct == FUNCT_JALR && decoded.rd != 0)
            {
                global_registers[decoded.rd] = pc_plus_4;
                printf("  [PC Update] JALR: Saved return address 0x%08X to r%d\n",
                       pc_plus_4, decoded.rd);
            }
        }
        else if (control.Branch)
        {
            /* Branch instruction */
            int branch_taken = 0;

            if (decoded.opcode == OPCODE_BEQ)
                branch_taken = zero_flag; /* beq: if equal (zero_flag=1) */
            else if (decoded.opcode == OPCODE_BNE)
                branch_taken = !zero_flag; /* bne: if not equal (zero_flag=0) */

            if (branch_taken)
            {
                next_pc = calculate_branch_address(program_counter, decoded.immediate);
                printf("  [PC Update] Branch taken to 0x%08X\n", next_pc);
                ++branch_taken_count;
            }
            else
            {
                printf("  [PC Update] Branch not taken, next PC = 0x%08X\n", next_pc);
            }
        }
        else
        {
            printf("  [PC Update] Sequential, next PC = 0x%08X\n", next_pc);
        }

        /* Update PC */
        program_counter = next_pc;

        /* Update statistics based on instruction type */
        if (decoded.opcode == OPCODE_RTYPE)
            ++r_type_count;
        else if (decoded.opcode == OPCODE_J || decoded.opcode == OPCODE_JAL)
            ++j_type_count;
        else
            ++i_type_count;

        ++executed_instruction_count;
        ++cycle_counter;

        /* Periodic status output for debugging */
        if (cycle_counter % 100 == 0)
        {
            printf("  [Progress] %u cycles executed, PC=0x%08X, executed instructions=%u\n",
                   cycle_counter, program_counter, executed_instruction_count);
        }

        /* Infinite loop detection and prevention */
        if (cycle_counter >= MAX_CYCLE_COUNT)
        {
            printf("\n\n  [ERROR] Maximum cycle count (%u) reached. Possible infinite loop!\n", MAX_CYCLE_COUNT);
            printf("  Last PC value: 0x%08X, Next PC value: 0x%08X\n",
                   program_counter, next_pc);
            break;
        }
    }
}

/*───────────────────────────────────────────────────────────────────────────────────────────────*/
/* 6.  Binary loader and summary print                                                       */
/*───────────────────────────────────────────────────────────────────────────────────────────────*/
void load_binary_file(const char *file_path)
{
    FILE *fp = fopen(file_path, "rb");
    if (!fp)
    {
        perror("Error opening binary file");
        exit(1);
    }

    /* Initialize memory */
    memset(global_memory, 0, MEMORY_SIZE_BYTES);

    /* Read binary file */
    size_t memory_index = 0;
    size_t words_read = 0;
    uint32_t word;

    /* Critical Fix: Properly load binary data in little-endian format */
    while (fread(&word, sizeof(uint32_t), 1, fp) == 1)
    {
        if (memory_index + 3 >= MEMORY_SIZE_BYTES)
        {
            fprintf(stderr, "Binary file too large for memory size!\n");
            fclose(fp);
            exit(1);
        }

        /* Store in memory byte by byte (little endian) */
        global_memory[memory_index++] = word & 0xFF;
        global_memory[memory_index++] = (word >> 8) & 0xFF;
        global_memory[memory_index++] = (word >> 16) & 0xFF;
        global_memory[memory_index++] = (word >> 24) & 0xFF;

        words_read++;
    }

    fclose(fp);
    printf("Binary file loaded: %zu words (%zu bytes) read\n", words_read, memory_index);

    /* Debug output to validate binary contents */
    printf("Analyzing loaded binary...\n");

    /* Count J-type and jalr instructions to ensure proper loading */
    int j_type_count = 0;
    int jalr_count = 0;

    for (size_t addr = 0; addr < memory_index; addr += 4)
    {
        uint32_t instr = load_word_little_endian(addr);
        uint8_t opcode = (instr >> 26) & 0x3F;

        /* Detect J-type instructions */
        if (opcode == OPCODE_J || opcode == OPCODE_JAL)
        {
            j_type_count++;
            uint32_t target = instr & 0x03FFFFFF;
            printf("  Found J-type instruction at 0x%08zX, target: 0x%07X\n", addr, target);
        }

        /* Detect jalr instructions */
        if (opcode == OPCODE_RTYPE)
        {
            uint8_t funct = instr & 0x3F;
            if (funct == FUNCT_JALR)
            {
                jalr_count++;
                uint8_t rs = (instr >> 21) & 0x1F;
                uint8_t rd = (instr >> 11) & 0x1F;
                printf("  Found JALR instruction at 0x%08zX, rs=%d, rd=%d\n", addr, rs, rd);
            }
        }
    }

    printf("  Analysis complete: found %d J-type and %d JALR instructions\n", j_type_count, jalr_count);
}