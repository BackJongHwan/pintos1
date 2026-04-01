#include<stdio.h>
#include<syscall.h>
#include <stdlib.h>

int main(int argc, char *argv[]){
    //fibonacci
    int result;

    if(argc == 2){
        int n = atoi(argv[1]);
        result = fibonacci(n);
    }
    //add4
    else if(argc == 5){
        int a = atoi(argv[1]);
        int b = atoi(argv[2]);
        int c = atoi(argv[3]);
        int d = atoi(argv[4]);
        result = max_of_four_int(a, b, c, d);
    }else{
        printf("invaild argument!!\n");
        return -1;
    }
    printf("%d\n", result);
    return EXIT_SUCCESS;
}