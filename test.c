#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define MAX_MEMORY 0x1000000

uint8_t Memory[MAX_MEMORY];


FILE* fp;

int inst_count = 0;
int r_count = 0;
int i_count = 0;
int j_count = 0;
int sw_count = 0;
int lw_count = 0;
int branch_jr_count = 0;
int nop_count = 0;
int write_reg_count = 0;


// Register
typedef struct {

    uint32_t reg[32];
    uint32_t pc;

}Registers;




// Control unit
// control if 는 
// Control_id 는 regDst 밖에 없으므로 fetch 함수에서 처리

typedef struct {
    int alu_ctrl;       // alu_control
    int alu_op;        // alu_op
    int alu_src;       // alu_src

    int mem_read;
    int mem_write;

    int mem_to_reg;
    int reg_wb;
    int reg_dst;
    int get_imm;
    int ex_skip;
    int rt_ch;
    int rs_ch;

}Control;


typedef struct {
    uint32_t next_pc;    // next_pc = pc + 4
    uint32_t inst;       // instruction

    // 아래 두 변수는 lw-use hazard 를 위해서 사용된다.
    uint32_t reg_src;       // register source  :   [25-21]
    uint32_t reg_tar;       // register tar     :   [20-16]
    uint32_t opcode;       // opcode
    uint32_t funct;

    // branch & jr forward
    int forward_a;          
    int forward_b;
    uint32_t forward_a_val;
    uint32_t forward_b_val;


}if_id;

// Latch structures
typedef struct {
    uint32_t reg_src;       // register source  :   [25-21]
    uint32_t reg_tar;       // register tar     :   [20-16]
    uint32_t reg_write;     // register write : reg_tar or reg_dst
    uint32_t sign_imm;      // sign immdieate
    uint32_t shamt;      // sign immdieate
    uint32_t read_reg1;
    uint32_t read_reg2;
    Control* ctrl;           // control unit

    // ex forward
    int forward_a;
    int forward_b;
    uint32_t forward_a_val;
    uint32_t forward_b_val;

    
}id_ex;


typedef struct {
    
    uint32_t alu_res;       // alu_result
    uint32_t reg_write;     // register write : reg_tar or reg_dst
    uint32_t mem_val;       // reg_tar for sw
    Control* ctrl;          // control unit

}ex_mem;



typedef struct {
    
    uint32_t alu_res;       // alu_result
    uint32_t mem_read;      // memory read
    uint32_t reg_write;      // register write
    Control* ctrl;           // control unit

}mem_wb;


// control init 모음

void init_ctrl(Control* ctrl) {
    ctrl->alu_ctrl = 0;
    ctrl->alu_op = 0;
    ctrl->alu_src = 0;
    ctrl->mem_read = 0;
    ctrl->mem_write = 0;
    ctrl->mem_to_reg = 0;
    ctrl->reg_wb = 0;
    ctrl->reg_dst = 0;
    ctrl->get_imm = 0;
    ctrl->ex_skip = 0;
    ctrl->rs_ch = 0;
    ctrl->rt_ch = 0;
}


// init_latch 모음

void init_ifid(if_id* fd) {
    fd->next_pc = 0;       // register source  :   [25-21]
    fd->inst = 0;       // register tar     :   [20-16]
    fd->reg_src = 0;
    fd->reg_tar = 0;
    fd->opcode = 0;

    fd->funct = 0;
    fd->forward_a = 0;
    fd->forward_b = 0;
    fd->forward_a_val = 0;
    fd->forward_b_val = 0;
}


void init_idex(id_ex* ie) {

    ie->reg_src = 0;
    ie->reg_tar = 0;
    ie->reg_write = 0;
    ie->sign_imm = 0;
    ie->shamt = 0;
    ie->read_reg1 = 0;
    ie->read_reg2 = 0;
    ie->forward_a = 0;
    ie->forward_b = 0;
    ie->forward_a_val = 0;
    ie->forward_b_val = 0;

    // control init
    init_ctrl(ie->ctrl);
}


void init_exmem(ex_mem* em) {
    em->alu_res = 0;
    em->reg_write = 0;
    em->mem_val = 0;

    // control init
    init_ctrl(em->ctrl);
}


void init_memwb(mem_wb* mw) {
    mw->alu_res = 0;
    mw->mem_read = 0;
    mw->reg_write = 0;

    // control init
    init_ctrl(mw->ctrl);
}



void set_ctrl(id_ex* id_ex, uint32_t opcode, uint32_t funct) {
    // 초기화 모든 제어 신호를 0으로 설정
    init_ctrl(id_ex->ctrl);

    // opcode 및 funct에 따른 제어 신호 설정
    switch (opcode) {
    case 0x2:  // j
        //printf( "j   ");
        id_ex->ctrl->ex_skip = 1;
        break;
    case 0x3:  // jal
        //printf( "jal   ");
        id_ex->ctrl->ex_skip = 1;
        break;
    case 0x4:  // beq
        //printf( "beq   ");
        id_ex->ctrl->ex_skip = 1;
        id_ex->ctrl->rt_ch = 1;
        id_ex->ctrl->rs_ch = 1;
        break;
    case 0x5:  // bne
        //printf( "bne   ");
        id_ex->ctrl->ex_skip = 1;
        id_ex->ctrl->rt_ch = 1;
        id_ex->ctrl->rs_ch = 1;
        break;
    case 0x8:  // addi
        //printf( "addi   ");
        id_ex->ctrl->reg_wb = 1;
        id_ex->ctrl->get_imm = 1;
        id_ex->ctrl->alu_ctrl = 0b0010;
        id_ex->ctrl->alu_src = 1;
        id_ex->ctrl->rs_ch = 1;
        break;
    case 0x9:  // addiu
        //printf( "addiu   ");
        id_ex->ctrl->reg_wb = 1;
        id_ex->ctrl->get_imm = 1;
        id_ex->ctrl->alu_ctrl = 0b0010;
        id_ex->ctrl->alu_src = 1;
        id_ex->ctrl->rs_ch = 1;
        break;
    case 0xA:  // slti
        //printf( "slti   ");
        id_ex->ctrl->get_imm = 1;
        id_ex->ctrl->alu_ctrl = 0b0111;
        id_ex->ctrl->alu_src = 1;
        id_ex->ctrl->reg_wb = 1;
        id_ex->ctrl->rs_ch = 1;
        break;
    case 0xB:  // sltiu
        //printf( "sltiu   ");
        id_ex->ctrl->get_imm = 1;
        id_ex->ctrl->alu_ctrl = 0b0111;
        id_ex->ctrl->alu_src = 1;
        id_ex->ctrl->reg_wb = 1;
        id_ex->ctrl->rs_ch = 1;
        break;
    case 0xC:  // andi
        //printf( "andi   ");
        id_ex->ctrl->alu_ctrl = 0b0000;
        id_ex->ctrl->alu_src = 1;
        id_ex->ctrl->reg_wb = 1;
        id_ex->ctrl->get_imm = 2;
        id_ex->ctrl->rs_ch = 1;
        break;
    case 0xD:  // ori
        //printf( "ori   ");
        id_ex->ctrl->alu_ctrl = 0b0001;
        id_ex->ctrl->alu_src = 1;
        id_ex->ctrl->reg_wb = 1;
        id_ex->ctrl->get_imm = 2;
        id_ex->ctrl->rs_ch = 1;
        break;
    case 0x23:  // lw
        //printf( "lw   ");
        id_ex->ctrl->reg_wb = 1;
        id_ex->ctrl->get_imm = 1;
        id_ex->ctrl->alu_ctrl = 0b0010;
        id_ex->ctrl->alu_src = 1;
        id_ex->ctrl->mem_read = 1;
        id_ex->ctrl->mem_to_reg = 1;
        id_ex->ctrl->rs_ch = 1;
        break;
    case 0x2B:  // sw
        //printf( "sw   ");
        id_ex->ctrl->get_imm = 1;
        id_ex->ctrl->alu_ctrl = 0b0010;
        id_ex->ctrl->alu_src = 1;
        id_ex->ctrl->mem_write = 1;
        id_ex->ctrl->rt_ch = 1;
        id_ex->ctrl->rs_ch = 1;
        break;
    case 0xF:  // lui
        //printf( "lui   ");
        id_ex->ctrl->get_imm = 3;
        id_ex->ctrl->reg_dst = 0;
        id_ex->ctrl->alu_src = 0;
        id_ex->ctrl->ex_skip = 0;
        id_ex->ctrl->reg_wb = 1;
        break;
    case 0x0:  // R-type
        switch (funct) {
        case 0x20:  // Add
            //printf( "Add   ");
            id_ex->ctrl->reg_wb = 1;
            id_ex->ctrl->reg_dst = 1;
            id_ex->ctrl->alu_ctrl = 0b0010;
            id_ex->ctrl->rt_ch = 1;
            id_ex->ctrl->rs_ch = 1;
            break;
        case 0x22:  // Subtract
            //printf( "Subtract   ");
            id_ex->ctrl->reg_wb = 1;
            id_ex->ctrl->reg_dst = 1;
            id_ex->ctrl->alu_ctrl = 0b0110;
            id_ex->ctrl->rt_ch = 1;
            id_ex->ctrl->rs_ch = 1;
            break;
        case 0x24:  // And
            //printf( "And   ");
            id_ex->ctrl->reg_wb = 1;
            id_ex->ctrl->reg_dst = 1;
            id_ex->ctrl->alu_ctrl = 0b0000;
            id_ex->ctrl->rt_ch = 1;
            id_ex->ctrl->rs_ch = 1;
            break;
        case 0x25:  // Or
            //printf( "Or   ");
            id_ex->ctrl->reg_wb = 1;
            id_ex->ctrl->reg_dst = 1;
            id_ex->ctrl->alu_ctrl = 0b0001;
            id_ex->ctrl->rt_ch = 1;
            id_ex->ctrl->rs_ch = 1;
            break;
        case 0x27:  // Nor
            //printf( "Nor   ");
            id_ex->ctrl->reg_wb = 1;
            id_ex->ctrl->reg_dst = 1;
            id_ex->ctrl->alu_ctrl = 0b1100;
            id_ex->ctrl->rt_ch = 1;
            id_ex->ctrl->rs_ch = 1;
            break;
        case 0x00:  // SLL
            //printf( "SLL   ");
            id_ex->ctrl->reg_wb = 1;
            id_ex->ctrl->reg_dst = 1;
            id_ex->ctrl->alu_ctrl = 0b1110;
            id_ex->ctrl->rt_ch = 1;
            break;
        case 0x02:  // SRL
            //printf( "SRL   ");
            id_ex->ctrl->reg_wb = 1;
            id_ex->ctrl->reg_dst = 1;
            id_ex->ctrl->alu_ctrl = 0b1111;
            id_ex->ctrl->rt_ch = 1;
            break;
        case 0x2A:  // SLT
            //printf( "SLT   ");
            id_ex->ctrl->reg_wb = 1;
            id_ex->ctrl->reg_dst = 1;
            id_ex->ctrl->alu_ctrl = 0b0111;
            id_ex->ctrl->rt_ch = 1;
            id_ex->ctrl->rs_ch = 1;
            break;
        case 0x2B:  // SLTU
            //printf( "SLTU   ");
            id_ex->ctrl->reg_wb = 1;
            id_ex->ctrl->reg_dst = 1;
            id_ex->ctrl->alu_ctrl = 0b0111;
            id_ex->ctrl->rt_ch = 1;
            id_ex->ctrl->rs_ch = 1;
            break;
        case 0x08:  // JR
            //printf( "JR   ");
            id_ex->ctrl->ex_skip = 1;
            id_ex->ctrl->reg_dst = 1;
            id_ex->ctrl->rs_ch = 1;
            break;
        case 0x23:  // Subu
            //printf( "Subu   ");
            id_ex->ctrl->reg_wb = 1;
            id_ex->ctrl->reg_dst = 1;
            id_ex->ctrl->alu_ctrl = 0b0110;
            id_ex->ctrl->rt_ch = 1;
            id_ex->ctrl->rs_ch = 1;
            break;
        case 0x21:  // Addu
            //printf( "Addu   ");
            id_ex->ctrl->reg_wb = 1;
            id_ex->ctrl->reg_dst = 1;
            id_ex->ctrl->alu_ctrl = 0b0010;
            id_ex->ctrl->rt_ch = 1;
            id_ex->ctrl->rs_ch = 1;
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }
}




void copy_control(Control* dest, const Control* src) {
    dest->alu_ctrl = src->alu_ctrl;
    dest->alu_op = src->alu_op;
    dest->alu_src = src->alu_src;
    dest->mem_read = src->mem_read;
    dest->mem_write = src->mem_write;
    dest->mem_to_reg = src->mem_to_reg;
    dest->reg_wb = src->reg_wb;
    dest->reg_dst = src->reg_dst;
    dest->get_imm = src->get_imm;
    dest->ex_skip = src->ex_skip;
    dest->rt_ch = src->rt_ch;
    dest->rs_ch = src->rs_ch;
}



// 
void fetch(Registers* r, uint8_t* mem, if_id* if_id) {
    inst_count++;
    init_ifid(if_id);

    //printf( "[fetch] \t\t");
    uint32_t temp = 0;              // 리틀엔디안으로 저장된 inst 값 추출
    for (int i = 0; i < 4; i++) {
        temp = temp << 8;
        temp |= mem[r->pc + (3 - i)];
    }

    if_id->next_pc = r->pc + 4;
    if_id->inst = temp;

    if_id->opcode = if_id->inst >> 26;                                 // opcode 부분, [31-26]
    if_id->reg_src = (if_id->inst >> 21) & 0x0000001f;          // rs
    if_id->reg_tar = (if_id->inst >> 16) & 0x0000001f;          // rt
    if_id->funct = if_id->inst & 0x3f;                                 // funct
    //printf( "opcode : %x,   reg_tar : %d,   reg_tar : %d,   funct :   %d\n", if_id->opcode, if_id->reg_src, if_id->reg_tar, if_id->funct);
}


void decode(Registers* r, if_id* if_id, id_ex* id_ex) {

    //printf( "[decode start] \t");

    init_idex(id_ex);

    // instruction split
    uint32_t opcode, funct = 0, jaddr = 0;
    opcode = if_id->opcode;                                 // opcode 부분, [31-26]
    funct = if_id->funct;                                 // funct
    // 1. opcode 통해 control logic 배열하기.
    set_ctrl(id_ex, opcode, funct);


    // 2. reg_src, reg_tar, rd, imm, shamt 다음 latch로 이동
    id_ex->reg_src = if_id->reg_src;          // rs
    id_ex->reg_tar = if_id->reg_tar;          // rt

    if (id_ex->ctrl->reg_dst == 1) {                                // r-type
        r_count++;
        id_ex->reg_write = (if_id->inst >> 11) & 0x0000001f;        // rd
        if (funct != 0x8) {
            //printf( "R%d  ", id_ex->reg_write);
            //printf( "R%d  ", id_ex->reg_tar);
        }

        if (funct == 0x00 || funct == 0x02) {
            id_ex->shamt = (if_id->inst >> 6) & 0x0000001f;             // shamt
            //printf( "%d", id_ex->shamt);
        }
        else if (funct != 0x8) {
            //printf( "R%d  ", id_ex->reg_src);
        }
    }

    // 3. j-type addr :  jaddr 구하기
    if (opcode == 0x2 || opcode == 0x3) {
        jaddr = if_id->inst & 0x3ffffff;
    }


    // 4. zero immediate, sign immediate, lui immediate
    if (id_ex->ctrl->get_imm != 0) {
        i_count++;
        id_ex->sign_imm = if_id->inst & 0xffff;                     // imm
        id_ex->reg_write = if_id->reg_tar;
        //printf( "R%d  ", id_ex->reg_tar);
        //printf( "R%d  ", id_ex->reg_src);


        if (id_ex->ctrl->get_imm == 1) {
            if ((id_ex->sign_imm >> 15) == 0) {
                id_ex->sign_imm = (id_ex->sign_imm & 0x0000ffff);
            }
            else {
                id_ex->sign_imm = (id_ex->sign_imm | 0xffff0000);
            }
        }
        else if (id_ex->ctrl->get_imm == 2) {
            id_ex->sign_imm = (id_ex->sign_imm & 0x0000ffff);
        }
        else if (id_ex->ctrl->get_imm == 3) {
            id_ex->sign_imm = id_ex->sign_imm << 16;
        }


        //if (id_ex->ctrl->get_imm == 2 || (id_ex->sign_imm >> 15) == 0) {            // zero ext & sign extend(양수)
        //    id_ex->sign_imm = (id_ex->sign_imm & 0x0000ffff);
        //}
        //else if ((id_ex->sign_imm >> 15) == 1) {                                    // sign extend(음수)
        //    id_ex->sign_imm = (id_ex->sign_imm | 0xffff0000);
        //}
        //else if (id_ex->ctrl->get_imm == 3) {       // lui imm
        //    id_ex->sign_imm = (id_ex->sign_imm << 16) & 0xffff0000;                 // lui immediate
        //}
        //printf( "0x%x  ", id_ex->sign_imm);
    }

    // 5. branch, j, jal, jr 
    if (id_ex->ctrl->ex_skip == 1) {
        branch_jr_count++;
        // branch
        if (opcode == 0x4 || opcode == 0x5) {        // beq, bne
            // 5-1.  branch 주소 계산 및 분기 여부 파악
            
            uint32_t oper1 = (if_id->forward_a >= 1) ? if_id->forward_a_val : r->reg[id_ex->reg_src];
            uint32_t oper2 = (if_id->forward_b >= 1) ? if_id->forward_b_val : r->reg[id_ex->reg_tar];

            int beq_bne = (opcode == 0x4) ? 1 : 0;   // beq = 1, bne = 0
            int check = (oper1 == oper2);      // xor : 동등 여부 파악
            int taken = (check == beq_bne);                                      // xor : 동등 여부 파악
            //printf( "[ 0x%x   0x%x ] :  ", oper1, oper2);
            if (taken) {
                // taken
                //printf( "taken");
                
                id_ex->sign_imm = if_id->inst & 0xffff;                     // imm
                uint32_t branchaddr = id_ex->sign_imm;

                if ((id_ex->sign_imm >> 15) == 1) { // branch-addr
                    branchaddr = ((id_ex->sign_imm << 2) | 0xfffc0000);
                }
                else if ((id_ex->sign_imm >> 15) == 0) {
                    branchaddr = ((id_ex->sign_imm << 2) & 0x3ffc);
                }
                r->pc = r->pc + branchaddr;        // pc = pc + 4 + branchaddr
                //printf( "\n");
                return;
            }
            else {
                //printf( "not taken");
                // not taken
                //printf( "\n");
                return;
            }
        }   // 5-2. j inst exe
        else if (opcode == 0x2) {   // j
            jaddr <<= 2;
            r->pc = jaddr;
            //printf( "j   0x%x", r->pc);

        }   // 5-3 jal inst exe
        else if (opcode == 0x3) {   // jal
            jaddr <<= 2;
            r->reg[31] = r->pc + 4; // pc+8
            r->pc = jaddr;
            //printf( "jal   0x%x", r->pc + 4);

        }   // 5-4 jr inst exe
        else {                      // jr
            uint32_t oper1 = (if_id->forward_a >= 1) ? if_id->forward_a_val : r->reg[id_ex->reg_src];
            r->pc = oper1;
            //printf( "   R%d : 0x%x", id_ex->reg_src, oper1);
        }
        
        //printf( "\n");

        return;
    }
    ////printf( "jal : to %x and store r[31] = pc + 4 \n\n", r->pc);
    ////printf( "j : %x\n\n", r->pc);
    ////printf( "jr : move to %x\n\n", r->pc);



    // 6. read reg 값 넣어주기.
    id_ex->read_reg1 = r->reg[id_ex->reg_src];      // rs 값 read data1
    id_ex->read_reg2 = r->reg[id_ex->reg_tar];      // rt 값 read data2
    //printf( "\n");

    return;
}



uint32_t alu(id_ex* id_ex, uint32_t oper1, uint32_t oper2) {
    uint32_t temp = 0;
    //printf( "ALU : ");

    if (id_ex->ctrl->alu_ctrl == 0b0000) {             // and
        //printf( "[ and  :  0x%x & 0x%x ]", oper1, oper2);
        temp = oper1 & oper2;
    }
    else if (id_ex->ctrl->alu_ctrl == 0b0001) {        // or
        //printf( "[ or   :  0x%x | 0x%x ]", oper1, oper2);
        temp = oper1 | oper2;
    }
    else if (id_ex->ctrl->alu_ctrl == 0b0010) {        // add
        //printf( "[ add  :  0x%x + 0x%x ]", oper1, oper2);
        temp = oper1 + oper2;
    }
    else if (id_ex->ctrl->alu_ctrl == 0b0110) {        // sub
        //printf( "[ sub  :  0x%x - 0x%x ]", oper1, oper2);
        temp = oper1 - oper2;
    }
    else if (id_ex->ctrl->alu_ctrl == 0b1100) {        // nor
        //printf( "[ nor  :  ~(0x%x | 0x%x) ]", oper1, oper2);
        temp = ~(oper1 | oper2);
    }
    else if (id_ex->ctrl->alu_ctrl == 0b0111) {        // slt
        //printf( "[ slt  :  0x%x < 0x%x ? 1 : 0 ]", oper1, oper2);
        temp = (oper2 > oper1) ? 1 : 0;
    }
    else if (id_ex->ctrl->alu_ctrl == 0b1110) {        // sll
        //printf( "[ sll  :  0x%x << 0x%x ]", oper1, oper2);
        temp = oper1 << oper2;
    }
    else if (id_ex->ctrl->alu_ctrl == 0b1111) {        // srl
        //printf( "[ srl  :  0x%x >> 0x%x ]", oper1, oper2);
        temp = oper1 >> oper2;
    }

    return temp;
}




void execute(id_ex* id_ex, ex_mem* ex_mem) {

    // ex_skip 먼저.
    //printf( "[execute] \t");
    
    // 1. ex_mem latch init
    init_exmem(ex_mem);
    copy_control(ex_mem->ctrl, id_ex->ctrl);

    if (id_ex->ctrl->get_imm == 3) {
        //printf( "lui  R[%d]  0x%x\n", id_ex->reg_write, id_ex->sign_imm);
        ex_mem->alu_res = id_ex->sign_imm;
        ex_mem->reg_write = id_ex->reg_write;

        return;
    }


    // 2. decode에서 처리된 명령어들 skip
    if (id_ex->ctrl->ex_skip != 0) {
        //printf( "skip\n");
        return;
    }

    //if (id2ex[0] == 0xf) {              // lui
    //    r->reg[id2ex[6]] = (id2ex[5] << 16) & 0xffff0000;
    //    //printf( "lui : lui val %x -> r[%d]\n\n", r->reg[id2ex[6]], id2ex[6]);
    //    return;
    //}

    

    // 4. reg_write 값 id_ex에서 ex_mem 으로 옮겨주기
    if (id_ex->ctrl->reg_wb == 1) {         // reg_write 옮기기
        ex_mem->reg_write = id_ex->reg_write;
    }


    // 5. alu 계산
    uint32_t oper1 = (id_ex->forward_a >= 1) ? id_ex->forward_a_val : id_ex->read_reg1;     // rs

    // r type
    if (id_ex->ctrl->reg_dst == 1) {
        uint32_t oper2 = (id_ex->forward_b >= 1) ? id_ex->forward_b_val : id_ex->read_reg2; // rt

        // sll srl
        if (id_ex->ctrl->alu_ctrl >= 0b1110) {
            ex_mem->alu_res = alu(id_ex, oper2, id_ex->shamt);
        }
        // 그 외 r type
        else {
            ex_mem->alu_res = alu(id_ex, oper1, oper2);
        }
    }
    else if (id_ex->ctrl->mem_write == 1) {
        ex_mem->alu_res = alu(id_ex, oper1, id_ex->sign_imm);
        ex_mem->mem_val = (id_ex->forward_b >= 1) ? id_ex->forward_b_val : id_ex->read_reg2;
    }
    // itype
    else {
        uint32_t oper2 = id_ex->sign_imm; // imm
        ex_mem->alu_res = alu(id_ex, oper1, oper2);

        
    }

    //printf( "    \tALU result : 0x%x\n", ex_mem->alu_res);


    /*//printf( "alu : [ %x shift %d ] \t", r->reg[id2ex[4]], id2ex[7]);
    //printf( "alu : [ %x\t %x ] \t", r->reg[id2ex[3]], id2ex[5]);
    //printf( "beq : [ %x\t%x ] \t", r->reg[id2ex[3]], r->reg[id2ex[4]]);
    //printf( "beq true : move to branch addr : %x\n\n", r->pc);
    //printf( "beq false : move to next : %x\n\n", r->pc);
    //printf( "bne : [ %x\t%x ] \t", r->reg[id2ex[3]], r->reg[id2ex[4]]);
    //printf( "bne true : move to branch addr : %x\n\n", r->pc);
    //printf( "bne false : move to next addr : %x\n\n", r->pc);*/
}






void mem_access(uint8_t* mem, ex_mem* ex_mem, mem_wb* mem_wb) {
    //printf( "[memory access] \t");
    init_memwb(mem_wb);
    copy_control(mem_wb->ctrl, ex_mem->ctrl);

    if (ex_mem->ctrl->get_imm == 3) {
        mem_wb->reg_write = ex_mem->reg_write;
        mem_wb->alu_res = ex_mem->alu_res;
        //printf( "lui\n");
        return;
    }

    if (ex_mem->ctrl->ex_skip != 0) {   // branch, j, jr jal skip
        //printf( "\n");
        return;
    }

    mem_wb->alu_res = ex_mem->alu_res;
    mem_wb->reg_write = ex_mem->reg_write;

    if ((ex_mem->ctrl->mem_read == 0) && (ex_mem->ctrl->mem_write == 0)) {  // lw sw 외 에는 return
        //printf( "\n");
        return;
    }


    if (ex_mem->ctrl->mem_read == 1) {             // lw

        //printf( "lw : load Mem[0x%x] -> ", ex_mem->alu_res);
        lw_count++;
        uint32_t temp = 0;
        for (int j = 3; j >= 0; j--) {
            temp <<= 8;
            temp |= Memory[ex_mem->alu_res + j];
        }
        mem_wb->mem_read = temp;           // lw로 저장해야되는값.
        //printf( "%x val -> reg[%d]", mem_wb->mem_read, mem_wb->reg_write);
    }
    else if (ex_mem->ctrl->mem_write == 1) {       // sw
        //printf( "sw : store val 0x%x -> Mem[%x]",ex_mem->mem_val , ex_mem->alu_res); //ex2mem[3],ex2mem[2]
        sw_count++;
        for (int i = 0; i < 4; i++) {
            Memory[ex_mem->alu_res + i] = (ex_mem->mem_val >> 8 * i) & 0xff;
        }
    }

    //printf( "\n");
    return;
}

// 여기부터 다시 ㄱㄱ
void write_back(Registers* r, mem_wb* mem_wb) {
    //printf( "[write back] \t");


    if (mem_wb->ctrl->get_imm == 3) {
        r->reg[mem_wb->reg_write] = mem_wb->alu_res;
        //printf( "lui  R%d  0x%x\n", mem_wb->reg_write, mem_wb->alu_res);

        return;
    }

    
    if (mem_wb->ctrl->reg_wb == 0) {        // reg_write 값 0이면 지우기
        //printf( "skip\n");
        return;
    }

    write_reg_count++;

    if (mem_wb->ctrl->mem_read == 1) {       //  lw의 경우
        r->reg[mem_wb->reg_write] = mem_wb->mem_read;
        //printf( "R%d = 0x%x\n", mem_wb->reg_write, mem_wb->mem_read);
    }
    else {                           //  R type alu
        r->reg[mem_wb->reg_write] = mem_wb->alu_res;
        //printf( "R%d = 0x%x\n", mem_wb->reg_write, mem_wb->alu_res);
    }


}





void datapath(Registers* r, uint8_t* mem) {
    int exit_proc = 0;
    int ctrl_flow[4] = { -1, -1, -1, -1 }; // Default: execute all stages (ID, EX, MEM, WB)

    if_id* ifId = (if_id*)malloc(sizeof(if_id));
    id_ex* idEx = (id_ex*)malloc(sizeof(id_ex));
    ex_mem* exMem = (ex_mem*)malloc(sizeof(ex_mem));
    mem_wb* memWb = (mem_wb*)malloc(sizeof(mem_wb));

    idEx->ctrl = (Control*)malloc(sizeof(Control));
    exMem->ctrl = (Control*)malloc(sizeof(Control));
    memWb->ctrl = (Control*)malloc(sizeof(Control));


    int stall_cnt = 0;      // stall 하는 횟수 삽입

    init_ifid(ifId);
    init_idex(idEx);
    init_exmem(exMem);
    init_memwb(memWb);

    // pipe line loop
    do {

        //printf( "================================================================================\n");
        
        /*if (r->pc == 0xfff0) {
            printf("");
        }*/

        int hazard_cnt = 0;
        //printf( "( %d )\tFetch pc : %x\n", inst_count, r->pc);

        if (ifId->inst == 0) {
            ctrl_flow[0] = 0;
            nop_count++;
        }

        if (inst_count == 30) {
            //printf("");
        }


        //printf( "[Hazard]\n");
        // Hazard Detection Unit
        // 1. Load-Use Hazard 감지: ID/EX 레지스터를 통해 하자드 감지
        // 2. Forwarding 필요 여부 감지: EX/MEM 및 ID/EX 레지스터를 통해 하자드 감지
        // 3. Branch 명령어 감지: IF/ID 레지스터를 통해 하자드 감지

        // Load-use Hazard 감지
        //if (idEx->ctrl->mem_read) {
        //    if ((idEx->reg_tar == ifId->reg_src) || (idEx->reg_tar == ifId->reg_tar)) {
        //        //printf( "load use hazard\n");
        //        // stall the pipeline
        //        r->pc -= 8;
        //        ctrl_flow[0] = 0;
        //    }
        //}

        // forwarding 감지



        int lw_ex_hazardA = 0;
        int lw_ex_hazardB = 0;
 
        if (idEx->ctrl->rs_ch == 1 && idEx->ctrl->ex_skip == 0) {
            // EX hazard
            int temp1 = 0;


            if (exMem->ctrl->reg_wb == 1 && exMem->reg_write != 0 && idEx->reg_src == exMem->reg_write) {
                idEx->forward_a = 0b10;
                hazard_cnt++;
                //printf("ex src hazard a : 0b10\n");
                if (exMem->ctrl->mem_read != 1) {
                    idEx->forward_a_val = exMem->alu_res;
                }
                else {
                    lw_ex_hazardA = 1;
                }
                temp1 = 1;
            }

            // MA hazard
            if ((temp1 == 0) && memWb->ctrl->reg_wb == 1 && idEx->reg_src == memWb->reg_write) {
                //printf("ma src hazard a : 0b01\n");
                idEx->forward_a = 0b01;
                hazard_cnt++;
                idEx->forward_a_val = (memWb->ctrl->mem_read == 1) ? memWb->mem_read : memWb->alu_res;
            }
        }

        if (idEx->ctrl->rt_ch == 1 && idEx->ctrl->ex_skip == 0) {
            // EX hazard
            int temp2 = 0;
            if (exMem->ctrl->reg_wb == 1 && exMem->reg_write != 0 && idEx->reg_tar == exMem->reg_write) {
                idEx->forward_b = 0b10;
                //printf("ex tar hazard b : 0b10\n");
                hazard_cnt++;
                if (exMem->ctrl->mem_read != 1) {
                    idEx->forward_b_val = exMem->alu_res;
                }
                else {
                    lw_ex_hazardB = 1;
                }
                temp2 = 1;
            }

            // MA hazard
            if ((temp2 == 0) && memWb->ctrl->reg_wb == 1 && idEx->reg_tar == memWb->reg_write) {
                //printf("ma tar hazard b : 0b01\n");
                idEx->forward_b = 0b01;
                idEx->forward_b_val = (memWb->ctrl->mem_read == 1) ? memWb->mem_read : memWb->alu_res;

                hazard_cnt++;
            }
        }




        // branchjr hazard
        int bj_lw_hazard = 0;       // branch jr hazard 에서 쓰임

        if ((ifId->opcode == 0x4 || ifId->opcode == 0x5) || (ifId->opcode == 0x0 && ifId->funct == 0x08)) {


            // branch MA hazard
            if ((exMem->ctrl->reg_wb == 1) && (exMem->reg_write != 0)) {

                // for lw-nop-branch hazard
                // rs
                if (exMem->reg_write == ifId->reg_src) {
                    //printf( "10[branch MA hazard] ( forward A(rs)  : 0b01 )\n");
                    ifId->forward_a = 0b01;
                    hazard_cnt++;

                }

                // rt : jr 제외
                if ((exMem->reg_write == ifId->reg_tar) && (ifId->opcode == 0x4 || ifId->opcode == 0x5)) {
                    //printf( "9 [branch MA hazard] ( forward B(rt) : 0b01 )\n");
                    ifId->forward_b = 0b01;
                    hazard_cnt++;

                }


                if (exMem->ctrl->mem_read == 1) {   // lw
                    bj_lw_hazard = 1;
                    hazard_cnt++;
                }


            }


            // branch EX hazard
            if ((idEx->ctrl->reg_wb == 1) && (idEx->reg_write != 0)) {
                // rs
                if (idEx->reg_write == ifId->reg_src) {
                    //printf( "8 branch EX hazard ( forward A  : 0b10 )\n");
                    ifId->forward_a = 0b10;
                    hazard_cnt++;

                }

                // rt : jr 제외
                if ((idEx->reg_write == ifId->reg_tar) && (ifId->opcode == 0x4 || ifId->opcode == 0x5)) {
                    //printf( "7 branch EX hazard ( forward B  : 0b10 )\n");
                    ifId->forward_b = 0b10;
                    hazard_cnt++;

                }
            }

        }

        //printf("hazard : %d\n", hazard_cnt);

        //// 2. MEM hazard 감지



        //// lui ma hazard
        //if (memWb->ctrl->get_imm == 3 && (idEx->reg_tar == memWb->reg_write)) {
        //    if (!(exMem->ctrl->reg_wb && (exMem->reg_write != 0) && (exMem->reg_write == idEx->reg_tar))) {
        //        idEx->forward_b = 0b01;
        //        idEx->forward_b_val = memWb->alu_res;
        //        //printf( "14 lui ma hazard [forward B : 0b10] 0x%x\n", idEx->forward_a_val);
        //        hazard_cnt++;
        //    }
        //}
        //else if (memWb->ctrl->get_imm == 3 && (idEx->reg_src == memWb->reg_write)) {
        //    if (!(exMem->ctrl->reg_wb && (exMem->reg_write != 0) && (exMem->reg_write == idEx->reg_src))) {
        //        idEx->forward_a = 0b01;
        //        idEx->forward_a_val = memWb->alu_res;
        //        //printf( "13 lui ma hazard [forward A : 0b10] 0x%x\n", idEx->forward_b_val);
        //        hazard_cnt++;

        //    }
        //}



        //// ma hazard
        //if ((memWb->ctrl->reg_wb) && (memWb->reg_write != 0) && (memWb->ctrl->ex_skip == 0)) {
        //    if (!(exMem->ctrl->reg_wb && (exMem->reg_write != 0) && (exMem->reg_write == idEx->reg_src))) {
        //        if (memWb->reg_write == idEx->reg_src) {
        //            idEx->forward_a = 0b01;

        //            idEx->forward_a_val = (memWb->ctrl->mem_read == 1) ? memWb->mem_read : memWb->alu_res;
        //            //printf( "12 mem hazard A ( forward A : 0b01 ) : 0x%x\n", idEx->forward_a_val);
        //            hazard_cnt++;

        //        }
        //    }

        //    if (!((exMem->ctrl->reg_wb) && (exMem->reg_write != 0) && (exMem->reg_write == idEx->reg_tar)) && (idEx->ctrl->reg_dst == 1)) {
        //        if (memWb->reg_write == idEx->reg_tar) {
        //            idEx->forward_b = 0b01;
        //            idEx->forward_b_val = (memWb->ctrl->mem_read == 1) ? memWb->mem_read : memWb->alu_res;
        //            //printf( "11 mem hazard ( B : 0b01 ) : 0x%x\n", idEx->forward_b_val);
        //            hazard_cnt++;

        //        }
        //    }
        //}


        //



        ///*if (ifId->opcode == 0x4 || ifId->opcode == 0x5) {
        //    if (idEx->ctrl->reg_wb && (idEx->reg_write == ifId->reg_src)) {
        //        //printf( "branch hazard [0b01]\n");
        //        branch = 0b01;
        //    }
        //    else if (idEx->ctrl->reg_wb && (idEx->reg_write == ifId->reg_tar)) {
        //        //printf( "branch hazard [0b10]\n");
        //        branch = 0b10;
        //    }
        //}*/



        ////// load branch hazard
        ////if (ifId->opcode == 0x4 || ifId->opcode == 0x5) {
        ////    if (exMem->ctrl->mem_read && (exMem->reg_write == ifId->reg_src)) {
        ////        //printf( "load-branch hazard [0b01]\n");
        ////        branch_load = 0b01;
        ////    }
        ////    else if (exMem->ctrl->mem_read && (exMem->reg_write == ifId->reg_tar)) {
        ////        branch_load = 0b10;
        ////        //printf( "load-branch hazard [0b10]\n");
        ////    }
        ////}


        //// 1. EX hazard 감지
        ///*if (exMem->ctrl->reg_wb && idEx->ctrl->alu_ctrl != 0 && exMem->ctrl->mem_read != 0) {
        //    if (exMem->reg_write != 0 && (exMem->reg_write == idEx->reg_src)) {
        //        idEx->forward_a = 0b10;
        //        idEx->forward_a_val = exMem->alu_res;
        //        //printf( "ex hazard [A : 0b10] 0x%x\n", idEx->forward_a_val);
        //    }
        //    if (exMem->reg_write != 0 && (exMem->reg_write == idEx->reg_tar)) {
        //        idEx->forward_b = 0b10;
        //        idEx->forward_b_val = exMem->alu_res;
        //        //printf( "ex hazard [B : 0b10] 0x%x\n", idEx->forward_b_val);
        //    }
        //}*/

        //// EX Hazard
        ////(exMem->ctrl->mem_read == 0) &&
        //if ((exMem->ctrl->mem_read == 0) && (exMem->ctrl->reg_wb == 1) && (exMem->ctrl->ex_skip == 0)) {
        //    if ((exMem->reg_write != 0) && (exMem->reg_write == idEx->reg_src)) {
        //        idEx->forward_a = 0b10;
        //        idEx->forward_a_val = exMem->alu_res;
        //        //printf( "6 ex hazard [forward A : 0b10] 0x%x\n", idEx->forward_a_val);
        //        hazard_cnt++;

        //    }

        //    if (idEx->ctrl->reg_dst == 1) {        // r-type
        //        if ((exMem->reg_write != 0) && (exMem->reg_write == idEx->reg_tar)) {
        //            idEx->forward_b = 0b10;
        //            idEx->forward_b_val = exMem->alu_res;
        //            //printf( "5 ex hazard [forward B : 0b10] 0x%x\n", idEx->forward_b_val);
        //            hazard_cnt++;

        //        }
        //    }
        //}


        //// lw hazard
        //int lw_ex_hazard = 0;
        //if ((exMem->ctrl->mem_read == 1) && (exMem->reg_write != 0)) {
        //    if (exMem->reg_write == idEx->reg_src) {
        //        idEx->forward_a = 0b10;
        //        lw_ex_hazard = 1;
        //        hazard_cnt++;
        //    }

        //    if (idEx->ctrl->reg_dst == 1) {        // r-type
        //        if (exMem->reg_write == idEx->reg_tar) {
        //            idEx->forward_b = 0b10;
        //            lw_ex_hazard = 1;
        //            hazard_cnt++;

        //        }
        //    }
        //}




        //// SW Hazard
        //if (idEx->ctrl->mem_write) { // sw 명령어인 경우
        //    
        //    //  sw 의 reg_src가 변경 사항이 있는 경우
        //    if (exMem->ctrl->reg_wb && (exMem->reg_write != 0) && (exMem->reg_write == idEx->reg_src)) {
        //        idEx->forward_a = 0b10;
        //        idEx->forward_a_val = exMem->alu_res;
        //        //printf( "4 ex hazard [A (sw)] : 0b10 0x%x\n", idEx->forward_a_val);
        //        hazard_cnt++;

        //    }

        //    //  sw 의 reg_tar가 변경 사항이 있는 경우  
        //    if (exMem->ctrl->reg_wb && (exMem->reg_write != 0) && (exMem->reg_write == idEx->reg_tar)) {
        //        idEx->forward_b = 0b10;
        //        idEx->forward_b_val = exMem->alu_res;
        //        //printf( "3 ex hazard [B (sw)] : 0b10 0x%x\n", idEx->forward_b_val);
        //        hazard_cnt++;

        //    }


        //    /*if (memWb->ctrl->reg_wb && (memWb->reg_write != 0) && (memWb->reg_write == idEx->reg_src)) {
        //        idEx->forward_a = 0b01;
        //        idEx->forward_a_val = memWb->alu_res;
        //        //printf( "mem hazard [A (sw)] : 0b01 0x%x\n", idEx->forward_a_val);
        //    }*/
        //}


        //// lui ex hazard
        //if (exMem->ctrl->get_imm == 3 && (idEx->reg_tar == exMem->reg_write)) {
        //    idEx->forward_b = 0b10;
        //    idEx->forward_b_val = exMem->alu_res;
        //    //printf( "2 lui ex hazard[forward B : 0b10] 0x%x\n", idEx->forward_b_val);
        //    hazard_cnt++;

        //}
        //else if (exMem->ctrl->get_imm == 3 && (idEx->reg_src == exMem->reg_write)) {
        //    idEx->forward_a = 0b10;
        //    idEx->forward_a_val = exMem->alu_res;
        //    //printf( "1 lui ex hazard[forward A : 0b10] 0x%x\n", idEx->forward_a_val);
        //    hazard_cnt++;

        //}


        ////printf( "hazard count : %d\n[data path]\n",hazard_cnt);
        
       

        // Write Back stage

        if (ctrl_flow[3] == 1) {
            // 1. Write Back 단계 실행
            //printf( "pc : 0x%x    ", r->pc - 16);
            write_back(r, memWb);
        }
        else if (ctrl_flow[3] == 0) {
            // 1. NOP 실행

            //printf( "pc : 0x%x    ", r->pc - 16);
            //printf( "[write back]   nop\n");
        }
        else {
            //printf( "nothing\n");
            // 1. 아무 작업도 하지 않음
        }



        


        // Memory Access stage
        if (ctrl_flow[2] == 1) {
            // 1. Memory Access 단계 실행
            //printf( "pc : 0x%x    ", r->pc - 12);
            mem_access(Memory, exMem, memWb);
        }
        else if (ctrl_flow[2] == 0) {
            // 1. NOP 실행
            //printf( "pc : 0x%x    ", r->pc - 12);
            //printf( "[memory access]   nop\n");
            init_memwb(memWb);
        }
        else {
            // 1. 아무 작업도 하지 않음
            //printf( "nothing\n");
        }


        if (lw_ex_hazardA == 1) {
            idEx->forward_a_val = memWb->mem_read;

        }
        if (lw_ex_hazardB == 1) {
            idEx->forward_b_val = memWb->mem_read;
        }



        
        if (ifId->forward_a == 0b01) {
            ifId->forward_a_val = (bj_lw_hazard == 1) ? memWb->mem_read : memWb->alu_res;
        }


        if (ifId->forward_b == 0b01) {
            ifId->forward_b_val = (bj_lw_hazard == 1) ? memWb->mem_read : memWb->alu_res;
        }





        // Execute stage
        if (ctrl_flow[1] == 1) {
            //printf( "pc : 0x%x    ", r->pc - 8);
            execute(idEx, exMem);
            // 1. Execute 단계 실행
            // 2. Forwarding 필요 시 데이터 포워딩 수행
        }
        else if (ctrl_flow[1] == 0) {
            // 1. NOP 실행
            //printf( "pc : 0x%x    ", r->pc - 8);
            //printf( "[execute]   nop\n");
            init_exmem(exMem);
        }
        else {
            // 1. 아무 작업도 하지 않음
            //printf( "nothing\n");
        }



        // branch ex hazard
        if (ifId->forward_a == 0b10) {
            ifId->forward_a_val = exMem->alu_res;
        }

        if (ifId->forward_b == 0b10) {
            ifId->forward_b_val = exMem->alu_res;
        }



        // Decode stage
        if (ctrl_flow[0] == 1) {
            //printf( "pc : 0x%x    ", r->pc - 4);
            decode(r, ifId, idEx);
        }
        else if (ctrl_flow[0] == 0) {
            // 1. NOP 실행
            //printf( "pc : 0x%x    ", r->pc - 4);
            //printf( "[decode]   nop\n");
            init_idex(idEx);
        }
        else {

            // 1. 아무 작업도 하지 않음
            //printf( "nothing\n");
        }
        


        // Fetch stage (always execute)
        // 1. Fetch 단계 실행
        // 2. Branch Prediction 수행
        if (r->pc != 0xffffffff) {
            //printf( "pc : 0x%x    \n", r->pc);

            fetch(r, mem, ifId);


        }

        if (r->pc != 0xffffffff) {
            r->pc = r->pc + 4;
            for (int i = 3; i > 0; i--) {
                ctrl_flow[i] = ctrl_flow[i - 1];
            }

            if (stall_cnt == 0) {
                ctrl_flow[0] = 1;
            }

        }
        else if (r->pc == 0xffffffff) {
            //printf( "0xffffffff\n");
            exit_proc++;
            for (int i = 3; i > 0; i--) {
                ctrl_flow[i] = ctrl_flow[i - 1];
            }
            if (exit_proc >= 3) {
                ctrl_flow[0] = -1;
            }
        }
        //printf( "%d : 0x%x\n", inst_count, r->reg[2]);

    } while (exit_proc <= 5);


    printf( "================================================================================\n");
    printf( "Return register (r2)                 : %d\n", r->reg[2]);
    printf( "Total clock cycle                    : %d\n", inst_count);
    printf( "r-type count                         : %d\n", r_count);
    printf( "i-type count                         : %d\n", i_count);
    printf( "branch, j-type count, jr             : %d\n", branch_jr_count);
    printf( "lw count                             : %d\n", lw_count);
    printf( "sw count                             : %d\n", sw_count);
    printf( "nop count                            : %d\n", nop_count);
    printf( "register write count                 : %d\n", write_reg_count);
    printf( "=================================================================================\n");



    // 메모리 해제
    free(ifId);
    free(idEx->ctrl); // 할당된 Control 구조체 메모리 해제
    free(idEx);
    free(exMem->ctrl); // 할당된 Control 구조체 메모리 해제
    free(exMem);
    free(memWb->ctrl); // 할당된 Control 구조체 메모리 해제
    free(memWb);

    

    return;
}

    /*
    * single cycle
    //r->pc != 0xffffffff
    while (r->pc != 0xffffffff) {
        //printf( "0x%x : \t", r->pc);
        inst = fetch(r, mem);
        if (inst == 0) {
            //printf( "NOP\n\n");
            continue;
        }
        //printf( "%08x \t", inst);
        decode(r, &c, inst, id2ex, addr_flow);
        execute(r, &c, id2ex, ex2mem, addr_flow);
        // branch, jump, jal, jalr 분기 명령어들 모음.
        temp = id2ex[0] == 2 || id2ex[0] == 3 || id2ex[0] == 4 || id2ex[0] == 5 || id2ex[0] == 0xf || id2ex[1] == 3 || (id2ex[1] == 0 && id2ex[2] == 0x9);
        if (temp) {
            for (int i = 0; i < 8; i++) {
                id2ex[i] = 0;
            }
            continue;
        }

        mem_access(&c, ex2mem, mem2wb);
        write_back(r, &c, mem2wb);
        //printf( "\n\n");

        init_latch(&inst, addr_flow, id2ex, ex2mem, mem2wb);
    }
    //printf( "================================================================================\n");
    //printf( "Return register (r2)                 : %d\n", r->reg[2]);
    //printf( "Total cycle                          : %d\n", inst_count);
    //printf( "r-type count                         : %d\n", r_count);
    //printf( "i-type count                         : %d\n", i_count);
    //printf( "j-type count                         : %d\n", j_count);
    //printf( "branch count                         : %d\n", b_count);
    //printf( "memory access count                  : %d\n", ma_count);
    //printf( "=================================================================================\n");
    */



// register init function
void init_reg(Registers* r) {
    r->pc = 0;

    for (int i = 0; i < 32; i++) {
        r->reg[i] = 0;
    }

    r->reg[31] = 0xffffffff;
    r->reg[29] = 0x1000000;
}






int main(void) {

    //fp = fopen("data.txt", "w");

    FILE* file = fopen("C:\\Users\\ldj23\\Desktop\\computer science\\test_prog\\fib2.bin", "rb"); // "filename.bin" => filename 변경해서 다른파일 넣기.
    if (!file) {
        perror("File opening failed");
        return 1;
    }

    

    uint32_t temp;              // temp 변수에 32bit를 받고 각각 넣어주기 위한 변수 : buffer 역할.
    size_t bytesRead;           // 읽어 드린 개수 확인하는 변수 : 더 이상 읽어 들일 binary 값이 없으면 종료
    size_t memoryIndex = 0;     // Instruction Memory의 총 개수.

    int count = 0;

    while ((bytesRead = fread(&temp, sizeof(uint32_t), 1, file)) > 0 && memoryIndex < (MAX_MEMORY - 3)) {
        // 파일에서 uint32_t 크기만큼 읽고, Memory 배열에 값을 리틀엔디안 식으로 할당.
        Memory[memoryIndex++] = (temp >> 24) & 0xFF;    // 상위 8비트
        Memory[memoryIndex++] = (temp >> 16) & 0xFF;    // 다음 8비트
        Memory[memoryIndex++] = (temp >> 8) & 0xFF;     // 다음 8비트
        Memory[memoryIndex++] = temp & 0xFF;            // 하위 8비트
    }

    fclose(file);



    Registers regis;
    init_reg(&regis);
    datapath(&regis, Memory);


    return 0;
}