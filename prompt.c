#include <stdio.h>
#include <stdlib.h>

#include <editline/readline.h>

int main(int argc, char *argv[])
{
    
    puts("Kyulang version 0.0.0.0.1");
    puts("Press Ctrl+C to Exit\n");

    while (1) {
        char* input = readline("halo^_^ ~>");
        
        add_history(input);

        printf("No, you are a %s\n", input);

        free(input);
    }

    return 0;
}
