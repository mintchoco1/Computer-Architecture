#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#define MAX_MEM 0x1000000

#define ENDIANNESS_LITTLE 0
#define ENDIANNESS_BIG 1
#define ENDIANNESS ENDIANNESS_LITTLE

uint8_t mem[MAX_MEM];

int inst_cnt = 0;
int r_cnt = 0;
int i_cnt = 0;
int j_cnt = 0;
int ma_cnt = 0;
int b_cnt = 0;

// for mult inst
uint64_t hi = 0;
uint64_t lo = 0;

typedef struct {
    uint32_t mips_inst;
    uint8_t opcode;
    uint8_t rs;
    uint8_t rt;
    uint8_t rd;
    uint8_t shamt;
    uint8_t funct;
    uint16_t immediate;
    uint32_t address;
    int type; // 0: R, 1: J, 2: I
} Instruction;

typedef struct {
    uint32_t Reg[32];
    uint32_t pc;
    uint32_t hi;
    uint32_t lo;
} Registers;

typedef struct {
    int RegDst;
    int ALUSrc;
    int MemToReg;
    int RegWrite;
    int MemRead;
    int MemWrite;
    int Branch;
    int Jump;
    int JumpReg;
    int JumpLink;
    int ALUOp;
} ControlSignal;

void init_regs(Registers* r) {
    r->hi = 0;
    r->lo = 0;
    r->pc = 0;
    for (int i = 0; i < 32; i++) {
        r->Reg[i] = 0;
    }
    r->Reg[31] = 0xffffffff;  // 종료 조건
    r->Reg[29] = 0x1000000;   // $sp
}

int32_t sign_extend(uint16_t imm) {
    return (int16_t)imm;
}

void instruction_to_decoded_info(const Instruction* inst, uint32_t* decoded_info) {
    decoded_info[0] = inst->opcode;
    decoded_info[1] = inst->type;
    decoded_info[2] = inst->funct;
    decoded_info[3] = inst->rs;
    decoded_info[4] = inst->rt;
    decoded_info[5] = (inst->type == 1) ? sign_extend(inst->immediate) : inst->rd;
    decoded_info[6] = (inst->type == 2) ? (inst->address << 2) : inst->shamt;
    decoded_info[7] = (inst->type == 1) ? inst->rt : inst->rd; // write-back 대상
}

Instruction decoded_info_to_instruction(const uint32_t* decoded_info) {
    Instruction inst;
    inst.mips_inst = 0;  // 복원이 어려우므로 0으로 설정
    inst.opcode = decoded_info[0];
    inst.type = decoded_info[1];

    if (inst.type == 0) {  // R-type
        inst.funct = decoded_info[2];
        inst.rs = decoded_info[3];
        inst.rt = decoded_info[4];
        inst.rd = decoded_info[5];
        inst.shamt = decoded_info[6];
    } else if (inst.type == 1) {  // I-type
        inst.rs = decoded_info[3];
        inst.rt = decoded_info[4];
        inst.immediate = (uint16_t)decoded_info[5];  // sign-extend 안함
    } else if (inst.type == 2) {  // J-type
        inst.address = decoded_info[6] >> 2; // 복원 시 address는 오른쪽으로 2 shift
    }

    return inst;
}

void set_control_signal(const Instruction* inst, ControlSignal* ctrl) {
    memset(ctrl, 0, sizeof(ControlSignal));

    switch (inst->opcode) {
        case 0x00:  // R-type
            ctrl->RegDst = 1;
            ctrl->RegWrite = 1;
            ctrl->ALUOp = 0b10;

            switch (inst->funct) {
                case 0x08:  // jr
                    ctrl->JumpReg = 1;
                    ctrl->RegWrite = 0;
                    break;
                case 0x09:  // jalr
                    ctrl->JumpReg = 1;
                    ctrl->JumpLink = 1;
                    ctrl->RegWrite = 1;
                    break;
                case 0x10: case 0x12:  // mfhi, mflo
                    ctrl->RegWrite = 1;
                    break;
                case 0x11: case 0x13:  // mthi, mtlo
                    ctrl->RegWrite = 0;
                    break;
                case 0x18: case 0x19:  // mult, multu
                case 0x1A: case 0x1B:  // div, divu
                    ctrl->RegWrite = 0;
                    break;
                default: break;
            }
            break;

        case 0x02:  // j
            ctrl->Jump = 1;
            break;
        case 0x03:  // jal
            ctrl->Jump = 1;
            ctrl->JumpLink = 1;
            ctrl->RegWrite = 1;
            break;

        case 0x04: case 0x05:  // beq, bne
        case 0x06: case 0x07:  // blez, bgtz
        case 0x01:             // bltz, bgez
            ctrl->Branch = 1;
            ctrl->ALUOp = 0b01;
            break;

        case 0x08: case 0x09:  // addi, addiu
        case 0x0C: case 0x0D:  // andi, ori
        case 0x0A: case 0x0B:  // slti, sltiu
        case 0x0E:             // xori
        case 0x0F:             // lui
            ctrl->RegWrite = 1;
            ctrl->ALUSrc = 1;
            ctrl->RegDst = 0;
            ctrl->ALUOp = 0b00;
            break;

        case 0x23: case 0x24: case 0x25:  // lw, lbu, lhu
            ctrl->MemRead = 1;
            ctrl->MemToReg = 1;
            ctrl->RegWrite = 1;
            ctrl->ALUSrc = 1;
            ctrl->ALUOp = 0b00;
            break;

        case 0x28: case 0x29: case 0x2B:  // sb, sh, sw
            ctrl->MemWrite = 1;
            ctrl->ALUSrc = 1;
            ctrl->ALUOp = 0b00;
            break;

        default:
            // 알 수 없는 명령어는 아무 제어 신호도 설정하지 않음
            break;
    }
}

uint8_t alu_control(uint8_t ALUOp, uint8_t funct) {
    if (ALUOp == 0b00) {
        // I-type: add, lw, sw 등
        return 0b0010; // ADD
    } else if (ALUOp == 0b01) {
        // beq, bne 등 → SUB
        return 0b0110;
    } else if (ALUOp == 0b10) {
        // R-type → funct로 결정
        switch (funct) {
            case 0x20: return 0b0010; // add
            case 0x21: return 0b0010; // addu
            case 0x22: return 0b0110; // sub
            case 0x23: return 0b0110; // subu
            case 0x24: return 0b0000; // and
            case 0x25: return 0b0001; // or
            case 0x26: return 0b0011; // xor
            case 0x27: return 0b1100; // nor
            case 0x2A: return 0b0111; // slt
            case 0x2B: return 0b0111; // sltu
            case 0x00: return 0b1000; // sll
            case 0x02: return 0b1001; // srl
            case 0x03: return 0b1010; // sra
            case 0x18: return 0b1101; // mult
            case 0x19: return 0b1110; // multu
            case 0x1A: return 0b1111; // div
            case 0x1B: return 0b1111; // divu
            default: return 0b1111;   // 예외 처리
        }
    }

    return 0b1111; // 정의되지 않은 경우
}

uint32_t alu_execute(uint8_t signal, uint32_t a, uint32_t b) {
    switch (signal) {
        case 0b0000: return a & b;
        case 0b0001: return a | b;
        case 0b0010: return a + b;
        case 0b0011: return a ^ b;
        case 0b0110: return a - b;
        case 0b0111: return ((int32_t)a < (int32_t)b) ? 1 : 0;
        case 0b1000: return b << a;
        case 0b1001: return b >> a;
        case 0b1100: return ~(a | b);
        case 0b1101: /* MULT */ {
            uint64_t result = (uint64_t)a * (uint64_t)b;
            hi = (result >> 32) & 0xFFFFFFFF;
            lo = result & 0xFFFFFFFF;
            return 0;
        }
        default: return 0;
    }
}

uint32_t fetch(Registers *regs, uint8_t* mem) {
    printf("\t[Instruction Fetch] ");
    uint32_t temp = 0;
    for (int i = 0; i < 4; i++) {
        temp = (temp << 8) | mem[regs->pc + (3 - i)];
    }
    uint32_t pc = regs->pc;
    regs->pc += 4;
    printf(" 0x%08x (PC=0x%08x)\n", temp, pc);
    return temp;
}

Instruction decode(uint32_t word) {
    Instruction inst;
    inst.mips_inst = word;
    inst.opcode = (word >> 26) & 0x3F;

    if (inst.opcode == 0x00) {                 // R‑type 
        inst.rs = (word >> 21) & 0x0000001F;
        inst.rt = (word >> 16) & 0x0000001F;
        inst.rd = (word >> 11) & 0x0000001F;
        inst.shamt = (word >> 6) & 0x0000001F;
        inst.funct = word & 0x3F;
        inst.type = 0;
        
        printf("\t[Instruction Decode] Type: R, Inst: ");
        switch(inst.funct) {
            case 0x08: printf("jr r%d", inst.rs); break;
            case 0x20: printf("add r%d r%d r%d", inst.rd, inst.rs, inst.rt); break;
            case 0x22: printf("sub r%d r%d r%d", inst.rd, inst.rs, inst.rt); break;
            case 0x24: printf("and r%d r%d r%d", inst.rd, inst.rs, inst.rt); break;
            case 0x25: printf("or r%d r%d r%d", inst.rd, inst.rs, inst.rt); break;
            case 0x27: printf("nor r%d r%d r%d", inst.rd, inst.rs, inst.rt); break;
            case 0x00: printf("sll r%d r%d %d", inst.rd, inst.rt, inst.shamt); break;
            case 0x02: printf("srl r%d r%d %d", inst.rd, inst.rt, inst.shamt); break;
            case 0x2A: printf("slt r%d r%d r%d", inst.rd, inst.rs, inst.rt); break;
            case 0x2B: printf("sltu r%d r%d r%d", inst.rd, inst.rs, inst.rt); break;
            case 0x23: printf("subu r%d r%d r%d", inst.rd, inst.rs, inst.rt); break;
            case 0x21: printf("addu r%d r%d r%d", inst.rd, inst.rs, inst.rt); break;
            case 0x18: printf("mult r%d r%d", inst.rs, inst.rt); break;
            default: printf("unknown R-type"); break;
        }
        r_cnt++;
    }
    else if (inst.opcode == 0x02 || inst.opcode == 0x03) { // J‑type 
        inst.address = word & 0x03FFFFFF;
        inst.type = 2;
        
        printf("\t[Instruction Decode] Type: J, Inst: ");
        if (inst.opcode == 0x02) {
            printf("j 0x%x", inst.address);
        } else {
            printf("jal 0x%x", inst.address);
        }
        j_cnt++;
    }
    else {                                     // I‑type 
        inst.rs = (word >> 21) & 0x0000001F;
        inst.rt = (word >> 16) & 0x0000001F;
        inst.immediate = word & 0xFFFF;
        inst.type = 1;
        
        printf("\t[Instruction Decode] Type: I, Inst: ");
        switch(inst.opcode) {
            case 0x04: printf("beq r%d r%d %x", inst.rs, inst.rt, inst.immediate); break;
            case 0x05: printf("bne r%d r%d %x", inst.rs, inst.rt, inst.immediate); break;
            case 0x08: printf("addi r%d r%d %d", inst.rt, inst.rs, inst.immediate); break;
            case 0x09: printf("addiu r%d r%d %d", inst.rt, inst.rs, inst.immediate); break;
            case 0x0A: printf("slti r%d r%d %d", inst.rt, inst.rs, inst.immediate); break;
            case 0x0B: printf("sltiu r%d r%d %d", inst.rt, inst.rs, inst.immediate); break;
            case 0x0C: printf("andi r%d r%d %d", inst.rt, inst.rs, inst.immediate); break;
            case 0x0D: printf("ori r%d r%d %d", inst.rt, inst.rs, inst.immediate); break;
            case 0x23: printf("lw r%d %d(r%d)", inst.rt, inst.immediate, inst.rs); break;
            case 0x2B: printf("sw r%d %d(r%d)", inst.rt, inst.immediate, inst.rs); break;
            case 0x0F: printf("lui r%d %d", inst.rt, inst.immediate); break;
            default: printf("unknown I-type"); break;
        }
        i_cnt++;
    }
    printf("\n");
    return inst;
}

void execute(Registers* regs, ControlSignal* ctrl, uint32_t* decoded_info, uint32_t* execute_to_memory) {
    uint8_t opcode = decoded_info[0];
    int type = decoded_info[1];
    uint8_t funct = decoded_info[2];
    uint8_t rs = decoded_info[3];
    uint8_t rt = decoded_info[4];

    printf("\t[Execute] ALU = ");

    uint32_t operand1 = regs->Reg[rs];
    uint32_t operand2;
    
    // Select the second operand based on ALUSrc
    if (ctrl->ALUSrc) {
        operand2 = decoded_info[5];  // immediate value
    } else {
        operand2 = regs->Reg[rt];    // register value
    }

    // Special case for shift operations
    if (type == 0 && (funct == 0x00 || funct == 0x02 || funct == 0x03)) { // sll, srl, sra
        operand1 = regs->Reg[rt];
        operand2 = decoded_info[6];  // shamt value
    }

    uint8_t alu_sig = alu_control(ctrl->ALUOp, funct);
    uint32_t alu_result = alu_execute(alu_sig, operand1, operand2);

    // Check for branch
    if (ctrl->Branch) {
        bool taken = false;
        switch (opcode) {
            case 0x04: // beq
                taken = (regs->Reg[rs] == regs->Reg[rt]);
                break;
            case 0x05: // bne
                taken = (regs->Reg[rs] != regs->Reg[rt]);
                break;
            case 0x06: // blez
                taken = ((int32_t)regs->Reg[rs] <= 0);
                break;
            case 0x07: // bgtz
                taken = ((int32_t)regs->Reg[rs] > 0);
                break;
            case 0x01: // bltz, bgez
                if (rt == 0x00)      taken = ((int32_t)regs->Reg[rs] < 0);  // bltz
                else if (rt == 0x01) taken = ((int32_t)regs->Reg[rs] >= 0); // bgez
                break;
        }
        
        if (taken) {
            regs->pc = regs->pc - 4 + (decoded_info[5] << 2); // PC + (sign-extended immediate << 2)
            printf("Branch taken, new PC: 0x%08x\n", regs->pc);
            return;
        } else {
            printf("Branch not taken\n");
            return;
        }
    }

    // Process Jump
    if (ctrl->Jump) {
        uint32_t jump_addr = decoded_info[6]; // jump_addr = (PC & 0xF0000000) | (address << 2)
        regs->pc = (regs->pc & 0xF0000000) | jump_addr;
        
        // For jal, save return address
        if (ctrl->JumpLink) {
            regs->Reg[31] = regs->pc + 4;
        }
        
        printf("Jump to: 0x%08x\n", regs->pc);
        return;
    }

    // Process JumpReg (jr, jalr)
    if (ctrl->JumpReg) {
        uint32_t jump_addr = regs->Reg[rs];
        
        // For jalr, save return address
        if (ctrl->JumpLink) {
            regs->Reg[decoded_info[5]] = regs->pc + 4; // rd register
        }
        
        regs->pc = jump_addr;
        printf("Jump register to: 0x%08x\n", regs->pc);
        return;
    }

    // For operations requiring write-back
    execute_to_memory[0] = alu_result;                                    // ALU result
    execute_to_memory[1] = ctrl->RegDst ? decoded_info[7] : decoded_info[4]; // Destination register
    execute_to_memory[2] = regs->Reg[rt];                                // Store value for SW
    execute_to_memory[3] = ctrl->MemRead;                                // MemRead flag
    execute_to_memory[4] = ctrl->MemWrite;                               // MemWrite flag
    execute_to_memory[5] = ctrl->MemToReg;                               // MemToReg flag
    execute_to_memory[6] = ctrl->RegWrite;                               // RegWrite flag
    
    printf("0x%08x\n", alu_result);
}

void mem_access(uint8_t* mem, uint32_t* execute_to_memory, uint32_t* memory_to_writeback) {
    printf("\t[Memory Access] ");
    
    uint32_t alu_result = execute_to_memory[0];
    uint32_t write_data = execute_to_memory[2];
    uint8_t mem_read = execute_to_memory[3];
    uint8_t mem_write = execute_to_memory[4];
    
    if (mem_read) {
        // Memory read (lw, lb, lhu, etc.)
        uint32_t data = 0;
        for (int i = 0; i < 4; i++) {
            data |= (uint32_t)mem[alu_result + i] << (i * 8);
        }
        memory_to_writeback[0] = data;
        printf("Load Address: 0x%08x, Value: 0x%08x\n", alu_result, data);
        ma_cnt++;
    } else if (mem_write) {
        // Memory write (sw, sb, sh, etc.)
        for (int i = 0; i < 4; i++) {
            mem[alu_result + i] = (write_data >> (i * 8)) & 0xFF;
        }
        printf("Store Address: 0x%08x, Value: 0x%08x\n", alu_result, write_data);
        ma_cnt++;
    } else {
        memory_to_writeback[0] = alu_result;  // Just pass ALU result
        printf("Pass\n");
    }
    
    // Pass control signals and destination register
    memory_to_writeback[1] = execute_to_memory[1];  // Destination register
    memory_to_writeback[2] = execute_to_memory[5];  // MemToReg
    memory_to_writeback[3] = execute_to_memory[6];  // RegWrite
}

void write_back(Registers* regs, uint32_t* memory_to_writeback) {
    printf("\t[Write Back] ");
    
    uint32_t write_data = memory_to_writeback[0];
    uint32_t dest_reg = memory_to_writeback[1];
    uint8_t mem_to_reg = memory_to_writeback[2];
    uint8_t reg_write = memory_to_writeback[3];
    
    if (reg_write && dest_reg != 0) {  // Don't write to $0 register
        regs->Reg[dest_reg] = write_data;
        printf("R%d = 0x%08x\n", dest_reg, write_data);
    } else {
        printf("No write back\n");
    }
}

void load_mips_binary(const char* filename) {
    FILE* file = fopen(filename, "rb");

    // 파일 열기 실패 시 종료
    if (!file) {
        perror("File opening failed");
        exit(1);
    }
    
    uint32_t temp;
    size_t bytesRead;
    size_t mem_idx = 0;

    while ((bytesRead = fread(&temp, sizeof(uint32_t), 1, file)) > 0 && mem_idx < (MAX_MEM - 3)) {
        // 파일에서 uint32_t 크기만큼 읽고, Memory 배열에 값을 리틀엔디안 식으로 할당.
        mem[mem_idx++] = (temp >> 24) & 0xFF; // 상위 8비트
        mem[mem_idx++] = (temp >> 16) & 0xFF; // 다음 8비트
        mem[mem_idx++] = (temp >> 8) & 0xFF; // 다음 8비트
        mem[mem_idx++] = temp & 0xFF; // 하위 8비트
    }
    fclose(file);
}

void single_cycle(Registers* regs, uint8_t* mem) {
    uint32_t inst_word;
    Instruction inst;
    ControlSignal ctrl;

    uint32_t decoded_info[8];
    uint32_t execute_to_memory[10]; 
    uint32_t memory_to_writeback[4]; 

    inst_cnt = 0;
    
    while (regs->pc != 0xffffffff) {
        printf("Cycle %d:\n", ++inst_cnt);
        
        // Fetch stage
        inst_word = fetch(regs, mem);
        if (inst_word == 0) {
            printf("\tNOP\n\n");
            continue;
        }
        
        // Decode stage
        inst = decode(inst_word);
        set_control_signal(&inst, &ctrl);
        instruction_to_decoded_info(&inst, decoded_info);
        
        // Execute stage
        execute(regs, &ctrl, decoded_info, execute_to_memory);
        
        // Memory access stage
        mem_access(mem, execute_to_memory, memory_to_writeback);
        
        // Write back stage
        write_back(regs, memory_to_writeback);
        
        printf("\n");
    }
    
    printf("Simulation finished.\n");
    printf("Cycles: %d, R-type: %d, I-type: %d, J-type: %d, Memory accesses: %d\n", 
           inst_cnt, r_cnt, i_cnt, j_cnt, ma_cnt);
    printf("Final result in $v0 (R2): %d (0x%08x)\n", regs->Reg[2], regs->Reg[2]);
}

int main(int argc, char* argv[]) {
    const char* filename;
    
    if (argc > 1) {
        filename = argv[1];
    } else {
        printf("No input file specified, please provide a binary file.\n");
        return 1;
    }
    
    memset(mem, 0, MAX_MEM);
    
    load_mips_binary(filename);
    
    Registers regs;
    init_regs(&regs);

    single_cycle(&regs, mem);

    return 0;
}