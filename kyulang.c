#include "mpc.h"

#include <errno.h>
#include <math.h>
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

//macro for errors
#define KASSERT(args, cond, err) \
    if (!(cond)) { kval_del(args); return kval_err(err); }

//incorrect number of arguments
#define KARGCHECK(args, err) \
    if( args->count != 1 ) { kval_del(args); return kval_err(err); }

//empty list
#define EMPTYCHECK(args, err) \
    if( args->cell[0]->count != 0 ) { kval_del(args); return kval_err(err); }

enum { KVAL_NUM, KVAL_ERR, KVAL_SEXPR, KVAL_QEXPR, KVAL_SYM };

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
kval* kval_qexpr(void);

kval* kval_read_num(mpc_ast_t* t);
kval* kval_read(mpc_ast_t* t);
kval* kval_add(kval* v, kval* x);

void kval_expr_print(kval* v, char open, char close);
void kval_print(kval* v);
void kval_println(kval* v);

void kval_del(kval* v);

kval* kval_eval_sexpr(kval* v);

kval* builtin_op(kval* a, char* op);

kval* kval_eval(kval* v);
kval* kval_take(kval* v, int i);
kval* kval_pop(kval* v, int i);
kval* kval_join(kval* x, kval* y);

//qexpr funcs
kval* builtin_head(kval* v);
kval* builtin_tail(kval* v);
kval* builtin_list(kval* v);
kval* builtin_eval(kval* v);
kval* builtin_join(kval* v);

kval* builtin(kval* a, char* func);

int main(int argc, char *argv[])
{

    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Symbol = mpc_new("symbol");
    mpc_parser_t* Sexpr = mpc_new("sexpr");
    mpc_parser_t* Qexpr = mpc_new("qexpr");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Kyulang = mpc_new("Kyulang");

    mpca_lang(MPCA_LANG_DEFAULT,
            "   \
            number  :   /-?[0-9]+/ ;    \
            symbol  :   '+' | '-' | '*' | '/' | '%' | '^' | \"min\" | \"max\" | \"list\" | \"head\" | \"tail\" | \"join\" | \"eval\" ; \
            sexpr   :   '(' <expr>* ')' ; \
            qexpr   :   '{' <expr>* '}' ; \
            expr    :   <number> | <symbol> | <sexpr> | <qexpr> ; \
            Kyulang :   /^/ <expr>* /$/ ; \
            ",
            Number, Symbol, Sexpr, Qexpr, Expr, Kyulang);

    puts("Kyulang version 0.0.0.0.1");
    puts("Press Ctrl+C to Exit\n");

    while (1) {
        char* input = readline("halo^_^ ~>");

        add_history(input);

        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Kyulang, &r)) {
            mpc_ast_print(r.output);
            kval* x = kval_eval(kval_read(r.output));
            kval_println(x);
            kval_del(x);
        } else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);
    }

    mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr,  Expr, Kyulang);

    return 0;
}

kval* builtin_op(kval* a, char* op) {
    for (int i = 0; i < a->count; i++) {
        if (a->cell[i]->type != KVAL_NUM) {
            kval_del(a);
            return kval_err(">~< Add some numbers in!");
        }
    }

    kval* x = kval_pop(a, 0);

    if ((strcmp(op, "-") == 0) && a->count == 0) {
      x->num = -x->num;
    }

    while (a->count > 0) {

        kval* y = kval_pop(a, 0);

        if (strcmp(op, "+") == 0) { (x->num += y->num); }
        if (strcmp(op, "-") == 0) { (x->num -= y->num); }
        if (strcmp(op, "*") == 0) { (x->num *= y->num); }
        if (strcmp(op, "/") == 0) {
            if (y->num == 0) {
                kval_del(x); kval_del(y);
                x = kval_err(">~< Cannot divide by zero, dummy!"); break;
            }
            x->num /= y->num;
        }
        if (strcmp(op, "%") == 0) {
            if (y->num == 0) {
                kval_del(x); kval_del(y);
                x = kval_err(">~< Cannot divide by zero, dummy!"); break;
            }
            x->num %= y->num;
        }
        if (strcmp(op, "^") == 0) { x->num = pow(x->num, y->num); }
        if (strcmp(op, "min") == 0) {
            if (x->num > y->num) {
                kval_del(x);
                kval_del(a);
                return y;
            } else { kval_del(y); kval_del(a); return x; }}
        if (strcmp(op, "max") == 0) {
            if (x->num < y->num) {
                kval_del(x);
                kval_del(a);
                return y;
            } else { kval_del(y); kval_del(a); return x; }}
        kval_del(y);
    }
    kval_del(a);
    return x;
}

kval* kval_eval_sexpr(kval* v) {
    for (int i = 0; i < v->count; i++) {
        v->cell[i] = kval_eval(v->cell[i]);
    }

    for (int i = 0; i<v->count; i++) {
        if (v->cell[i]->type == KVAL_ERR) { return kval_take(v, i); }
    }

    if (v->count == 0) {
        return v;
    }

    if (v->count == 1) { return kval_take(v, 0); }

    kval* f = kval_pop(v, 0);
    if (f->type != KVAL_SYM) {
        kval_del(f); kval_del(v);
        return kval_err(">~< The S-expression needs to start with a symbol!");
    }

    kval* result = builtin(v, f->sym);
    kval_del(f);
    return result;
}

kval* kval_eval(kval* v) {
    if (v->type == KVAL_SEXPR) { return kval_eval_sexpr(v); }
    return v;
}

kval* kval_pop(kval* v, int i) {
    kval* x = v->cell[i];

    memmove(&v->cell[i], &v->cell[i+1], sizeof(kval*) * (v->count-i-1));

    v->count--;

    v->cell = realloc(v->cell, sizeof(kval*) * v->count);
    return x;
}

kval* kval_take(kval* v, int i) {
    kval* x = kval_pop(v, i);
    kval_del(v);
    return x;
}


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

kval* kval_qexpr(void) {
    kval* v = malloc(sizeof(kval));
    v->type = KVAL_QEXPR;
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
        case KVAL_QEXPR:
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
    if (strstr(t->tag, "qexpr")) { x = kval_qexpr(); }

    for (int i = 0; i < t->children_num; i++) {
        if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
        if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
        if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
        if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
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


kval* builtin_head(kval* v) {
    KARGCHECK(v,  ">~< 'head' cannot handle that many arguments!");
    EMPTYCHECK(v, ">~< 'head' is empty!");
    KASSERT(v, v->cell[0]->type == KVAL_QEXPR, ">~< 'head' only likes Q-expressions!");

    kval* x = kval_take(v, 0);

    while (x->count > 1) {
        kval_del(kval_pop(x, 1));
    }
    return x;
}

kval* builtin_tail(kval* v) {
    KARGCHECK(v, ">~< 'tail' cannot handle that many arguments!");
    EMPTYCHECK(v, ">~< 'tail' is empty!");
    KASSERT(v, v->cell[0]->type == KVAL_QEXPR, ">~< 'tail' only likes Q-expressions!");

    kval* x = kval_take(v, 0);

    kval_del(kval_pop(x, 0));
    return x;
}

kval* builtin_list(kval* v) {
    v->type = KVAL_QEXPR;
    return v;
}

kval* builtin_eval(kval* v) {
    KARGCHECK(v, ">~< 'eval' cannot handle that many arguments!");
    EMPTYCHECK(v, ">~< 'eval' is empty!");

    kval* x = kval_take(v, 0);
    x->type = KVAL_SEXPR;
    return kval_eval(x);
}

kval* builtin_join(kval* v) {
    for (int i = 0; i < v->count; i++) {
        KASSERT(v, v->cell[i]->type == KVAL_QEXPR, ">~< 'join' passed incorrect type!");
    }

    kval* x = kval_pop(v, 0);

    while (v->count) {
        x = kval_join(x, kval_pop(v, 0));
    }

    kval_del(v);
    return x;
}

kval* kval_join(kval* x, kval* y) {
    while (y->count) {
        x = kval_add(x, kval_pop(y, 0));
    }

    kval_del(y);
    return x;
}

kval* builtin(kval* a, char* func) {
    if (strcmp("list", func) == 0) { return builtin_list(a); }
    if (strcmp("head", func) == 0) { return builtin_head(a); }
    if (strcmp("tail", func) == 0) { return builtin_tail(a); }
    if (strcmp("join", func) == 0) { return builtin_join(a); }
    if (strcmp("eval", func) == 0) { return builtin_eval(a); }
    if (strstr("+-*/%^", func) || strcmp(func,"min")==0 || strcmp(func,"max")==0) {
        return builtin_op(a, func);
    }
    kval_del(a);
    return kval_err(">~< I am sorry I do not know what this is!");
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
        case KVAL_QEXPR:  kval_expr_print(v, '{', '}'); break;
    }
}

void kval_println(kval* v) {
    kval_print(v);
    putchar('\n');
}
