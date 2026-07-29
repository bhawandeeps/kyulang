#include <stdio.h>

static char input[2048];

int main(int argc, char *argv[])
{
    
    puts("Kyulang version 0.0.0.0.1");
    puts("Press Ctrl+C to Exit\n");

    while (1) {
        fputs("halo^_^ ~>", stdout);
        
        fgets(input, 2048, stdin);

        printf("No, you are a %s\n", input);
    }

    return 0;
}
