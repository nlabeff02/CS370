#include <stdio.h>
#include <stdint.h>
/*
 * File: program_statistics.c
 * Author: Nathan LaBeff
 * Date: 02-29-2024
 * Desc: This file contains code that reads in an input file "trace.txt"
 *       and converts the MIPS code with in putting the output into "statistics.txt"
 *       where it then puts it into a list with types of instructions, loads, stores, etc.
 */
typedef enum {
    i, r, j
} Type;

typedef struct {
    uint64_t addr, word;
    int op, rs, rt, rd, shamt, funct, imm, jAddr;
    Type type;
} Instruction;

typedef struct {
    int insts, rType, iType, jType, fwdTaken, bkwTaken, notTaken, loads, stores, arith;
    int reg[32][2];
} Stats;

int bitsAt(uint64_t word, int start, int end) {
    uint64_t bitMask = ((1ULL << (end - start + 1)) - 1) << start;
    return (int) ((word & bitMask) >> start);
}

//Function to get the op code and identify the type of instruction.
Type getOp(int op) {
    switch (op) {
        case 0x00:
            return r;
        case 0x02: //j
        case 0x03: //jal
            return j;
        default:
            return i;
    }
}

//increases count of the instruction types in stats
void addType(Stats *stats, Type type) {
    switch (type) {
        case r:
            stats->rType++;
            break;
        case i:
            stats->iType++;
            break;
        case j:
            stats->jType++;
            break;
    }
}

//load counter
void addToLoad(Stats *stats, int op) {
    switch (op) {
        case 0x20: //add
        case 0x24: //and
        case 0x21: //addu
        case 0x25: //or
        case 0x23: //subu
            stats->loads++;
            break;
        case 0x28: //sb
        case 0x29: //sh
        case 0x2b: //sw
            stats->stores++;
            break;
    }
}

//method to count the number of arithmetic instructions
void addArith(Stats *stats, int op, int funct) {
    if (op == 0x00) { // R-type instructions
        switch (funct) {
            case 0x20: // add
            case 0x21: // addu
            case 0x22: // sub
            case 0x23: // subu
            case 0x18: // mult
            case 0x19: // multu
            case 0x1a: // div
            case 0x1b: // divu
            case 0x10: // mfhi
            case 0x12: // mflo
            case 0x00: // mfc0
            case 0x01: // mtc0
                stats->arith++;
                break;
        }
    } else { // I-type instructions
        switch (op) {
            case 0x08: // addi
            case 0x09: // addiu
                stats->arith++;
                break;
        }
    }
}

//increases the count of the read and write in the stats based on a given instruction
void addReadWrite(Stats *stats, Instruction *in) {
    int read = 0, write = 1;
    switch (in->type) {
        case r:
            stats->reg[in->rd][write]++;
            stats->reg[in->rs][read]++;
            stats->reg[in->rt][read]++;
            switch (in->funct) {
                case 0x08:
                    stats->reg[in->rd][write]--;
                    stats->reg[in->rt][read]--;
                    break;
                case 0x00:
                case 0x02:
                case 0x3:
                    stats->reg[in->rs][read]--;
                    break;
            }
            break;

        case i:
            stats->reg[in->rt][write]++;
            stats->reg[in->rs][read]++;

            switch (in->op) {
                case 0xf:
                    stats->reg[in->rs][read]--;
                    break;

                case 0x4:
                case 0x5:
                    stats->reg[in->rt][write]--;
                    stats->reg[in->rt][read]++;
                    break;

                case 0x28:
                case 0x29:
                case 0x2b:
                case 0x38:
                    stats->reg[in->rt][read]++;
                    stats->reg[in->rt][write]--;
                    break;

            }
            break;

        default:
            if (in->op == 0x3) {
                stats->reg[in->rt][write]--;
                stats->reg[in->rs][read]--;
                stats->reg[31][write]++;
            }
            break;

    }
}

//Counter for the different branches identifies if its fwd, bkw, or not.
void addBranchCount(Stats *stats, long prevAddr, long inAddr, int prevOp) {
    long diff = inAddr - prevAddr;
    if (diff > 4) {
        stats->fwdTaken++;
    } else if (diff < 0) {
        stats->bkwTaken++;
    }else if (prevOp == 0x04 || prevOp == 0x05){
        stats -> notTaken++;
    }
}

//The function for identifying the op, rs, rt, etc in order to build stats.
Stats getStat(Instruction *list, int count) {
    Stats stats = {0};
    Instruction *prev = NULL;
    for (int i = 0; i < count; i++) {
        list[i].op = bitsAt(list[i].word, 26, 31);
        list[i].rs = bitsAt(list[i].word, 21, 25);
        list[i].rt = bitsAt(list[i].word, 16, 20);
        list[i].rd = bitsAt(list[i].word, 11, 15);
        list[i].shamt = bitsAt(list[i].word, 6, 10);
        list[i].funct = bitsAt(list[i].word, 0, 5);
        list[i].imm = bitsAt(list[i].word, 0, 15);
        list[i].jAddr = bitsAt(list[i].word, 0, 25);
        list[i].type = getOp(list[i].op);

        stats.insts++;

        //Calls to the different add functions in order to later print for output
        addType(&stats, list[i].type);
        addToLoad(&stats, list[i].op);
        addArith(&stats, list[i].op, list[i].funct);
        addReadWrite(&stats, &list[i]);
        if (prev != NULL) {
            addBranchCount(&stats, prev->addr, list[i].addr, prev->op);
        }

        prev = &list[i];
    }
    return stats;
}

//Print method
void printStats(FILE *outFile, Stats *stats) {
    fprintf(outFile, "insts: %d\n", stats->insts);
    fprintf(outFile, "r-type: %d\n", stats->rType);
    fprintf(outFile, "i-type: %d\n", stats->iType);
    fprintf(outFile, "j-type: %d\n", stats->jType);
    fprintf(outFile, "fwd-taken: %.6f\n", (float) stats->fwdTaken / stats->insts * 100);
    fprintf(outFile, "bkw-taken: %.6f\n", (float) stats->bkwTaken / stats->insts * 100);
    fprintf(outFile, "not-taken: %.6f\n", (float) stats->notTaken / stats->insts * 100);
    fprintf(outFile, "loads: %.6f\n", (float) stats->loads / stats->insts * 100);
    fprintf(outFile, "stores: %.6f\n", (float) stats->stores / stats->insts * 100);
    fprintf(outFile, "arith: %.6f\n", (float) stats->arith / stats->insts * 100);
    for (int i = 0; i < 32; i++) {
        fprintf(outFile, "reg-%d: %d %d\n", i, stats->reg[i][0], stats->reg[i][1]);
    }
}

//Open the input/make the output file, call getStats, call print method, close files
int main() {
    FILE *inFile = fopen("trace.txt", "r");
    if (inFile == NULL) {
        printf("Input file not found! \n");
        return 1;
    }
    FILE *outFile = fopen("statistics.txt", "w");
    if (outFile == NULL) {
        printf("Unable to open output file! \n");
        return 1;
    }
    Instruction list[1000];
    int counter = 0;
    while (fscanf(inFile, "%lx %lx", &list[counter].addr, &list[counter].word) != EOF) {
        counter++;
    }
    fclose(inFile);
    Stats stats = getStat(list, counter);
    printStats(outFile, &stats);
    fclose(outFile);
    return 0;

}

