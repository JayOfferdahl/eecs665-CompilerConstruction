#include <stdio.h>
#include "cc.h"
#include "semutil.h"
#include "sem.h"
#include "sym.h"
#include <stdlib.h>
#include <string.h>

extern int formalnum;
extern char formaltypes[];
extern int localnum;
extern char localtypes[];
extern int localwidths[];

int numlabels = 0;         /* total labels in file */
int numblabels = 0;        /* toal backpatch labels in file */

/*
 * backpatch - backpatch list of quadruples starting at p with k
 */
void backpatch(struct sem_rec *p, int k) {
    printf("B%i=L%i\n", p->s_place, k);
    p->s_place = k;
}

/*
 * bgnstmt - encountered the beginning of a statement
 */
void bgnstmt() {
    extern int lineno;
    printf("bgnstmt %d\n", lineno);
}

/*
 * call - procedure invocation
 */
struct sem_rec *call(char *f, struct sem_rec *args) {
    char type;
    int argCount = 0;

    // Print all the back links
    while(args != NULL) {
        type = args->s_mode & T_DOUBLE ? 'f' : 'i';
        printf("arg%c t%i\n", type, args->s_place);
        args = args->back.s_link;
        argCount++;
    }

    // All functions in C are global!
    printf("t%i := global %s\n", nexttemp(), f);
    return gen("f", node(currtemp(), 0, NULL, NULL), 
                    node(argCount, 0, NULL, NULL), 0);
}

/*
 * ccand - logical and
 */
struct sem_rec *ccand(struct sem_rec *e1, int m, struct sem_rec *e2) {
    backpatch(e1->s_false, m);
    return node(0, 0, e2->back.s_true, merge(e1->s_false, e2->s_false));
}

/*
 * ccexpr - convert arithmetic expression to logical expression
 */
struct sem_rec *ccexpr(struct sem_rec *e) {
    struct sem_rec *t1;

    if(e) {

        t1 = gen("!=", e, cast(con("0"), e->s_mode), e->s_mode);

        printf("bt t%d B%d\n", t1->s_place, ++numblabels);
        printf("br B%d\n", ++numblabels);
        return (node(0, 0,
                    node(numblabels-1, 0, (struct sem_rec *) NULL, 
                        (struct sem_rec *) NULL),
                    node(numblabels, 0, (struct sem_rec *) NULL, 
                        (struct sem_rec *) NULL)));
    }
    else
        fprintf(stderr, "Argument sem_rec is NULL\n");
}

/*
 * ccnot - logical not
 */
struct sem_rec *ccnot(struct sem_rec *e) {
    return node(0, 0, e->s_false, e->back.s_true);
}

/*
 * ccor - logical or
 */
struct sem_rec *ccor(struct sem_rec *e1, int m, struct sem_rec *e2) {
    backpatch(e1->s_false, m);
    return node(0, 0, merge(e1->back.s_true, e2->back.s_true), e2->s_false);
}

/*
 * con - constant reference in an expression
 */
struct sem_rec *con(char *x) {
    struct id_entry *p;

    if((p = lookup(x, 0)) == NULL) {
        p = install(x, 0);
        p->i_type = T_INT;
        p->i_scope = GLOBAL;
        p->i_defined = 1;
    }

    /* print the quad t%d = const */
    printf("t%i := %s\n", nexttemp(), x);

    /* construct a new node corresponding to this constant generation 
       into a temporary. This will allow this temporary to be referenced
       in an expression later*/
    return(node(currtemp(), p->i_type, (struct sem_rec *) NULL,
                (struct sem_rec *) NULL));
}

/*
 * dobreak - break statements
 */
void dobreak() {
    // Generate goto, no need to store backpatch here since it's a break
    n();

    // Leave the current scope
    leaveblock();
}

/*
 * docontinue - continue statement
 */
void docontinue() {
    n();
}

/*
 * dodo - do statement
 */
void dodo(int m1, int m2, struct sem_rec *e, int m3) {
    backpatch(e->back.s_true, m2);
    backpatch(e->s_false, m3);
}

/*
 * dofor - for statement
 */
void dofor(int m1, struct sem_rec *e2, int m2, struct sem_rec *n1,
        int m3, struct sem_rec *n2, int m4) {
    backpatch(e2->back.s_true, m3);
    backpatch(e2->s_false, m4);
    backpatch(n1, m1);
    backpatch(n2, m2);
}

/*
 * dogoto - goto statement
 */
void dogoto(char *id) {
    lookup(id, 0);
}

/*
 * doif - one-arm if statement
 */
void doif(struct sem_rec *e, int m1, int m2) {
    backpatch(e->back.s_true, m1);
    backpatch(e->s_false, m2);
}

/*
 * doifelse - if then else statement
 */
void doifelse(struct sem_rec *e, int m1, struct sem_rec *n, int m2, int m3) {
    backpatch(e->back.s_true, m1);
    backpatch(e->s_false, m2);
    backpatch(n, m3);
}

/*
 * doret - return statement
 */
void doret(struct sem_rec *e) {
    char type = 'i';

    // Determine whether this is an int, float, or addr
    if(e->s_mode & T_DOUBLE)
        type = 'f';
    else if(e->s_mode & T_INT)
        type = 'i';
    else if(e->s_mode & T_ADDR)
        type = 'a';

    printf("ret%c t%i\n", type, e->s_place);
}

/*
 * dowhile - while statement
 */
void dowhile(int m1, struct sem_rec *e, int m2, struct sem_rec *n, int m3) {
    backpatch(e->back.s_true, m2);
    backpatch(e->s_false, m3);
    backpatch(n, m1);
}

/*
 * endloopscope - end the scope for a loop
 */
void endloopscope(int m) {
    for(int i = 0; i < m; i++)
        leaveblock();
}

/*
 * exprs - form a list of expressions
 */
struct sem_rec *exprs(struct sem_rec *l, struct sem_rec *e) {
    struct sem_rec* temp = l;

    // Get to the back of the links
    while(temp->back.s_link != NULL)
        temp = temp->back.s_link;

    // Set the final s_link to e
    temp->back.s_link = e;

    return l;
}

/*
 * fhead - beginning of function body
 */
void fhead(struct id_entry *p) {
    // Parse the function parameters and print their respective sizes
    for(int i = 0; i < formalnum; i++) {
        int dclSize = formaltypes[i] == 'f' ? 8 : 4;
        printf("formal %i\n", dclSize);
    }

    // Parse the local variables and print their respective sizes
    for(int i = 0; i < localnum; i++) {
        int dclSize = localtypes[i] == 'f' ? 8 : 4;
        printf("localloc %i\n", dclSize);
    }
}

/*
 * fname - function declaration
 */
struct id_entry *fname(int t, char *id) {
    // Print the function name
    printf("func %s\n", id);

    // Update the symbol table scope
    enterblock();

    // Install this function into the symbol table
    struct id_entry* func = install(id, 0);
    func->i_type = t;
    func->i_scope = GLOBAL;

    return func;
}

/*
 * ftail - end of function body
 */
void ftail() {
    // Print fend, leave the current block
    printf("fend\n");
    leaveblock();

    // Reset variable numbers from this function
    localnum = 0;
    formalnum = 0;
}

/*
 * id - variable reference
 */
struct sem_rec *id(char *x) {
    struct id_entry *p;

    if((p = lookup(x, 0)) == NULL) {
        yyerror("undeclared identifier");
        p = install(x, -1);
        p->i_type = T_INT;
        p->i_scope = LOCAL;
        p->i_defined = 1;
    }
    if(p->i_scope == GLOBAL)
        printf("t%d := global %s\n", nexttemp(), x);
    else if(p->i_scope == LOCAL)
        printf("t%d := local %d\n", nexttemp(), p->i_offset);
    else if(p->i_scope == PARAM) {
        printf("t%d := param %d\n", nexttemp(), p->i_offset);
        if(p->i_type & T_ARRAY) {
            (void) nexttemp();
            printf("t%d := @i t%d\n", currtemp(), currtemp() - 1);
        }
    }

    /* add the T_ADDR to know that it is still an address */
    return (node(currtemp(), p->i_type|T_ADDR, (struct sem_rec *) NULL,
                (struct sem_rec *) NULL));
}

/*
 * index - subscript
 */
struct sem_rec *tom_index(struct sem_rec *x, struct sem_rec *i) {
    return (gen("[]", x, cast(i, T_INT), x->s_mode&~(T_ARRAY)));
}

/*
 * labeldcl - process a label declaration
 */
void labeldcl(char *id) {
    // Lookup this label
    slookup(id);

    // Print label statement and increment the number of labels
    m();
}

/*
 * m - generate label and return next temporary number
 */
int m() {
    // Print out line with the label number (increment label number)
    printf("label L%i\n", ++numlabels);
    return numlabels;
}

/*
 * n - generate goto and return backpatch pointer
 */
struct sem_rec *n() {
  // Print out this goto branch statement
  printf("br B%i\n", ++numblabels);

  // Generate a semantic record which holds onto this label number
  struct sem_rec* temp = (struct sem_rec *) malloc(sizeof(struct sem_rec));

  temp->s_place = numblabels;
  return temp;
}

/*
 * op1 - unary operators
 */
struct sem_rec *op1(char *op, struct sem_rec *y) {
    if(*op == '@' && !(y->s_mode & T_ARRAY)) {
        /* get rid of T_ADDR if it is being dereferenced so can handle
           T_DOUBLE types correctly */
        y->s_mode &= ~T_ADDR;
        return (gen(op, (struct sem_rec *) NULL, y, y->s_mode));
    }
    else if(*op == '~' && y->s_mode == T_DOUBLE) {
        // Cast this to int before trying to complement it, no need to reset
        // this mode to double, it will be converted later if needed
        y = cast(y, T_INT);
        y->s_mode = T_INT;
    }
    return gen(op, NULL, y, y->s_mode);
}

/*
 * op2 - arithmetic operators
 */
struct sem_rec *op2(char *op, struct sem_rec *x, struct sem_rec *y) {
    int conv = T_INT;

    // Check if a conversion is needed
    if(x->s_mode & T_DOUBLE || y->s_mode & T_DOUBLE)
        conv = T_DOUBLE;

    return gen(op, cast(x, conv), cast(y, conv), conv);
}

/*
 * opb - bitwise operators
 */
struct sem_rec *opb(char *op, struct sem_rec *x, struct sem_rec *y) {
    // If either inputs are doubles, convert to int before operating
    return gen(op, cast(x, T_INT), cast(y, T_INT), T_INT);
}

/*
 * rel - relational operators
 */
struct sem_rec *rel(char *op, struct sem_rec *x, struct sem_rec *y) {
    int conv = T_INT;

    // Check if a conversion is needed
    if(x->s_mode & T_DOUBLE || y->s_mode & T_DOUBLE)
        conv = T_DOUBLE;

    struct sem_rec * temp = gen(op, cast(x, conv), cast(y, conv), conv);

    // Branch if our temporary evaluates to true
    printf("bt t%i B%i\n", currtemp(), ++numblabels);

    // True points to the branch true label number
    temp->back.s_true = (struct sem_rec *) malloc(sizeof(struct sem_rec));
    temp->back.s_true->s_place = numblabels;

    // Else, branch to the following B label
    printf("br B%i\n", ++numblabels);

    // False points to the newly generated label for branch
    temp->s_false = (struct sem_rec *) malloc(sizeof(struct sem_rec));
    temp->s_false->s_place = numblabels;

    return temp;
}

/*
 * set - assignment operators
 */
struct sem_rec *set(char *op, struct sem_rec *x, struct sem_rec *y) {
    /* assign the value of expression y to the lval x */
    struct sem_rec *p, *cast_y;

    /* if for type consistency of x and y */
    cast_y = y;
    if((x->s_mode & T_DOUBLE) && !(y->s_mode & T_DOUBLE)){
        /*cast y to a double*/
        printf("t%d := cvf t%d\n", nexttemp(), y->s_place);
        y->s_place = currtemp();
        cast_y = node(currtemp(), T_DOUBLE, (struct sem_rec *) NULL,
                (struct sem_rec *) NULL);
    }
    else if((x->s_mode & T_INT) && !(y->s_mode & T_INT)){
        /*convert y to integer*/
        printf("t%d := cvi t%d\n", nexttemp(), y->s_place);
        y->s_place = currtemp();
        cast_y = node(currtemp(), T_INT, (struct sem_rec *) NULL,
                (struct sem_rec *) NULL);
    }

    if(*op!='\0' || x==NULL || y==NULL) {
        if(x->s_mode > T_ARRAY) {
            // Float/double array
            if(x->s_mode & T_DOUBLE) {
                printf("t%d := @f t%d\n", nexttemp(), x->s_place);
                printf("t%d := t%d %sf t%d\n", nexttemp(), currtemp(), op, y->s_place);
            }
            // Integer array
            else if(x->s_mode & T_INT) {
                printf("t%d := @i t%d\n", nexttemp(), x->s_place);
                printf("t%d := t%d %si t%d\n", nexttemp(), currtemp(), op, y->s_place);
            }
        }
    }

    // Update the s_place of cast_y in order to fix labeling issues (was 1 less than needed)
    cast_y->s_place = currtemp();

    /*output quad for assignment*/
    if(x->s_mode & T_DOUBLE)
        printf("t%d := t%d =f t%d\n", nexttemp(), 
                x->s_place, cast_y->s_place);
    else
        printf("t%d := t%d =i t%d\n", nexttemp(), 
                x->s_place, cast_y->s_place);

    /*create a new node to allow just created temporary to be referenced later */
    return(node(currtemp(), (x->s_mode&~(T_ARRAY)),
                (struct sem_rec *)NULL, (struct sem_rec *)NULL));
}

/*
 * startloopscope - start the scope for a loop
 */
void startloopscope() {
    // Enter a new scope in the symbol table
    enterblock();
}

/*
 * string - generate code for a string
 */
struct sem_rec *string(char *s) {
    // Print out the temporary for this string and increment temp with nexttemp()
    printf("t%i := %s\n", nexttemp(), s);

    // Return a semantic record holding this temporary and type of string
    return node(currtemp(), T_STR, NULL, NULL);
}

/************* Helper Functions **************/

/*
 * cast - force conversion of datum y to type t
 */
struct sem_rec *cast(struct sem_rec *y, int t) {
    if(t == T_DOUBLE && y->s_mode != T_DOUBLE)
        return (gen("cv", (struct sem_rec *) NULL, y, t));
    else if(t != T_DOUBLE && y->s_mode == T_DOUBLE)
        return (gen("cv", (struct sem_rec *) NULL, y, t));
    else
        return (y);
}

/*
 * gen - generate and return quadruple "z := x op y"
 */
struct sem_rec *gen(char *op, struct sem_rec *x, struct sem_rec *y, int t) {
    if(strncmp(op, "arg", 3) != 0 && strncmp(op, "ret", 3) != 0)
        printf("t%d := ", nexttemp());
    if(x != NULL && *op != 'f')
        printf("t%d ", x->s_place);
    printf("%s", op);
    if(t & T_DOUBLE && (!(t & T_ADDR) || (*op == '[' && *(op+1) == ']'))) {
        printf("f");
        if(*op == '%')
            yyerror("cannot %% floating-point values");
    }
    else
        printf("i");
    if(x != NULL && *op == 'f')
        printf(" t%d %d", x->s_place, y->s_place);
    else if(y != NULL)
        printf(" t%d", y->s_place);
    printf("\n");
    return (node(currtemp(), t, (struct sem_rec *) NULL,
                (struct sem_rec *) NULL));
}
