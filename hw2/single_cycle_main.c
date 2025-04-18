#include "DataPath.h"

int main(void){

    FILE *fp = fopen("test.txt", "rb");
    if(fp == NULL){
        printf("Error opening file\n");
        return 1;
    }

    fclose(fp);
    return 0;
}