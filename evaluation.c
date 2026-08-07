#include "mpc.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <string.h>

//fake functions for windows
static char buffer[2048];

char* readline(char* prompt) {
    fputs(prompt, stdout);
    fgets(buffer, 2048, stdin);
    char* cpy = malloc(strlen(buffer)+1);
    strcpy(cpy, buffer);
    cpy[strlen(cpy)-1] = '\0';
    return cpy;
}

void add_history(char* unused) {}

#else
#include <editline/readline.h>

#endif

enum { KVAL_NUM, KVAL_ERR };

enum { KERR_DIV_ZERO, KERR_BAD_OP, KERR_BAD_NUM };

typedef struct {
    int type;
    long num;
    int err;
} kval;

kval kval_num(long x);
kval kval_err(int x);
void kval_print(kval v);
void kval_println(kval v);

kval eval(mpc_ast_t* t);

kval eval_op(kval x, char* op, kval y);

int main(int argc, char *argv[])
{

    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Operator = mpc_new("operator");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Kyulang = mpc_new("Kyulang");

    mpca_lang(MPCA_LANG_DEFAULT,
            "   \
            number  :   /-?[0-9]+/ ;    \
            operator:   '+' | '-' | '*' | '/' | '%' | '^' | \"min\" | \"max\" ; \
            expr    :   <number> | '(' <operator> <expr>+ ')' ; \
            Kyulang :   /^/ <operator> <expr>+ /$/ ; \
            ",
            Number, Operator, Expr, Kyulang);

    puts("Kyulang version 0.0.0.0.1");
    puts("Press Ctrl+C to Exit\n");

    while (1) {
        char* input = readline("halo^_^ ~>");

        add_history(input);

        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Kyulang, &r)) {
            mpc_ast_print(r.output);
            kval result = eval(r.output);
            kval_println(result);
            mpc_ast_delete(r.output);
        } else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);
    }

    mpc_cleanup(4, Number, Operator, Expr, Kyulang);

    return 0;
}

kval eval(mpc_ast_t* t) {
    if (strstr(t->tag, "number")) {
        errno = 0;
        long x = strtol(t->contents, NULL, 10);
        return errno != ERANGE ? kval_num(x) : kval_err(KERR_BAD_NUM);
    }

    char* op = t->children[1]->contents;

    kval x = eval(t->children[2]);

    int i = 3;

    if (strcmp(op, "-") == 0 && !strstr(t->children[i]->tag, "expr")) {
        x.num = -(x.num);
        return x;
    }

    while(strstr(t->children[i]->tag, "expr")) {
        x = eval_op(x, op, eval(t->children[i]));
        i++;
    }
    return x;
}

kval eval_op(kval x, char* op, kval y) {
    if (x.type == KVAL_ERR) { return x; }
    if (y.type == KVAL_ERR) { return y; }

    if (strcmp(op, "+") == 0) { return kval_num(x.num + y.num); }
    if (strcmp(op, "-") == 0) { return kval_num(x.num - y.num); }
    if (strcmp(op, "*") == 0) { return kval_num(x.num * y.num); }
    if (strcmp(op, "/") == 0) { return y.num == 0 ? kval_err(KERR_DIV_ZERO) : kval_num(x.num / y.num); }
    if (strcmp(op, "%") == 0) { return y.num == 0 ? kval_err(KERR_DIV_ZERO) : kval_num(x.num % y.num); }
    if (strcmp(op, "^") == 0) { return kval_num(pow(x.num, y.num)); }
    if (strcmp(op, "min") == 0) { return x.num > y.num ? y : x; }
    if (strcmp(op, "max") == 0) { return x.num < y.num ? y : x; }

    return kval_err(KERR_BAD_OP);
}

kval kval_num(long x) {
    kval v;
    v.type = KVAL_NUM;
    v.num = x;
    return v;
}

kval kval_err(int x) {
    kval v;
    v.type = KVAL_ERR;
    v.err = x;
    return v;
}

void kval_print(kval v) {
    switch (v.type) {
        case KVAL_NUM: printf("I uh- uh- think it is %li", v.num); break;

        case KVAL_ERR:
            if (v.err == KERR_DIV_ZERO) {
                printf("ERROR>~<: You cannot divide by a zero!");
            }
            if (v.err == KERR_BAD_OP) {
                printf("ERROR>~<: The operator isn't valid bro!");
            }
            if (v.err == KERR_BAD_NUM) {
                printf("ERROR>~<: The number is too big for me!");
            }
        break;
    }
}

void kval_println(kval v) {
    kval_print(v);
    putchar('\n');
}
