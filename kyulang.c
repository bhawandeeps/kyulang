#include "mpc.h"

#include <errno.h>
#include <math.h>
#include <stdarg.h>
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
#define KASSERT(args, cond, fmt, ...) \
  if (!(cond)) { \
    kval* err = kval_err(fmt, ##__VA_ARGS__); \
    kval_del(args); \
    return err; \
  }

//incorrect number of arguments
#define KARGCHECK(args, fmt, ...) \
    if( args->count != 1 ) { kval* kerr =  kval_err(fmt, ##__VA_ARGS__); kval_del(args); return kerr; }

//empty list
#define EMPTYCHECK(args, err) \
    if( args->cell[0]->count == 0 ) { kval_del(args); return kval_err(err); }

/* STRUCTS AND ENUMS */
struct kval;
struct kenv;
typedef struct kval kval;
typedef struct kenv kenv;

enum { KVAL_NUM, KVAL_ERR, KVAL_FUN,  KVAL_SEXPR, KVAL_QEXPR, KVAL_SYM };

typedef void(*repl_cmd)(kenv* e);

typedef kval*(*kbuiltin)(kenv*, kval*);

typedef struct kval {
    int type;
    long num;

    char* err;
    char* sym;
    char* kbuiltin_name;

    kbuiltin func;

    int count;
    struct kval** cell;
} kval;

typedef struct {
    char* name;
    repl_cmd fn;
} repl_command_t;

struct kenv {
    int count;
    char** sym;
    kval** vals;
};

/* FUNCTION DECLARATIONS */

//repl level functions
int try_repl_command(char* input, kenv* e);
void repl_printall(kenv* e);

//kenv functions
kenv* kenv_new(void);
void kenv_del(kenv* e);

kval* kenv_get(kenv* e, kval* k);
void kenv_put(kenv* e, kval* k, kval* v);

//kval functions
kval* kval_num(long x);
kval* kval_err(char* fmt, ...);
kval* kval_sym(char* s);
kval* kval_sexpr(void);
kval* kval_qexpr(void);
kval* kval_fun(kbuiltin func, char* name);

kval* kval_read_num(mpc_ast_t* t);
kval* kval_read(mpc_ast_t* t);
kval* kval_add(kval* v, kval* x);

void kval_expr_print(kval* v, char open, char close);
void kval_print(kval* v);
void kval_println(kval* v);
kval* kval_copy(kval* v);

void kval_del(kval* v);

kval* kval_eval_sexpr(kenv* e, kval* v);

kval* builtin_op(kenv* e, kval* a, char* op);

kval* kval_eval(kenv* e, kval* v);
kval* kval_take(kval* v, int i);
kval* kval_pop(kval* v, int i);
kval* kval_join(kval* x, kval* y);

//builtins
kval* builtin_add(kenv* e, kval* a);
kval* builtin_sub(kenv* e, kval* a);
kval* builtin_mul(kenv* e, kval* a);
kval* builtin_div(kenv* e, kval* a);

kval* builtin_rem(kenv* e, kval* a);
kval* builtin_pow(kenv* e, kval* a);
kval* builtin_min(kenv* e, kval* a);
kval* builtin_max(kenv* e, kval* a);

kval* builtin_head(kenv* e, kval* v);
kval* builtin_tail(kenv* e, kval* v);
kval* builtin_list(kenv* e, kval* v);
kval* builtin_eval(kenv* e, kval* v);
kval* builtin_join(kenv* e, kval* v);
kval* builtin_cons(kenv* e, kval* v);
kval* builtin_len(kenv* e, kval* v);
kval* builtin_init(kenv* e, kval* v);

void kenv_add_builtin(kenv* e, char* name, kbuiltin func);
void kenv_add_builtins(kenv* e);

kval* builtin_def(kenv* e, kval* a);

char* ktype_name(int t);

/* REPL TABLE DEFINITION  */
repl_command_t repl_commands[] = {
    { "printall", repl_printall }
};
# define REPL_COMMAND_COUNT (sizeof(repl_commands) / sizeof(repl_commands[0]))


/* MAIN CODE BLOCK */
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
            symbol  :    /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ; \
            sexpr   :   '(' <expr>* ')' ; \
            qexpr   :   '{' <expr>* '}' ; \
            expr    :   <number> | <symbol> | <sexpr> | <qexpr> ; \
            Kyulang :   /^/ <expr>* /$/ ; \
            ",
            Number, Symbol, Sexpr, Qexpr, Expr, Kyulang);

    puts("Kyulang version 0.0.0.0.7");
    puts("Press Ctrl+C to Exit\n");

    kenv* e = kenv_new();
    kenv_add_builtins(e);

    while (1) {
        char* input = readline("halo^_^ ~>");

        add_history(input);

        if (strcmp(input, "clear")) {
            puts("Tankyu and until we meet again! ^_^");
            free(input);
            break;
        }

        if (try_repl_command(input, e)) {
            free(input);
            continue;
        }

        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Kyulang, &r)) {

            kval* x = kval_eval(e, kval_read(r.output));
            kval_println(x);
            kval_del(x);

            mpc_ast_delete(r.output);
        } else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);
    }

    kenv_del(e);

    mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr,  Expr, Kyulang);

    return 0;
}

kenv* kenv_new(void) {
    kenv* e = malloc(sizeof(kenv));
    e->count = 0;
    e->sym = NULL;
    e->vals = NULL;
    return e;
}

void kenv_del(kenv* e) {
    for (int i = 0; i<e->count; i++) {
        free(e->sym[i]);
        kval_del(e->vals[i]);
    }
    free(e->sym);
    free(e->vals);
    free(e);
}

kval* builtin_op(kenv* e, kval* a, char* op) {
    for (int i = 0; i < a->count; i++) {
        if (a->cell[i]->type != KVAL_NUM) {
            char* bad_type = ktype_name(a->cell[i]->type);
            kval_del(a);
            return kval_err(">~< '%s' does not know what %s is. Use %s!",
                             op, bad_type, ktype_name(KVAL_NUM));
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


/* FUNCTION DEFINITIONS */

// repl level functions
void repl_printall(kenv* e) {
    int found = 0;

    for (int i = 0; i < e->count; i++) {
        if (e->vals[i]->type == KVAL_FUN) {
            continue;
        }
        printf("%s: ", e->sym[i]);
        kval_println(e->vals[i]);
        found = 1;
    }
    if (found == 0) {
        printf(">~< No variables exist in this session!\n");
    }
}

int try_repl_command(char* input, kenv* e) {
    for (size_t i = 0; i < REPL_COMMAND_COUNT; i++) {
        if (strcmp(input, repl_commands[i].name) == 0) {
            repl_commands[i].fn(e);
            return 1;
        }
    }
    return 0;
}
// repl level functions end

kval* kval_eval_sexpr(kenv* e, kval* v) {
    for (int i = 0; i < v->count; i++) {
        v->cell[i] = kval_eval(e, v->cell[i]);
    }

    for (int i = 0; i<v->count; i++) {
        if (v->cell[i]->type == KVAL_ERR) { return kval_take(v, i); }
    }

    if (v->count == 0) { return v; }

    if (v->count == 1) { return kval_take(v, 0); }

    kval* f = kval_pop(v, 0);
    if (f->type != KVAL_FUN) {
        kval_del(f); kval_del(v);
        return kval_err(">~< %s is not a function, dummy!", ktype_name(f->type));
    }

    kval* result = f->func(e, v);
    kval_del(f);
    return result;
}

kval* kval_eval(kenv* e, kval* v) {
    if (v->type == KVAL_SYM) {
        kval* x = kenv_get(e, v);
        return x;
    }
    if (v->type == KVAL_SEXPR) { return kval_eval_sexpr(e, v); }
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

kval* kval_err(char* fmt, ...) {
    kval* v = malloc(sizeof(kval));
    v->type = KVAL_ERR;

    va_list va;
    va_start(va, fmt);

    v->err = malloc(512);

    vsnprintf(v->err, 511, fmt, va);

    v->err = realloc(v->err, strlen(v->err) + 1);

    va_end(va);
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
kval* kval_fun(kbuiltin func, char* name) {
    kval* v = malloc(sizeof(kval));
    v->type = KVAL_FUN;
    v->func = func;
    v->kbuiltin_name = malloc(strlen(name) + 1);
    strcpy(v->kbuiltin_name, name);
    return v;
}

kval* kenv_get(kenv* e, kval* k) {
    for (int i = 0; i < e->count; i++) {
        if (strcmp(e->sym[i], k->sym) == 0) {
            return kval_copy(e->vals[i]);
        }
    }
    return kval_err(">~< Ru-oh, the symbol %s is unbound!", k->sym);
}

void kenv_put(kenv* e, kval* k, kval* v) {
    for (int i = 0; i  < e->count; i++) {
        if (strcmp(e->sym[i], k->sym) == 0) {
            kval_del(e->vals[i]);
            e->vals[i] = kval_copy(v);
            return;
        }
    }

    e->count++;
    e->vals = realloc(e->vals, sizeof(kval*) * e->count);
    e->sym = realloc(e->sym, sizeof(char*) * e->count);

    e->vals[e->count-1] = kval_copy(v);
    e->sym[e->count-1] = malloc(strlen(k->sym) + 1);
    strcpy(e->sym[e->count-1], k->sym);
}

void kval_del(kval* v) {
    switch(v->type) {
        case KVAL_NUM: break;
        case KVAL_FUN: (free(v->kbuiltin_name)); break;

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

kval* builtin_add(kenv* e, kval* a) {
  return builtin_op(e, a, "+");
}

kval* builtin_sub(kenv* e, kval* a) {
  return builtin_op(e, a, "-");
}

kval* builtin_mul(kenv* e, kval* a) {
  return builtin_op(e, a, "*");
}

kval* builtin_div(kenv* e, kval* a) {
  return builtin_op(e, a, "/");
}

kval* builtin_rem(kenv* e, kval* a) {
    return builtin_op(e, a, "%");
}

kval* builtin_pow(kenv* e, kval* a) {
    return builtin_op(e, a, "^");
}

kval* builtin_min(kenv* e, kval* a) {
    return builtin_op(e, a, "min");
}
kval* builtin_max(kenv* e, kval* a) {
    return builtin_op(e, a, "max");
}

kval* builtin_head(kenv* e, kval* v) {
    KARGCHECK(v,  ">~< 'head' cannot fit %d arguments, take it %s argument(s) at a time!", v->count, "1");
    EMPTYCHECK(v, ">~< 'head' is empty!");
    KASSERT(v, v->cell[0]->type == KVAL_QEXPR, ">~< 'head' does not like %s, use %s instead!", ktype_name(v->cell[0]->type), ktype_name(KVAL_QEXPR));

    kval* x = kval_take(v, 0);

    while (x->count > 1) {
        kval_del(kval_pop(x, 1));
    }
    return x;
}

kval* builtin_tail(kenv* e, kval* v) {
    KARGCHECK(v,  ">~< 'tail' cannot fit %d arguments, take it %s argument(s) at a time!", v->count, "1");
    EMPTYCHECK(v, ">~< 'tail' is empty!");
    KASSERT(v, v->cell[0]->type == KVAL_QEXPR, ">~< 'head' does not like %s, use %s instead!", ktype_name(v->cell[0]->type), ktype_name(KVAL_QEXPR));

    kval* x = kval_take(v, 0);

    kval_del(kval_pop(x, 0));
    return x;
}

kval* builtin_list(kenv* e, kval* v) {
    v->type = KVAL_QEXPR;
    return v;
}

kval* builtin_eval(kenv* e, kval* v) {
    KARGCHECK(v,  ">~< 'eval' cannot fit %d arguments, take it %s argument(s) at a time!", v->count, "1");
    EMPTYCHECK(v, ">~< 'eval' is empty!");

    kval* x = kval_take(v, 0);
    x->type = KVAL_SEXPR;
    return kval_eval(e, x);
}

kval* builtin_join(kenv* e, kval* v) {
    for (int i = 0; i < v->count; i++) {
        KASSERT(v, v->cell[i]->type == KVAL_QEXPR, ">~< 'join' does not like %s, use %s instead!", ktype_name(v->cell[i]->type), ktype_name(KVAL_QEXPR));
    }

    kval* x = kval_pop(v, 0);

    while (v->count) {
        x = kval_join(x, kval_pop(v, 0));
    }

    kval_del(v);
    return x;
}

kval* builtin_cons(kenv* e, kval* v) {
    KASSERT(v, v->count == 2, ">~< 'cons' needs exactly a value and a Q-Expression!");
    KASSERT(v, v->cell[1]->type == KVAL_QEXPR, ">~< 'cons' needs a Q-Expression as its second argument!");

    kval* x = kval_pop(v, 0);
    kval* q = kval_take(v, 0);

    q->count++;
    q->cell = realloc(q->cell, sizeof(kval*) * q->count);
    memmove(&q->cell[1], &q->cell[0], sizeof(kval*) * (q->count - 1));
    q->cell[0] = x;

    return q;
}

kval* builtin_len(kenv* e, kval* v) {
    KASSERT(v, v->count == 1, ">~< 'len' needs exactly one argument!");
    KASSERT(v, v->cell[0]->type == KVAL_QEXPR, ">~< 'len' does not like %s, use %s instead!", ktype_name(v->cell[0]->type), ktype_name(KVAL_QEXPR));

    kval* result = kval_num(v->cell[0]->count);
    kval_del(v);
    return result;
}

kval* builtin_init(kenv* e, kval* v) {
    KASSERT(v, v->count == 1, ">~< 'init' needs exactly one argument!");
    KASSERT(v, v->cell[0]->type == KVAL_QEXPR, ">~< 'init' does not like %s, use %s instead!", ktype_name(v->cell[0]->type), ktype_name(KVAL_QEXPR));
    KASSERT(v, v->cell[0]->count != 0, ">~< 'init' passed {}!");

    kval* x = kval_take(v, 0);
    kval_del(kval_pop(x, x->count - 1));
    return x;
}

void kenv_add_builtin(kenv* e, char* name, kbuiltin func) {
    kval* k = kval_sym(name);
    kval* v = kval_fun(func, name);
    kenv_put(e, k, v);
    kval_del(k);
    kval_del(v);
}
void kenv_add_builtins(kenv* e) {
    kenv_add_builtin(e, "list", builtin_list);
    kenv_add_builtin(e, "head", builtin_head);
    kenv_add_builtin(e, "tail", builtin_tail);
    kenv_add_builtin(e, "eval", builtin_eval);
    kenv_add_builtin(e, "join", builtin_join);
    kenv_add_builtin(e, "cons", builtin_cons);
    kenv_add_builtin(e, "len", builtin_len);
    kenv_add_builtin(e, "init", builtin_init);

    /* Mathematical Functions */
    kenv_add_builtin(e, "+", builtin_add);
    kenv_add_builtin(e, "-", builtin_sub);
    kenv_add_builtin(e, "*", builtin_mul);
    kenv_add_builtin(e, "/", builtin_div);
    kenv_add_builtin(e, "%", builtin_rem);
    kenv_add_builtin(e, "^", builtin_pow);

    kenv_add_builtin(e, "min", builtin_min);
    kenv_add_builtin(e, "max", builtin_max);

    kenv_add_builtin(e, "def", builtin_def);
}

kval* builtin_def(kenv* e, kval* a) {
 KASSERT(a, a->cell[0]->type == KVAL_QEXPR, ">~< The 'def' function does not work with that!" );

 kval* syms = a->cell[0];

 for (int i = 0; i < syms->count; i++) {
     KASSERT(a, syms->cell[i]->type == KVAL_SYM, ">~< You are trying to make 'def' dp something that it cannot!");
 }

 KASSERT(a, syms->count == a->count-1, ">~< The 'def' function cannot define incorrect number of values to symbols");

 for (int i = 0; i < syms->count; i++) {
     kenv_put(e, syms->cell[i], a->cell[i+1]);
 }

 kval_del(a);
 return kval_sexpr();
}

kval* kval_join(kval* x, kval* y) {
    while (y->count) {
        x = kval_add(x, kval_pop(y, 0));
    }

    kval_del(y);
    return x;
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

kval* kval_copy(kval* v) {
    kval* x = malloc(sizeof(kval));
    x->type = v->type;

    switch (v->type) {
        case KVAL_NUM: x->num = v->num; break;
        case KVAL_FUN:
            x->func = v->func;
            x->kbuiltin_name = malloc(strlen(v->kbuiltin_name) + 1);
            strcpy(x->kbuiltin_name, v->kbuiltin_name);
            break;

        case KVAL_SYM:
            x->sym = malloc(strlen(v->sym) + 1);
            strcpy(x->sym, v->sym); break;

        case KVAL_ERR:
            x->err = malloc(strlen(v->err) + 1);
            strcpy(x->err, v->err); break;

        case KVAL_SEXPR:
        case KVAL_QEXPR:
            x->count = v->count;
            x->cell = malloc(sizeof(kval*) * x->count);
            for (int i = 0; i<x->count; i++) {
                x->cell[i] = kval_copy(v->cell[i]);
            }
    }

    return x;
}

void kval_print(kval* v) {
    switch (v->type) {
        case KVAL_NUM:  printf("%li", v->num); break;
        case KVAL_FUN:  printf("<%s>", v->kbuiltin_name); break;
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

char* ktype_name(int t) {
  switch(t) {
    case KVAL_FUN: return "Function";
    case KVAL_NUM: return "Number";
    case KVAL_ERR: return "Error";
    case KVAL_SYM: return "Symbol";
    case KVAL_SEXPR: return "S-Expression";
    case KVAL_QEXPR: return "Q-Expression";
    default: return "Unknown";
  }
}
