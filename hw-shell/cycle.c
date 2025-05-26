#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
int main(int argc,char* argv[]){
    for(int i=0;i<1000000;++i){

        printf("Cycle %d\n", i);
    }
}