/*
 * Adapted version for C89 compatibility.
 * 
 * This is a modified version of the original TinyExpr library, adapted for
 * use with old C89 compilers on DOS systems (e.g., Turbo C, DJGPP, Open Watcom).
 * The modifications ensure compatibility with ANSI C 1989 standards without
 * relying on C99 features.
 * 
 * Key changes:
 * - Removed macros NEW_EXPR and CHECK_NULL (which used C99 __VA_ARGS__ and 
 *   compound literals).
 * - Replaced with direct calls to new_expr using fixed-size arrays for 
 *   parameters.
 * - Added manual NULL checks and memory freeing in expression-building 
 *   functions (base, power, factor, term, expr, list) to handle errors 
 *   without macros.
 * - Moved variable declarations to the beginning of blocks for C89 
 *   compliance (e.g., in fac, ncr, find_builtin, optimize, etc.).
 * - Minor adjustments: Removed "Falls through" comments, added temporary 
 *   variables for state preservation, and used explicit variable scopes.
 * 
 * No functional changes to the parsing or evaluation logic were made; only 
 * structural adaptations for compatibility.
 * 
 * Original library: TinyExpr by Lewis Van Winkle 
 * (https://github.com/codeplea/tinyexpr)
 * Adaptation by: Dmitriy Eliseev
 * Date: 2025-07-10
 * 
 * This is an altered source version and not the original software.
 * All changes are provided under the same Zlib license.
 */

/* SPDX-License-Identifier: Zlib */
/*
 * TINYEXPR - Tiny recursive descent parser and evaluation engine in C
 *
 * Copyright (c) 2015-2020 Lewis Van Winkle
 *
 * http://CodePlea.com
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 * claim that you wrote the original software. If you use this software
 * in a product, an acknowledgement in the product documentation would be
 * appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 * misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

 
/* COMPILE TIME OPTIONS */

/* Exponentiation associativity:
For a^b^c = (a^b)^c and -a^b = (-a)^b do nothing.
For a^b^c = a^(b^c) and -a^b = -(a^b) uncomment the next line.*/
/* #define TE_POW_FROM_RIGHT */

/* Logarithms
For log = base 10 log do nothing
For log = natural log uncomment the next line. */
/* #define TE_NAT_LOG */

#include "tinyexpr.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>

#ifndef NAN
#define NAN (0.0/0.0)
#endif

#ifndef INFINITY
#define INFINITY (1.0/0.0)
#endif


typedef double (*te_fun2)(double, double);

enum {
    TOK_NULL = TE_CLOSURE7+1, TOK_ERROR, TOK_END, TOK_SEP,
    TOK_OPEN, TOK_CLOSE, TOK_NUMBER, TOK_VARIABLE, TOK_INFIX
};


enum {TE_CONSTANT = 1};


typedef struct state {
    const char *start;
    const char *next;
    int type;
    union {double value; const double *bound; const void *function;};
    void *context;

    const te_variable *lookup;
    int lookup_len;
} state;


#define TYPE_MASK(TYPE) ((TYPE)&0x0000001F)

#define IS_PURE(TYPE) (((TYPE) & TE_FLAG_PURE) != 0)
#define IS_FUNCTION(TYPE) (((TYPE) & TE_FUNCTION0) != 0)
#define IS_CLOSURE(TYPE) (((TYPE) & TE_CLOSURE0) != 0)
#define ARITY(TYPE) ( ((TYPE) & (TE_FUNCTION0 | TE_CLOSURE0)) ? ((TYPE) & 0x00000007) : 0 )

static te_expr *new_expr(const int type, const te_expr *parameters[]) {
    int arity = ARITY(type);
    int psize = sizeof(void*) * arity;
    int size = (sizeof(te_expr) - sizeof(void*)) + psize + (IS_CLOSURE(type) ? sizeof(void*) : 0);
    te_expr *ret = malloc(size);

    if (ret == NULL) {
        return NULL;
    }

    memset(ret, 0, size);
    if (arity && parameters) {
        memcpy(ret->parameters, parameters, psize);
    }
    ret->type = type;
    ret->bound = 0;
    return ret;
}


void te_free_parameters(te_expr *n) {
    if (!n) return;
    switch (TYPE_MASK(n->type)) {
        case TE_FUNCTION7: case TE_CLOSURE7: te_free(n->parameters[6]);
        case TE_FUNCTION6: case TE_CLOSURE6: te_free(n->parameters[5]);
        case TE_FUNCTION5: case TE_CLOSURE5: te_free(n->parameters[4]);
        case TE_FUNCTION4: case TE_CLOSURE4: te_free(n->parameters[3]);
        case TE_FUNCTION3: case TE_CLOSURE3: te_free(n->parameters[2]);
        case TE_FUNCTION2: case TE_CLOSURE2: te_free(n->parameters[1]);
        case TE_FUNCTION1: case TE_CLOSURE1: te_free(n->parameters[0]);
    }
}


void te_free(te_expr *n) {
    if (!n) return;
    te_free_parameters(n);
    free(n);
}


static double pi(void) {return 3.14159265358979323846;}
static double e(void) {return 2.71828182845904523536;}
static double fac(double a) {/* simplest version of fac */
    unsigned long int result = 1, i;
    unsigned int ua;
    if (a < 0.0)
        return NAN;
    if (a > UINT_MAX)
        return INFINITY;
    ua = (unsigned int)(a);
    for (i = 1; i <= ua; i++) {
        if (i > ULONG_MAX / result)
            return INFINITY;
        result *= i;
    }
    return (double)result;
}
static double ncr(double n, double r) {
    unsigned long int un, ur, i;
    unsigned long int result = 1;
    if (n < 0.0 || r < 0.0 || n < r) return NAN;
    if (n > UINT_MAX || r > UINT_MAX) return INFINITY;
    un = (unsigned int)(n);
    ur = (unsigned int)(r);
    if (ur > un / 2) ur = un - ur;
    for (i = 1; i <= ur; i++) {
        if (result > ULONG_MAX / (un - ur + i))
            return INFINITY;
        result *= un - ur + i;
        result /= i;
    }
    return result;
}
static double npr(double n, double r) {return ncr(n, r) * fac(r);}

#ifdef _MSC_VER
#pragma function (ceil)
#pragma function (floor)
#endif

static const te_variable functions[] = {
    /* must be in alphabetical order */
    {"abs", fabs,     TE_FUNCTION1 | TE_FLAG_PURE, 0},
    {"acos", acos,    TE_FUNCTION1 | TE_FLAG_PURE, 0},
    {"asin", asin,    TE_FUNCTION1 | TE_FLAG_PURE, 0},
    {"atan", atan,    TE_FUNCTION1 | TE_FLAG_PURE, 0},
    {"atan2", atan2,  TE_FUNCTION2 | TE_FLAG_PURE, 0},
    {"ceil", ceil,    TE_FUNCTION1 | TE_FLAG_PURE, 0},
    {"cos", cos,      TE_FUNCTION1 | TE_FLAG_PURE, 0},
    {"cosh", cosh,    TE_FUNCTION1 | TE_FLAG_PURE, 0},
    {"e", e,          TE_FUNCTION0 | TE_FLAG_PURE, 0},
    {"exp", exp,      TE_FUNCTION1 | TE_FLAG_PURE, 0},
    {"fac", fac,      TE_FUNCTION1 | TE_FLAG_PURE, 0},
    {"floor", floor,  TE_FUNCTION1 | TE_FLAG_PURE, 0},
    {"ln", log,       TE_FUNCTION1 | TE_FLAG_PURE, 0},
#ifdef TE_NAT_LOG
    {"log", log,      TE_FUNCTION1 | TE_FLAG_PURE, 0},
#else
    {"log", log10,    TE_FUNCTION1 | TE_FLAG_PURE, 0},
#endif
    {"log10", log10,  TE_FUNCTION1 | TE_FLAG_PURE, 0},
    {"ncr", ncr,      TE_FUNCTION2 | TE_FLAG_PURE, 0},
    {"npr", npr,      TE_FUNCTION2 | TE_FLAG_PURE, 0},
    {"pi", pi,        TE_FUNCTION0 | TE_FLAG_PURE, 0},
    {"pow", pow,      TE_FUNCTION2 | TE_FLAG_PURE, 0},
    {"sin", sin,      TE_FUNCTION1 | TE_FLAG_PURE, 0},
    {"sinh", sinh,    TE_FUNCTION1 | TE_FLAG_PURE, 0},
    {"sqrt", sqrt,    TE_FUNCTION1 | TE_FLAG_PURE, 0},
    {"tan", tan,      TE_FUNCTION1 | TE_FLAG_PURE, 0},
    {"tanh", tanh,    TE_FUNCTION1 | TE_FLAG_PURE, 0},
    {0, 0, 0, 0}
};

static const te_variable *find_builtin(const char *name, int len) {
    int imin, imax, i, c;
    imin = 0;
    imax = sizeof(functions) / sizeof(te_variable) - 2;

    /*Binary search.*/
    while (imax >= imin) {
        i = (imin + ((imax-imin)/2));
        c = strncmp(name, functions[i].name, len);
        if (!c) c = '\0' - functions[i].name[len];
        if (c == 0) {
            return functions + i;
        } else if (c > 0) {
            imin = i + 1;
        } else {
            imax = i - 1;
        }
    }

    return 0;
}

static const te_variable *find_lookup(const state *s, const char *name, int len) {
    int iters;
    const te_variable *var;
    if (!s->lookup) return 0;

    for (var = s->lookup, iters = s->lookup_len; iters; ++var, --iters) {
        if (strncmp(name, var->name, len) == 0 && var->name[len] == '\0') {
            return var;
        }
    }
    return 0;
}



static double add(double a, double b) {return a + b;}
static double sub(double a, double b) {return a - b;}
static double mul(double a, double b) {return a * b;}
static double divide(double a, double b) {return a / b;}
static double negate(double a) {return -a;}
static double comma(double a, double b) {(void)a; return b;}


void next_token(state *s) {
    s->type = TOK_NULL;

    do {

        if (!*s->next){
            s->type = TOK_END;
            return;
        }

        /* Try reading a number. */
        if ((s->next[0] >= '0' && s->next[0] <= '9') || s->next[0] == '.') {
            s->value = strtod(s->next, (char**)&s->next);
            s->type = TOK_NUMBER;
        } else {
            /* Look for a variable or builtin function call. */
            if (isalpha(s->next[0])) {
                const char *start;
                const te_variable *var;
                start = s->next;
                while (isalpha(s->next[0]) || isdigit(s->next[0]) || (s->next[0] == '_')) s->next++;

                var = find_lookup(s, start, s->next - start);
                if (!var) var = find_builtin(start, s->next - start);

                if (!var) {
                    s->type = TOK_ERROR;
                } else {
                    switch(TYPE_MASK(var->type))
                    {
                        case TE_VARIABLE:
                            s->type = TOK_VARIABLE;
                            s->bound = var->address;
                            break;

                        case TE_CLOSURE0: case TE_CLOSURE1: case TE_CLOSURE2: case TE_CLOSURE3:
                        case TE_CLOSURE4: case TE_CLOSURE5: case TE_CLOSURE6: case TE_CLOSURE7:
                            s->context = var->context;

                        case TE_FUNCTION0: case TE_FUNCTION1: case TE_FUNCTION2: case TE_FUNCTION3:
                        case TE_FUNCTION4: case TE_FUNCTION5: case TE_FUNCTION6: case TE_FUNCTION7:
                            s->type = var->type;
                            s->function = var->address;
                            break;
                    }
                }

            } else {
                /* Look for an operator or special character. */
                switch (s->next++[0]) {
                    case '+': s->type = TOK_INFIX; s->function = add; break;
                    case '-': s->type = TOK_INFIX; s->function = sub; break;
                    case '*': s->type = TOK_INFIX; s->function = mul; break;
                    case '/': s->type = TOK_INFIX; s->function = divide; break;
                    case '^': s->type = TOK_INFIX; s->function = pow; break;
                    case '%': s->type = TOK_INFIX; s->function = fmod; break;
                    case '(': s->type = TOK_OPEN; break;
                    case ')': s->type = TOK_CLOSE; break;
                    case ',': s->type = TOK_SEP; break;
                    case ' ': case '\t': case '\n': case '\r': break;
                    default: s->type = TOK_ERROR; break;
                }
            }
        }
    } while (s->type == TOK_NULL);
}


static te_expr *list(state *s);
static te_expr *expr(state *s);
static te_expr *power(state *s);

static te_expr *base(state *s) {
    te_expr *ret;
    int arity, i;
    const te_expr *params[7];
    /* ADAPTATION: Added temporary variables to store the function state. */
    int f_type;
    const void* f_func;
    void* f_context;

    switch (TYPE_MASK(s->type)) {
        case TOK_NUMBER:
            ret = new_expr(TE_CONSTANT, 0);
            if (ret == NULL) return NULL;
            ret->value = s->value;
            next_token(s);
            break;

        case TOK_VARIABLE:
            ret = new_expr(TE_VARIABLE, 0);
            if (ret == NULL) return NULL;
            ret->bound = s->bound;
            next_token(s);
            break;

        case TE_FUNCTION0:
        case TE_CLOSURE0:
            ret = new_expr(s->type, 0);
            if (ret == NULL) return NULL;
            ret->function = s->function;
            if (IS_CLOSURE(s->type)) ret->parameters[0] = s->context;
            next_token(s);
            if (s->type == TOK_OPEN) {
                next_token(s);
                if (s->type != TOK_CLOSE) {
                    s->type = TOK_ERROR;
                } else {
                    next_token(s);
                }
            }
            break;

        case TE_FUNCTION1:
        case TE_CLOSURE1:
            /* ADAPTATION: Store the function state before processing arguments. */
            f_type = s->type;
            f_func = s->function;
            if (IS_CLOSURE(f_type)) f_context = s->context;

            next_token(s);
            params[0] = power(s);
            if(params[0] == NULL) return NULL;
            
            /* ADAPTATION: Use the stored type. */
            ret = new_expr(f_type, params);
            if(ret == NULL) {
                te_free((te_expr*)params[0]);
                return NULL;
            }
            
            /* ADAPTATION: Use the stored pointers. */
            ret->function = f_func;
            if (IS_CLOSURE(f_type)) ret->parameters[1] = f_context;
            break;

        case TE_FUNCTION2: case TE_FUNCTION3: case TE_FUNCTION4:
        case TE_FUNCTION5: case TE_FUNCTION6: case TE_FUNCTION7:
        case TE_CLOSURE2: case TE_CLOSURE3: case TE_CLOSURE4:
        case TE_CLOSURE5: case TE_CLOSURE6: case TE_CLOSURE7:
            arity = ARITY(s->type);

            /* ADAPTATION: Store the function state before processing arguments. */
            f_type = s->type;
            f_func = s->function;
            if (IS_CLOSURE(f_type)) f_context = s->context;

            next_token(s);

            if (s->type != TOK_OPEN) {
                s->type = TOK_ERROR;
                ret = NULL; /* Error, exiting. */
            } else {
                for(i = 0; i < arity; i++) {
                    next_token(s);
                    params[i] = expr(s);
                    if(params[i] == NULL) {
                        /* Free the already created parameters. */
                        for (i--; i >= 0; i--) te_free((te_expr*)params[i]);
                        return NULL;
                    }
                    if(s->type != TOK_SEP) {
                        break;
                    }
                }
                if(s->type != TOK_CLOSE || i != arity - 1) {
                    s->type = TOK_ERROR;
                    /* Free all parameters in case of error. */
                    for (i = ARITY(f_type) - 1; i >= 0; i--) te_free((te_expr*)params[i]);
                    ret = NULL; /* Error, exiting. */
                } else {
                    next_token(s);
                    /* ADAPTATION: Use the stored type. */
                    ret = new_expr(f_type, params);
                    if(ret == NULL) {
                        for (i = arity - 1; i >= 0; i--) te_free((te_expr*)params[i]);
                        return NULL;
                    }
                    /* ADAPTATION: Use the stored pointers. */
                    ret->function = f_func;
                    if (IS_CLOSURE(f_type)) ret->parameters[arity] = f_context;
                }
            }
            break;

        case TOK_OPEN:
            next_token(s);
            ret = list(s);
            if(ret == NULL) return NULL;

            if (s->type != TOK_CLOSE) {
                s->type = TOK_ERROR;
            } else {
                next_token(s);
            }
            break;

        default:
            ret = new_expr(0, 0);
            if(ret == NULL) return NULL;
            s->type = TOK_ERROR;
            ret->value = NAN;
            break;
    }

    return ret;
}


static te_expr *power(state *s) {
    int sign = 1;
    te_expr *ret;
    const te_expr *params[1];

    while (s->type == TOK_INFIX && (s->function == add || s->function == sub)) {
        if (s->function == sub) sign = -sign;
        next_token(s);
    }

    if (sign == 1) {
        ret = base(s);
    } else {
        te_expr *b = base(s);
        if(b == NULL) return NULL;

        params[0] = b;
        ret = new_expr(TE_FUNCTION1 | TE_FLAG_PURE, params);
        if(ret == NULL){
            te_free(b);
            return NULL;
        }
        ret->function = negate;
    }

    return ret;
}

#ifdef TE_POW_FROM_RIGHT
static te_expr *factor(state *s) {
    te_expr *ret;
    int neg;
    te_expr *se, *p, *insert, *insertion, *prev;
    te_fun2 t;
    const te_expr *params[2];

    ret = power(s);
    if (ret == NULL) return NULL;

    neg = 0;

    if (ret->type == (TE_FUNCTION1 | TE_FLAG_PURE) && ret->function == negate) {
        se = ret->parameters[0];
        free(ret);
        ret = se;
        neg = 1;
    }

    insertion = 0;

    while (s->type == TOK_INFIX && (s->function == pow)) {
        t = s->function;
        next_token(s);

        if (insertion) {
            p = power(s);
            if(p == NULL) { te_free(ret); return NULL; }

            params[0] = insertion->parameters[1];
            params[1] = p;
            insert = new_expr(TE_FUNCTION2 | TE_FLAG_PURE, params);
            if(insert == NULL) { te_free(p); te_free(ret); return NULL; }

            insert->function = t;
            insertion->parameters[1] = insert;
            insertion = insert;
        } else {
            p = power(s);
            if (p == NULL) { te_free(ret); return NULL; }
            
            prev = ret;
            params[0] = prev;
            params[1] = p;
            ret = new_expr(TE_FUNCTION2 | TE_FLAG_PURE, params);
            if (ret == NULL) { te_free(p); te_free(prev); return NULL; }

            ret->function = t;
            insertion = ret;
        }
    }

    if (neg) {
        const te_expr* neg_param[1];
        prev = ret;
        neg_param[0] = prev;
        ret = new_expr(TE_FUNCTION1 | TE_FLAG_PURE, neg_param);
        if (ret == NULL) { te_free(prev); return NULL; }
        ret->function = negate;
    }

    return ret;
}
#else
static te_expr *factor(state *s) {
    te_expr *ret, *p, *prev;
    te_fun2 t;
    const te_expr *params[2];

    ret = power(s);
    if (ret == NULL) return NULL;

    while (s->type == TOK_INFIX && (s->function == pow)) {
        t = s->function;
        next_token(s);
        p = power(s);
        if (p == NULL) { te_free(ret); return NULL; }

        prev = ret;
        params[0] = prev;
        params[1] = p;
        ret = new_expr(TE_FUNCTION2 | TE_FLAG_PURE, params);
        if (ret == NULL) { te_free(p); te_free(prev); return NULL; }

        ret->function = t;
    }

    return ret;
}
#endif


static te_expr *term(state *s) {
    te_expr *ret, *f, *prev;
    te_fun2 t;
    const te_expr *params[2];

    ret = factor(s);
    if (ret == NULL) return NULL;

    while (s->type == TOK_INFIX && (s->function == mul || s->function == divide || s->function == fmod)) {
        t = s->function;
        next_token(s);
        f = factor(s);
        if (f == NULL) { te_free(ret); return NULL; }

        prev = ret;
        params[0] = prev;
        params[1] = f;
        ret = new_expr(TE_FUNCTION2 | TE_FLAG_PURE, params);
        if (ret == NULL) { te_free(f); te_free(prev); return NULL; }
        
        ret->function = t;
    }

    return ret;
}


static te_expr *expr(state *s) {
    te_expr *ret, *te, *prev;
    te_fun2 t;
    const te_expr *params[2];
    
    ret = term(s);
    if (ret == NULL) return NULL;

    while (s->type == TOK_INFIX && (s->function == add || s->function == sub)) {
        t = s->function;
        next_token(s);
        te = term(s);
        if (te == NULL) { te_free(ret); return NULL; }

        prev = ret;
        params[0] = prev;
        params[1] = te;
        ret = new_expr(TE_FUNCTION2 | TE_FLAG_PURE, params);
        if (ret == NULL) { te_free(te); te_free(prev); return NULL; }
        
        ret->function = t;
    }

    return ret;
}


static te_expr *list(state *s) {
    te_expr *ret, *e, *prev;
    const te_expr *params[2];
    
    ret = expr(s);
    if (ret == NULL) return NULL;

    while (s->type == TOK_SEP) {
        next_token(s);
        e = expr(s);
        if (e == NULL) { te_free(ret); return NULL; }

        prev = ret;
        params[0] = prev;
        params[1] = e;
        ret = new_expr(TE_FUNCTION2 | TE_FLAG_PURE, params);
        if (ret == NULL) { te_free(e); te_free(prev); return NULL; }
        
        ret->function = comma;
    }

    return ret;
}

/* Typedefs for function pointers to ensure C89 compatibility */
typedef double (*te_fun0)(void);
typedef double (*te_fun1)(double);
typedef double (*te_fun2)(double, double);
typedef double (*te_fun3)(double, double, double);
typedef double (*te_fun4)(double, double, double, double);
typedef double (*te_fun5)(double, double, double, double, double);
typedef double (*te_fun6)(double, double, double, double, double, double);
typedef double (*te_fun7)(double, double, double, double, double, double, double);

typedef double (*te_clo0)(void*);
typedef double (*te_clo1)(void*, double);
typedef double (*te_clo2)(void*, double, double);
typedef double (*te_clo3)(void*, double, double, double);
typedef double (*te_clo4)(void*, double, double, double, double);
typedef double (*te_clo5)(void*, double, double, double, double, double);
typedef double (*te_clo6)(void*, double, double, double, double, double, double);
typedef double (*te_clo7)(void*, double, double, double, double, double, double, double);


double te_eval(const te_expr *n) {
    if (!n) return NAN;

    switch(TYPE_MASK(n->type)) {
        case TE_CONSTANT: return n->value;
        case TE_VARIABLE: return *n->bound;

        case TE_FUNCTION0: case TE_FUNCTION1: case TE_FUNCTION2: case TE_FUNCTION3:
        case TE_FUNCTION4: case TE_FUNCTION5: case TE_FUNCTION6: case TE_FUNCTION7:
            switch(ARITY(n->type)) {
                case 0: return ((te_fun0)n->function)();
                case 1: return ((te_fun1)n->function)(te_eval(n->parameters[0]));
                case 2: return ((te_fun2)n->function)(te_eval(n->parameters[0]), te_eval(n->parameters[1]));
                case 3: return ((te_fun3)n->function)(te_eval(n->parameters[0]), te_eval(n->parameters[1]), te_eval(n->parameters[2]));
                case 4: return ((te_fun4)n->function)(te_eval(n->parameters[0]), te_eval(n->parameters[1]), te_eval(n->parameters[2]), te_eval(n->parameters[3]));
                case 5: return ((te_fun5)n->function)(te_eval(n->parameters[0]), te_eval(n->parameters[1]), te_eval(n->parameters[2]), te_eval(n->parameters[3]), te_eval(n->parameters[4]));
                case 6: return ((te_fun6)n->function)(te_eval(n->parameters[0]), te_eval(n->parameters[1]), te_eval(n->parameters[2]), te_eval(n->parameters[3]), te_eval(n->parameters[4]), te_eval(n->parameters[5]));
                case 7: return ((te_fun7)n->function)(te_eval(n->parameters[0]), te_eval(n->parameters[1]), te_eval(n->parameters[2]), te_eval(n->parameters[3]), te_eval(n->parameters[4]), te_eval(n->parameters[5]), te_eval(n->parameters[6]));
                default: return NAN;
            }

        case TE_CLOSURE0: case TE_CLOSURE1: case TE_CLOSURE2: case TE_CLOSURE3:
        case TE_CLOSURE4: case TE_CLOSURE5: case TE_CLOSURE6: case TE_CLOSURE7:
            switch(ARITY(n->type)) {
                case 0: return ((te_clo0)n->function)(n->parameters[0]);
                case 1: return ((te_clo1)n->function)(n->parameters[1], te_eval(n->parameters[0]));
                case 2: return ((te_clo2)n->function)(n->parameters[2], te_eval(n->parameters[0]), te_eval(n->parameters[1]));
                case 3: return ((te_clo3)n->function)(n->parameters[3], te_eval(n->parameters[0]), te_eval(n->parameters[1]), te_eval(n->parameters[2]));
                case 4: return ((te_clo4)n->function)(n->parameters[4], te_eval(n->parameters[0]), te_eval(n->parameters[1]), te_eval(n->parameters[2]), te_eval(n->parameters[3]));
                case 5: return ((te_clo5)n->function)(n->parameters[5], te_eval(n->parameters[0]), te_eval(n->parameters[1]), te_eval(n->parameters[2]), te_eval(n->parameters[3]), te_eval(n->parameters[4]));
                case 6: return ((te_clo6)n->function)(n->parameters[6], te_eval(n->parameters[0]), te_eval(n->parameters[1]), te_eval(n->parameters[2]), te_eval(n->parameters[3]), te_eval(n->parameters[4]), te_eval(n->parameters[5]));
                case 7: return ((te_clo7)n->function)(n->parameters[7], te_eval(n->parameters[0]), te_eval(n->parameters[1]), te_eval(n->parameters[2]), te_eval(n->parameters[3]), te_eval(n->parameters[4]), te_eval(n->parameters[5]), te_eval(n->parameters[6]));
                default: return NAN;
            }

        default: return NAN;
    }
}

#undef TE_FUN
#undef M

static void optimize(te_expr *n) {
    int arity;
    int known;
    int i;
    double value;
    
    /* Evaluates as much as possible. */
    if (n->type == TE_CONSTANT) return;
    if (n->type == TE_VARIABLE) return;

    /* Only optimize out functions flagged as pure. */
    if (IS_PURE(n->type)) {
        arity = ARITY(n->type);
        known = 1;
        for (i = 0; i < arity; ++i) {
            optimize(n->parameters[i]);
            if (((te_expr*)(n->parameters[i]))->type != TE_CONSTANT) {
                known = 0;
            }
        }
        if (known) {
            value = te_eval(n);
            te_free_parameters(n);
            n->type = TE_CONSTANT;
            n->value = value;
        }
    }
}


te_expr *te_compile(const char *expression, const te_variable *variables, int var_count, int *error) {
    state s;
    te_expr *root;
    
    s.start = s.next = expression;
    s.lookup = variables;
    s.lookup_len = var_count;

    next_token(&s);
    root = list(&s);
    if (root == NULL) {
        if (error) *error = -1;
        return NULL;
    }

    if (s.type != TOK_END) {
        te_free(root);
        if (error) {
            *error = (s.next - s.start);
            if (*error == 0) *error = 1;
        }
        return 0;
    } else {
        optimize(root);
        if (error) *error = 0;
        return root;
    }
}


double te_interp(const char *expression, int *error) {
    te_expr *n;
    double ret;
    
    n = te_compile(expression, 0, 0, error);
    if (n == NULL) {
        return NAN;
    }

    if (n) {
        ret = te_eval(n);
        te_free(n);
    } else {
        ret = NAN;
    }
    return ret;
}

static void pn (const te_expr *n, int depth) {
    int i, arity;
    printf("%*s", depth, "");

    switch(TYPE_MASK(n->type)) {
    case TE_CONSTANT: printf("%f\n", n->value); break;
    case TE_VARIABLE: printf("bound %p\n", n->bound); break;

    case TE_FUNCTION0: case TE_FUNCTION1: case TE_FUNCTION2: case TE_FUNCTION3:
    case TE_FUNCTION4: case TE_FUNCTION5: case TE_FUNCTION6: case TE_FUNCTION7:
    case TE_CLOSURE0: case TE_CLOSURE1: case TE_CLOSURE2: case TE_CLOSURE3:
    case TE_CLOSURE4: case TE_CLOSURE5: case TE_CLOSURE6: case TE_CLOSURE7:
         arity = ARITY(n->type);
         printf("f%d", arity);
         for(i = 0; i < arity; i++) {
             printf(" %p", n->parameters[i]);
         }
         printf("\n");
         for(i = 0; i < arity; i++) {
             pn(n->parameters[i], depth + 1);
         }
         break;
    }
}


void te_print(const te_expr *n) {
    pn(n, 0);
}
