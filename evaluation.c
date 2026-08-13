#include "mpc.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
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

enum { KVAL_NUM, KVAL_ERR, KVAL_SEXPR, KVAL_SYM };

typedef struct kval {
    int type;
    long num;

    char* err;
    char* sym;

    int count;
    struct kval** cell;
} kval;

kval* kval_num(long x);
kval* kval_err(char* m);
kval* kval_sym(char* s);
kval* kval_sexpr(void);

kval* kval_read_num(mpc_ast_t* t);
kval* kval_read(mpc_ast_t* t);
kval* kval_add(kval* v, kval* x);

void kval_expr_print(kval* v, char open, char close);
void kval_print(kval* v);
void kval_println(kval* v);

void kval_del(kval* v);

kval eval(mpc_ast_t* t);

kval eval_op(kval x, char* op, kval y);

int main(int argc, char *argv[])
{

    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Symbol = mpc_new("symbol");
    mpc_parser_t* Sexpr = mpc_new("sexpr");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Kyulang = mpc_new("Kyulang");

    mpca_lang(MPCA_LANG_DEFAULT,
            "   \
            number  :   /-?[0-9]+/ ;    \
            symbol  :   '+' | '-' | '*' | '/' | '%' | '^' | \"min\" | \"max\" ; \
            sexpr   :   '(' <expr>* ')' ; \
            expr    :   <number> | <symbol> | <sexpr>' ; \
            Kyulang :   /^/ <expr>* /$/ ; \
            ",
            Number, Symbol, Sexpr, Expr, Kyulang);

    puts("Kyulang version 0.0.0.0.1");
    puts("Press Ctrl+C to Exit\n");

    while (1) {
        char* input = readline("halo^_^ ~>");

        add_history(input);

        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Kyulang, &r)) {
            mpc_ast_print(r.output);
            kval* x = kval_read(r.output);
            kval_println(x);
            kval_del(x);
            //kval_println(result);
            //mpc_ast_delete(r.output);
        } else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);
    }

    mpc_cleanup(5, Number, Symbol, Sexpr, Expr, Kyulang);

    return 0;
}

/*kval eval(mpc_ast_t* t) {
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
*/

/*kval eval_op(kval x, char* op, kval y) {
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
}*/

kval* kval_num(long x) {
    kval* v = malloc(sizeof(kval));
    v->type = KVAL_NUM;
    v->num = x;
    return v;
}

kval* kval_err(char* m) {
    kval* v = malloc(sizeof(kval));
    v->type = KVAL_ERR;
    v->err = malloc(strlen(m) + 1);
    strcpy(v->err, m);
    return v;
}

kval* kval_sym(char* s) {
    kval* v = malloc(sizeof(kval));
    v->type = KVAL_SYM;
    v->sym = malloc(strlen(s) + 1);
    strcpy(v->sym, s);
    return v;
}

kval* kval_sexpr(void) {
    kval* v = malloc(sizeof(kval));
    v->type = KVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

void kval_del(kval* v) {
    switch(v->type) {
        case KVAL_NUM: break;

        case KVAL_ERR: (free(v->err)); break;
        case KVAL_SYM: (free(v->sym)); break;

        case KVAL_SEXPR:
            for (int i = 0; i < v->count; i++) {
                kval_del(v->cell[i]);
            }

            free(v->cell);
            break;
    }
    free(v);
}

kval* kval_read_num(mpc_ast_t* t) {
    errno = 0;
    long x = strtol(t->contents, NULL, 0);
    return errno != ERANGE ? kval_num(x) : kval_err("This is an invalid number!");
}

kval* kval_read(mpc_ast_t* t) {
    if (strstr(t->tag, "number")) { return kval_read_num(t); }
    if (strstr(t->tag, "symbol")) { return kval_sym(t->contents); }

    kval* x = NULL;
    if (strcmp(t->tag, ">") == 0) { x = kval_sexpr(); }
    if (strstr(t->tag, "sexpr")) { x = kval_sexpr(); }

    for (int i = 0; i < t->children_num; i++) {
        if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
        if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
        if (strcmp(t->children[i]->tag, "regex") == 0) { continue; }
        x = kval_add(x, kval_read(t->children[i]));
    }
    return x;
}

kval* kval_add(kval* v, kval* x) {
    v->count++;
    v->cell = realloc(v->cell, sizeof(kval*) * v->count);
    v->cell[v->count-1] = x;
    return v;
}

void kval_expr_print(kval* v, char open, char close) {
    putchar(open);

    for (int i = 0; i < v->count; i++) {
        kval_print(v->cell[i]);

        if (i != (v->count-1)) {
            putchar(' ');
        }
    }
    putchar(close);
}

void kval_print(kval* v) {
    switch (v->type) {
        case KVAL_NUM:  printf("%li", v->num); break;
        case KVAL_ERR:  printf("Error: %s", v->err); break;
        case KVAL_SYM:  printf("%s", v->sym); break;
        case KVAL_SEXPR:  kval_expr_print(v, '(', ')'); break;
    }
}

void kval_println(kval* v) {
    kval_print(v);
    putchar('\n');
}
