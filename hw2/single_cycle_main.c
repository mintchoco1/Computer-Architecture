#include "dataPath.h"

int main(void){

    FILE *fp = fopen("test.txt", "rb");
    if(fp == NULL){
        printf("Error opening file\n");
        return 1;
    }

    uint32_t instruction;
    size_t bytesread;

    while((bytesread = fread(&instruction, sizeof(uint32_t), 1, fp)) > 0){

    }

    fclose(fp);
    return 0;
}