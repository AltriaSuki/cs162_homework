#include<stdio.h>

int main(int argc, char* argv[]){
    int a;
    scanf("%d", &a);
    int div=a/10;
    int count=1;//表示数字的位数
    while(div!=0){
        count++;
        div=div/10;
    }
    int result=0;
    for(int i=0;i<count;++i){
        result=result*10+a%10;
        a=a/10;
    }
    printf("%d\n", result);
    return 0;
}