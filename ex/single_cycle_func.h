#include <stdio.h>
#include <stdint.h>


#define MAX_MEM 0x1000000

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
    uint32_t reg[32];
    int pc;
} Regs;

typedef struct {
    int mem_rd;
    int mem_to_reg;
    int mem_wr;
    int reg_wr;
    int alu_ctrl;
    int ex_skip;
    int ma_skip;
} Ctrl;

void init_ctrl(Ctrl* c) {
    c->mem_rd = 0;
    c->mem_to_reg = 0;
    c->mem_wr = 0;
    c->reg_wr = 1;
    c->alu_ctrl = -1;
    c->ex_skip = 0;
    c->ma_skip = 0;
}
//
void path_ctrl(Ctrl* c, uint32_t* opcode, int step) {
    if (step == 2) { // execute step
        if (opcode[0] == 0) { // R type
            if (opcode[2] == 0x20) { // add
                c->alu_ctrl = 0b0010;
                return;
            }
            //else if (opcode[2] == 0x18) { // mult
            //    c->alu_ctrl = 0b1001;
            //    return;
            //}
            else if (opcode[2] == 0x12) { // mflo
                c->ex_skip = 1;
                return;
            }
            else if (opcode[2] == 0x24) { // and
                c->alu_ctrl = 0b0000;
                return;
            }
            else if (opcode[2] == 0x25) { // or
                c->alu_ctrl = 0b0001;
                return;
            }
            else if (opcode[2] == 0x27) { // nor
                c->alu_ctrl = 0b1100;
                return;
            }
            else if (opcode[2] == 0x00) { // sll
                c->alu_ctrl = 0b1110;
                return;
            }
            else if (opcode[2] == 0x02) { // srl
                c->alu_ctrl = 0b1111;
                return;
            }
            else if (opcode[2] == 0x2a) { // slt
                c->alu_ctrl = 0b0111;
                return;
            }
            else if (opcode[2] == 0x2b) { // sltu
                c->alu_ctrl = 0b0111;
                return;
            }
            else if (opcode[2] == 0x23) { // subu
                c->alu_ctrl = 0b0110;
                return;
            }
            else if (opcode[2] == 0x21) { // addu
                c->alu_ctrl = 0b0010;
                return;
            }
        }
        else { // I type
            if (opcode[0] == 2) { // j
                return;
            }
            else if (opcode[0] == 4) { // beq
                c->alu_ctrl = 0b0110;
                return;
            }
            else if (opcode[0] == 5) { // bne
                c->alu_ctrl = 0b0110;
                return;
            }
            else if (opcode[0] == 8) { // addi
                c->alu_ctrl = 0b0010;
                return;
            }
            else if (opcode[0] == 9) { // addiu
                c->alu_ctrl = 0b0010;
                return;
            }
            else if (opcode[0] == 10) { // slti
                c->alu_ctrl = 0b0111;
                return;
            }
            else if (opcode[0] == 11) { // sltiu
                c->alu_ctrl = 0b0111;
                return;
            }
            else if (opcode[0] == 12) { // andi
                c->alu_ctrl = 0b0000;
                return;
            }
            else if (opcode[0] == 13) { // ori
                c->alu_ctrl = 0b0001;
                return;
            }
            else if (opcode[0] == 35) { // lw
                c->alu_ctrl = 0b0010;
                return;
            }
            else if (opcode[0] == 43) { // sw
                c->alu_ctrl = 0b0010;
                return;
            }
        }
    }
    else if (step == 3) { // memory access step
        if (opcode[1] == 1 || opcode[1] == 3 || opcode[1] == 4 || opcode[1] == 5) {
            c->ma_skip = 1;
        }
        if (opcode[0] == 35) { // lw
            c->mem_rd = 1;
            c->mem_wr = 0;
        }
        else if (opcode[0] == 43) { // sw
            c->mem_rd = 0;
            c->mem_wr = 1;
        }
        return;
    }
    else if (step == 4) { // write back step
        if (opcode[1] == 1 || opcode[1] == 3 || opcode[1] == 4 || opcode[1] == 5) {
            c->reg_wr = 0;
        }
        if (opcode[0] == 43) {
            c->reg_wr = 0;
        }
        if (opcode[0] == 0) { // R type
            c->mem_to_reg = 0;
        }
        else { // I type
            if (opcode[0] == 35) { // lw
                c->mem_to_reg = 1;
            }
        }
    }
    return;
}

uint32_t fetch(Regs* r, uint8_t* mem) {
    printf("\t[Instruction Fetch] ");
    uint32_t temp = 0;
    for (int i = 0; i < 4; i++) {
        temp = temp << 8;
        temp |= mem[r->pc + (3 - i)];
    }
    r->pc += 4;
    printf(" 0x%08x (PC=0x%08x)\n", temp, r->pc);
    return temp;
}

void decode(Regs* r, Ctrl* c, uint32_t inst, uint32_t* id2ex, uint32_t* addr_flow) {
    printf("\t[Instruction Decode] ");
    inst_cnt++;
    uint32_t inst_split[6] = { 0, };
    init_ctrl(c); // control 값 초기화
    inst_split[0] = inst >> 26; // opcode 부분, [31-26]
    if (inst_split[0] == 2 || inst_split[0] == 3) {
        inst_split[1] = inst & 0x3ffffff; // addr
    }
    else if (inst_split[0] == 0) {
        inst_split[1] = (inst >> 21) & 0x0000001f; // rs
        inst_split[2] = (inst >> 16) & 0x0000001f; // rt
        inst_split[3] = (inst >> 11) & 0x0000001f; // rd
        inst_split[4] = inst & 0x3f; // funct
        inst_split[5] = (inst >> 6) & 0x0000001f; // shamt
    }
    else {
        inst_split[1] = (inst >> 21) & 0x0000001f; // rs
        inst_split[2] = (inst >> 16) & 0x0000001f; // rt
        inst_split[3] = inst & 0xffff; // imm
    }

    printf("Type: ");
    if (inst_split[0] == 0) { // R type
        id2ex[1] = 0;
        printf("R, Inst: ");
        if (inst_split[4] == 0x08) {
            id2ex[1] = 3; // jr
            printf("jr r%d", inst_split[1]);
        }
        else if (inst_split[4] == 0x20) {
            printf("add r%d r%d r%d", inst_split[3], inst_split[2], inst_split[1]);
        }
        else if (inst_split[4] == 0x22) {
            printf("sub r%d r%d r%d", inst_split[3], inst_split[2], inst_split[1]);
        }
        else if (inst_split[4] == 0x24) {
            printf("and r%d r%d r%d", inst_split[3], inst_split[2], inst_split[1]);
        }
        else if (inst_split[4] == 0x25) {
            printf("or r%d r%d r%d", inst_split[3], inst_split[2], inst_split[1]);
        }
        else if (inst_split[4] == 0x27) {
            printf("nor r%d r%d r%d", inst_split[3], inst_split[2], inst_split[1]);
        }
        else if (inst_split[4] == 0x00) {
            printf("sll r%d r%d %d", inst_split[3], inst_split[2], inst_split[5]);
        }
        else if (inst_split[4] == 0x02) {
            printf("srl r%d r%d r%d", inst_split[3], inst_split[2], inst_split[5]);
        }
        else if (inst_split[4] == 0x2a) {
            printf("slt r%d r%d r%d", inst_split[3], inst_split[2], inst_split[1]);
        }
        else if (inst_split[4] == 0x2b) {
            printf("sltu r%d r%d r%d", inst_split[3], inst_split[2], inst_split[1]);
        }
        else if (inst_split[4] == 0x23) {
            printf("subu r%d r%d r%d", inst_split[3], inst_split[2], inst_split[1]);
        }
        else if (inst_split[4] == 0x21) {
            printf("addu r%d r%d r%d", inst_split[3], inst_split[2], inst_split[1]);
        }
        /*else if (inst_split[4] == 0x12) {
            printf("mflo r%d", inst_split[3]);
        }*/
        else if (inst_split[4] == 0x18) {
            printf("mult r%d r%d", inst_split[2], inst_split[1]);
        }
        r_cnt++;
    }
    else if (inst_split[0] == 2 || inst_split[0] == 3) { // J type
        id2ex[1] = 1;
        printf("J, Inst: ");
        if (inst_split[0] == 2) {
            printf("j 0x%x", inst_split[1]);
        }
        else if (inst_split[0] == 3) {
            printf("jal 0x%x", inst_split[1]);
        }
        j_cnt++;
    }
    else { // I type
        id2ex[1] = 2;
        printf("I, Inst: ");
        if (inst_split[0] == 4 || inst_split[0] == 5) { // beq bne
            id2ex[1] = 4;
            if (inst_split[0] == 4) {
                printf("beq r%d r%d %x", inst_split[2], inst_split[1], inst_split[3]);
            }
            else {
                printf("bne r%d r%d %x", inst_split[2], inst_split[1], inst_split[3]);
            }
        }
        else {
            if (inst_split[0] == 8) {
                printf("addi r%d r%d %d", inst_split[2], inst_split[1], inst_split[3]);
            }
            else if (inst_split[0] == 9) {
                printf("addiu r%d r%d %d", inst_split[2], inst_split[1], inst_split[3]);
            }
            else if (inst_split[0] == 10) {
                printf("slti r%d r%d %d", inst_split[2], inst_split[1], inst_split[3]);
            }
            else if (inst_split[0] == 11) {
                printf("sltiu r%d r%d %d", inst_split[2], inst_split[1], inst_split[3]);
            }
            else if (inst_split[0] == 12) {
                printf("andi r%d r%d %d", inst_split[2], inst_split[1], inst_split[3]);
            }
            else if (inst_split[0] == 13) {
                printf("ori r%d r%d %d", inst_split[2], inst_split[1], inst_split[3]);
            }
            else if (inst_split[0] == 35) {
                printf("lw r%d %d(r%d)", inst_split[2], inst_split[3], inst_split[1]);
            }
            else if (inst_split[0] == 43) {
                printf("sw r%d %d(r%d)", inst_split[2], inst_split[3], inst_split[1]);
            }
            else if (inst_split[0] == 0xf) {
                printf("lui r%d %d", inst_split[2], inst_split[3]);
            }
        }
        i_cnt++;
    }
    printf("\n\t    opcode: 0x%x, ", inst_split[0]);
    if (inst_split[0] == 0) { // R type
        if (inst_split[4] != 0x08 && inst_split[4] != 0x12 && inst_split[4] != 0x18) {
            printf(", rs: %d (0x%x), rt: %d (0x%x), rd: %d (0x%x)", inst_split[1], r->reg[inst_split[1]], inst_split[2], r->reg[inst_split[2]], inst_split[3], r->reg[inst_split[3]]);
            if (inst_split[4] == 0 || inst_split[4] == 2) {
                printf(", shmat: %d", inst_split[5]);
            }
        }
        else if (inst_split[4] == 0x08) { // jr
            printf(", rs: %d (0x%x)", inst_split[1], r->reg[inst_split[1]]);
        }
        else if (inst_split[4] == 0x12) { // mflo
            printf(", rd: %d (0x%x)", inst_split[3], r->reg[inst_split[3]]);
        }
        else if (inst_split[4] == 0x18) { // mult
            printf(", rs: %d (0x%x), rt: %d (0x%x)", inst_split[1], r->reg[inst_split[1]], inst_split[2], r->reg[inst_split[2]]);
        }
        printf(", funct: 0x%x", inst_split[4]);
    }
    else if (inst_split[0] == 2 || inst_split[0] == 3) { // J type
        printf(", imm: %d", inst_split[1]);
    }
    else if (inst_split[0] == 0xf) {
        printf(", rt: %d (0x%x)", inst_split[2], r->reg[inst_split[2]]);
    }
    else {
        printf(", rs: %d (0x%x), rt: %d (0x%x), imm: %d", inst_split[1], r->reg[inst_split[1]], inst_split[2], r->reg[inst_split[2]], inst_split[3]);
    }
    printf("\n");

    int rd, rw, alusrc, pcsrc, mread, mwrite, memtoreg, aluop;
    rd = (inst_split[0] == 0) ? 1 : 0;
    rw = ((inst_split[0] == 0) || (inst_split[0] == 35)) ? 1 : 0; // lw r type만
    alusrc = ((inst_split[0] == 43) || (inst_split[0] == 35)) ? 1 : 0; // lw sw 만
    pcsrc = ((inst_split[0] >= 2) && (inst_split[0] <= 5)) ? 1 : 0; // branch 및 jump
    mread = (inst_split[0] == 35) ? 1 : 0; // lw만
    mwrite = (inst_split[0] == 43) ? 1 : 0; // sw만
    memtoreg = ((inst_split[0] == 0xf) || (inst_split[0] == 35)) ? 1 : 0; // lui, lw
    aluop = 0;
    if ((inst_split[0] == 43) || (inst_split[0] == 35)) { // lw sw
        aluop = 0;
    }
    else if ((inst_split[0] == 4) || (inst_split[0] == 5)) { // branch
        aluop = 0b01;
    }
    else if (inst_split[0] == 0) { // rtype
        aluop = 0b10;
    }
    printf("\t    RegDst: %d, RegWrite: %d, ALUSrc: %d, PCSrc: %d, MemRead: %d, MemWrite: %d, MemtoReg: %d, ALUOp: %d\n", rd, rw, alusrc, pcsrc, mread, mwrite, memtoreg, aluop);

    id2ex[0] = inst_split[0]; // opcode
    if (id2ex[1] == 0) {
        id2ex[2] = inst_split[4]; // funct
        id2ex[3] = inst_split[1]; // read data1 : rs !!
        id2ex[4] = inst_split[2]; // read data2 : rt !!
        id2ex[6] = inst_split[3]; // write reg !!
        if (id2ex[2] == 0x0 || id2ex[2] == 0x2) {
            id2ex[7] = inst_split[5]; // shamt
        }
    }
    else if (id2ex[1] == 1) {
        addr_flow[0] = inst_split[1]; // jump address
    }
    else if (id2ex[1] == 2) { // i type
        id2ex[3] = inst_split[1]; // read data1 : rs !!
        id2ex[5] = inst_split[3]; // imm
        id2ex[6] = inst_split[2]; // write reg : rt !!
        if (id2ex[0] == 43) {
            id2ex[4] = inst_split[2];
        }
    }
    else if (id2ex[1] == 3) { // jr rs
        id2ex[3] = inst_split[1]; // read data1 : rs !!
    }
    else { // branch.
        id2ex[3] = inst_split[1]; // read data1 : rs !!
        id2ex[4] = inst_split[2]; // read data2 : rt !!
        id2ex[5] = inst_split[3]; // imm 그대로 넣어줌.
        b_cnt++;
    }

    // sign extend
    if (id2ex[1] == 2) {
        if (id2ex[0] == 0xc || id2ex[0] == 0xd || (id2ex[5] >> 15) == 0) {
            id2ex[5] = (id2ex[5] & 0x0000ffff);
        }
        else if ((id2ex[5] >> 15) == 1) { // sign-extend
            id2ex[5] = (id2ex[5] | 0xffff0000);
        }
    }
    else if (id2ex[1] == 1) { // jump-addr
        addr_flow[0] <<= 2;
    }
    else if (id2ex[1] == 4) {
        if ((id2ex[5] >> 15) == 1) { // branch-addr
            id2ex[5] = ((id2ex[5] << 2) | 0xfffc0000);
        }
        else if ((id2ex[5] >> 15) == 0) {
            id2ex[5] = ((id2ex[5] << 2) & 0x3ffc);
        }
    }

    addr_flow[1] = r->pc; // pc+4 값 저장.
}

uint32_t alu(Ctrl* c, uint32_t oper1, uint32_t oper2) {
    uint32_t temp = 0;
    if (c->alu_ctrl == 0b0000) { // and
        temp = oper1 & oper2;
    }
    else if (c->alu_ctrl == 0b0001) { // or
        temp = oper1 | oper2;
    }
    else if (c->alu_ctrl == 0b0010) { // add
        temp = oper1 + oper2;
    }
    else if (c->alu_ctrl == 0b0110) { // sub
        temp = oper1 - oper2;
    }
    else if (c->alu_ctrl == 0b1001) { // mult
        uint64_t temp;
        temp = (uint64_t)oper1 * (uint64_t)oper2;
        hi = temp & 0xffffffff00000000;
        lo = temp & 0xffffffff;
        temp = 0;
    }
    else if (c->alu_ctrl == 0b1100) { // nor
        temp = ~(oper1 | oper2);
    }
    else if (c->alu_ctrl == 0b0111) { // slt
        temp = (oper2 > oper1) ? 1 : 0;
    }
    else if (c->alu_ctrl == 0b1110) { // sll
        temp = oper1 << oper2;
    }
    else if (c->alu_ctrl == 0b1111) { // srl
        temp = oper1 >> oper2;
    }
    return temp;
}

void execute(Regs* r, Ctrl* c, uint32_t* id2ex, uint32_t* ex2mem, uint32_t* addr_flow) {
    printf("\t[Execute] ");
    ex2mem[0] = id2ex[0];
    ex2mem[1] = id2ex[1];
    // jump들 모두 먼저 처리.
    if (id2ex[1] == 1) {
        if (id2ex[0] == 3) { // jal
            r->reg[31] = addr_flow[1] + 4;
            r->pc = addr_flow[0];
            printf("Pass\n");
            return;
        }
        else if (id2ex[0] == 2) { // j
            r->pc = addr_flow[0];
            printf("Pass\n");
            return;
        }
    }
    else if (id2ex[1] == 3) { // jr
        r->pc = r->reg[id2ex[3]];
        printf("Pass\n");
        return;
    }

    if (id2ex[0] == 0xf) { // lui
        r->reg[id2ex[6]] = (id2ex[5] << 16) & 0xffff0000;
        printf("Pass\n");
        return;
    }

    if (id2ex[1] == 0 && id2ex[2] == 0x12) { // mflo
        r->reg[id2ex[6]] = lo;
        ex2mem[1] = 5;
        printf("Pass\n");
        return;
    }

    init_ctrl(c);
    path_ctrl(c, id2ex, 2);

    if (c->ex_skip == 1) {
        ex2mem[4] = id2ex[6];
        return;
    }

    printf("ALU = ");
    if (id2ex[1] == 0) { // r type exe
        if (id2ex[2] == 0x00 || id2ex[2] == 0x02) { // sll srl
            ex2mem[2] = alu(c, r->reg[id2ex[4]], id2ex[7]);
        }
        else {
            ex2mem[2] = alu(c, r->reg[id2ex[3]], r->reg[id2ex[4]]); // 그외 rtype
        }
        ex2mem[4] = id2ex[6]; // write reg
        ex2mem[3] = id2ex[4]; // read data2
    }
    else if (id2ex[1] == 2) { // i type exe
        ex2mem[2] = alu(c, r->reg[id2ex[3]], id2ex[5]);
        ex2mem[4] = id2ex[6]; // write reg
        if (id2ex[0] == 43) {
            ex2mem[3] = r->reg[id2ex[4]];
        }
    }
    else if (id2ex[1] == 4) { // beq bne exe
        uint32_t temp;
        addr_flow[2] = id2ex[5];
        if (id2ex[0] == 4) { // beq
            temp = alu(c, r->reg[id2ex[3]], r->reg[id2ex[4]]);
            if (temp == 0) {
                r->pc = addr_flow[1] + addr_flow[2]; // pc + 4 + branch addr
                printf("%d\n", (temp == 0));
                return;
            }
            else {
                r->pc = addr_flow[1]; // pc + 4
                printf("%d\n", (temp == 0));
                return;
            }
        }
        else if (id2ex[0] == 5) { // bne
            temp = alu(c, r->reg[id2ex[3]], r->reg[id2ex[4]]);
            if (temp != 0) {
                r->pc = addr_flow[1] + addr_flow[2]; // pc + 4 + branch addr
                printf("%d\n", (temp != 0));
                return;
            }
            else {
                r->pc = addr_flow[1]; // pc + 4
                printf("%d\n", (temp != 0));
                return;
            }
        }
    }
    printf("0x%x\n", ex2mem[2]);
}

void mem_access(Ctrl* c, uint32_t* ex2mem, uint32_t* mem2wb) {
    printf("\t[Memory Access] ");
    mem2wb[0] = ex2mem[0];
    mem2wb[1] = ex2mem[1];
    init_ctrl(c);
    path_ctrl(c, ex2mem, 3);

    if (c->ma_skip) { // ma_skip : mflo
        if (ex2mem[1] == 0) {
            mem2wb[4] = ex2mem[4];
        }
        printf("Pass\n");
        return;
    }

    if (c->mem_rd == 1) { // lw
        printf("Load, Address: 0x%x", ex2mem[2]);
        ma_cnt++;
        uint32_t temp = 0;
        for (int j = 3; j >= 0; j--) {
            temp <<= 8;
            temp |= mem[ex2mem[2] + j];
        }
        mem2wb[2] = temp; // lw로 저장해야되는값.
        mem2wb[4] = ex2mem[4]; // write reg 인덱스
        printf(", Value: 0x%x", mem2wb[2]);
    }
    else if (c->mem_wr == 1) { // sw
        printf("Store, Address: 0x%x, Value: 0x%x", ex2mem[2], ex2mem[3]);
        ma_cnt++;
        for (int i = 0; i < 4; i++) {
            mem[ex2mem[2] + i] = (ex2mem[3] >> 8 * i) & 0xff;
        }
    }
    else {
        printf("pass");
        mem2wb[3] = ex2mem[2]; // alu result 를 mem2wb[3]으로
        mem2wb[4] = ex2mem[4]; // write reg 인덱스
    }
    printf("\n");
    return;
}

void write_back(Regs* r, Ctrl* c, uint32_t* mem2wb) {
    printf("\t[Write Back] newPC: 0x%x", r->pc + 4);
    init_ctrl(c);
    path_ctrl(c, mem2wb, 4);
    if (c->reg_wr == 0) { // sw를 포함한 wb단계 배제
        return;
    }
    if (c->mem_to_reg == 1) { // lw의 경우
        r->reg[mem2wb[4]] = mem2wb[2];
    }
    else { // R type alu
        r->reg[mem2wb[4]] = mem2wb[3];
    }
}

void init_latch(uint32_t* inst, uint32_t* addr_flow, uint32_t* id2ex, uint32_t* ex2mem, uint32_t* mem2wb) {
    *(inst) = 0;
    for (int i = 0; i < 3; i++) {
        addr_flow[i] = 0;
    }
    for (int i = 0; i < 8; i++) {
        id2ex[i] = 0;
    }
    for (int i = 0; i < 5; i++) {
        ex2mem[i] = 0;
    }
    for (int i = 0; i < 5; i++) {
        mem2wb[i] = 0;
    }
}

void datapath(Regs* r, uint8_t* mem) {
    uint32_t inst = 0x00;
    uint32_t addr_flow[3];
    uint32_t id_ex[8]; // latch
    uint32_t ex_mem[5]; // latch
    uint32_t mem_wb[5]; // latch
    Ctrl c;
    init_ctrl(&c);

    while (r->pc != 0xffffffff) {
        printf("32203743> Cycle : %d\n", inst_cnt);
        inst = fetch(r, mem);
        if (inst == 0) {
            printf("\tNOP\n\n");
            continue;
        }
        decode(r, &c, inst, id_ex, addr_flow);
        execute(r, &c, id_ex, ex_mem, addr_flow);
        mem_access(&c, ex_mem, mem_wb);
        write_back(r, &c, mem_wb);
        printf("\n\n");
        init_latch(&inst, addr_flow, id_ex, ex_mem, mem_wb);
    }

    printf("32203743> Final Result");
    printf("\tCycles: %d, R-type instructions: %d, I-type instructions: %d, J-type instructions: %d\n \tReturn value(v0) : %d\n", inst_cnt, r_cnt, i_cnt, j_cnt, r->reg[2]);

    return;
}

void init_regs(Regs* r) {
    r->pc = 0;
    for (int i = 0; i < 32; i++) {
        r->reg[i] = 0;
    }
    r->reg[31] = 0xffffffff;
    r->reg[29] = 0x1000000;
}
