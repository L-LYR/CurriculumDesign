#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "enumVar.h"
#include "ast.h"

extern int line, poolSize; // memoryPool.c

extern char *src, *data; // tokenizer.c
extern int *currentId, *symbols, token, tokenVal;
extern void match(int tk);
extern int nextToken();

extern void initSymbolTab(); // lexer.c

extern Node *ast, *curNode, *funcMark; // ast.c
extern void *makeNode(int t);
extern void *setNode(int t);
extern void changeToFuncDef();

void *cn; // current node editor
Var *vl;  // variable list help pointer
Func *fl; // function list help pointer

int exprType; // the type of an expression
int isLvalue; // mark of lvalue
int params;   // number of parameters of function

// for virtual machine
// int idxOfBp; // point to the entry of current function

static void enumDeclaration()
{
    int i;

    i = 0;

    vl = ((EnumDecNode *)cn)->vl;

    while (token != '}')
    {
        if (token != Id)
        {
            printf("Line %d: bad enum identifier\n", line, token);
            exit(-1);
        }

        if (currentId[Class])
        {
            // redeclaration
            if (currentId[Class] == Glo)
                printf("Line %d: redeclaration of deferent type\n", line);
            else if (currentId[Class] == Num)
                printf("Line %d: redeclaration of enumerator\n", line);
            else if (currentId[Class] == Fun)
                printf("Line %d: redeclaration of deferent type\n", line);
            exit(-1);
        }

        nextToken();

        if (token == Assign) // ENUM_VAR = 100
        {
            nextToken();
            if (token != Num)
            {
                printf("Line %d: bad enum initializer\n", line);
                exit(-1);
            }
            i = tokenVal;
            nextToken();
        }
        currentId[Class] = Num;
        currentId[Type] = INT;
        currentId[Value] = i++;

        memcpy(vl->id, (char *)currentId[Name], currentId[Len]);
        vl->t = currentId[Value];
        vl++;

        if (token == ',')
            nextToken();
    }
}

static void functionParameter()
{
    int type;
    Var *p = fl->pl;
    params = 0;
    while (token != ')')
    {
        if (token == Int) // int type
            type = INT;
        else if (token == Char) // char type
            type = CHAR;
        else if (token == Void)
        {
            type = VOID;
            nextToken();
            return;
        }
        match(token);

        while (token == Mul) // pointer type
        {
            match(Mul);
            type = type + PTR;
        }

        if (token != Id)
        {
            printf("Line %d: bad parameter declaration\n", line);
            exit(-1);
        }
        if (currentId[Class])
        {
            if (currentId[Class] == Fun)
            {
                printf("Line %d: redeclaration of function\n", line);
                exit(-1);
            }
            else if (currentId[Class] == Num)
            {
                printf("Line %d: redeclaration of deferent type\n", line);
                exit(-1);
            }
        }

        match(Id);

        currentId[BClass] = currentId[Class];
        currentId[Class] = Loc;
        currentId[BType] = currentId[Type];
        currentId[Type] = type;
        currentId[BValue] = currentId[Value];
        currentId[Value] = params++;
        memcpy(p->id, (char *)currentId[Name], currentId[Len]);
        p->t = type;
        p++;
        if (token == ',')
            match(',');
    }
    // idxOfBp = params + 1;
}
// forward declaration
static ExprNode *expression(int level);

static void functionCall(FunCallNode *p)
{
    int i;
    int *fn;
    i = 0;
    fn = currentId;
    memcpy(p->n, (char *)currentId[Name], currentId[Len]);
    match('(');
    while (token != ')')
    {
        p->pl[i] = expression(Assign);
        if (token == ',')
            match(',');
        ++i;
    }
    match(')');
    if (fn[Class] != Fun)
    {
        printf("Line %d: bad function call\n", line);
        exit(-1);
    }
    else if (fn[Value] > i)
    {
        printf("Line %d: too few arguments in function call\n", line);
        exit(-1);
    }
    else if (fn[Value] < i)
    {
        printf("Line %d: too many arguments in function call\n", line);
        exit(-1);
    }
}

static void castOp(UnaryOpNode *p)
{
    char t;
    if (token == Int)
        t = INT;
    else if (token == Void)
        t = VOID;
    else if (token == Char)
        t = CHAR;
    match(token);
    while (token == Mul)
    {
        match(Mul);
        t = t + PTR;
    }
    p->op[0] = t;
    p->op[1] = 1;
}

static void preUnaryOpExpr(ExprNode *p)
{
    if (!token)
    {
        printf("Line %d: unexpected end of file\n", line);
        exit(-1);
    }

    if (token == Num)
    {
        p->t = ConstVal;
        p->n = (void *)tokenVal;
        match(Num);
        exprType = INT;
        isLvalue = 0;
    }
    else if (token == '"')
    {
        p->n = (char *)tokenVal;
        match('"');
        while (token == '"')
            match('"');
        p->t = ConstStr;
        exprType = Char + PTR;
        isLvalue = 0;
    }
    else if (token == Id)
    {
        match(Id);
        // 1. Function call
        // 2. Enum variable
        // 3. variable
        if (token == '(')
        {
            p->t = FunCall;
            p->n = makeNode(FunCall);
            functionCall(p->n);
            exprType = currentId[Type];
            isLvalue = 0;
        }
        else
        {
            if (currentId[Class] == 0)
            {
                printf("Line %d: undefined variable\n", line);
                exit(-1);
            }
            else
            {
                p->t = Variable;
                p->n = makeNode(Variable);
                ((VarNode *)(p->n))->r = currentId[Class];
                if (currentId[Class] == Num)
                {
                    ((VarNode *)(p->n))->t = currentId[Value];
                    exprType = Num;
                    isLvalue = 0;
                }
                else
                {
                    ((VarNode *)(p->n))->t = currentId[Type];
                    exprType = currentId[Type];
                    isLvalue = 1;
                }
                memcpy(((VarNode *)(p->n))->n, (char *)currentId[Name], currentId[Len]);
            }
        }
    }
    else if (token == Sizeof)
    {
        p->t = ConstVal;
        match(Sizeof);
        match('(');

        if (token == Int)
        {
            match(Int);
            exprType = INT;
        }
        else if (token == Char)
        {
            match(Char);
            exprType = CHAR;
        }
        else if (token == Void)
        {
            match(Void);
            exprType = VOID;
        }

        while (token == Mul)
        {
            match(Mul);
            exprType = exprType + PTR;
        }

        match(')');
        p->n = (void *)((exprType == CHAR || exprType == VOID) ? sizeof(char) : sizeof(int));
        exprType = INT;
        isLvalue = 0;
    }

    else if (token == '(')
    {
        match('(');
        if (token == Int || token == Char || token == Void)
        {
            p->t = PreUnary;
            p->n = makeNode(PreUnary);
            castOp(p->n);
            match(')');
            ((UnaryOpNode *)(p->n))->o = expression(Inc);
            exprType = ((UnaryOpNode *)(p->n))->op[0];
        }
        else
        {
            p->t = PreUnary;
            p->n = makeNode(PreUnary);
            ((UnaryOpNode *)(p->n))->op[0] = '(';
            ((UnaryOpNode *)(p->n))->op[1] = ')';
            ((UnaryOpNode *)(p->n))->o = expression(Assign);
            match(')');
        }
    }
    else if (token == Mul)
    {
        p->t = PreUnary;
        p->n = makeNode(PreUnary);
        ((UnaryOpNode *)(p->n))->op[0] = '*';
        match(Mul);
        ((UnaryOpNode *)(p->n))->o = expression(Inc);
        if (exprType >= PTR)
        {
            exprType -= PTR;
            isLvalue = 1;
        }
        else
        {
            printf("Line %d: bad dereference\n", line);
            exit(-1);
        }
    }
    else if (token == And)
    {
        p->t = PreUnary;
        p->n = makeNode(PreUnary);
        ((UnaryOpNode *)(p->n))->op[0] = '&';
        match(And);
        ((UnaryOpNode *)(p->n))->o = expression(Inc);
        if (isLvalue == 0)
        {
            printf("Line %d: bad address of\n", line);
            exit(-1);
        }
        exprType = exprType + PTR;
    }
    else if (token == '!')
    {
        p->t = PreUnary;
        p->n = makeNode(PreUnary);
        ((UnaryOpNode *)(p->n))->op[0] = '!';
        match('!');
        ((UnaryOpNode *)(p->n))->o = expression(Inc);
        exprType = INT;
    }
    else if (token == '~')
    {
        p->t = PreUnary;
        p->n = makeNode(PreUnary);
        ((UnaryOpNode *)(p->n))->op[0] = '~';
        match('~');
        ((UnaryOpNode *)(p->n))->o = expression(Inc);
        exprType = INT;
    }
    else if (token == Inc || token == Dec)
    {
        p->t = PreUnary;
        p->n = makeNode(PreUnary);
        if (token == Inc)
            ((UnaryOpNode *)(p->n))->op[0] =
                ((UnaryOpNode *)(p->n))->op[1] = '+';
        else
            ((UnaryOpNode *)(p->n))->op[0] =
                ((UnaryOpNode *)(p->n))->op[1] = '-';
        match(token);
        ((UnaryOpNode *)(p->n))->o = expression(Inc);
        if (isLvalue == 0)
        {
            printf("Line %d: bad lvalue of pre-operator\n", line);
            exit(-1);
        }
        isLvalue = 0; // important
    }
    else
    {
        printf("Line %d: bad expression\n", line);
        exit(-1);
    }
}

static ExprNode *binaryOpExpr(int level, ExprNode *p)
{
    int tmp;
    ExprNode *q = p, *last = NULL;
    while (token >= level)
    {
        if (q == NULL)
            q = makeNode(EXPR);
        tmp = exprType;
        if (token == Assign)
        {
            q->t = Binary;
            q->n = makeNode(Binary);
            ((BinaryOpNode *)(q->n))->op[0] = '=';
            match(Assign);
            if (isLvalue == 0)
            {
                printf("%d: bad lvalue in assignment\n", line);
                exit(-1);
            }
            ((BinaryOpNode *)(q->n))->ro = expression(Assign);
            exprType = tmp;
        }
        else if (token == Cond)
        {
            q->t = Ternary;
            q->n = makeNode(Ternary);
            match(Cond);
            ((TernaryOpNode *)(q->n))->a = expression(Assign);
            if (token == ':')
                match(':');
            else
            {
                printf("Line %d: missing delimiter\n", line);
                exit(-1);
            }
            ((TernaryOpNode *)(q->n))->b = expression(Cond);
            isLvalue = 0; // important
        }
        else if (token == Inc || token == Dec) // suffix inc(++) and dec(--)
        {
            if (isLvalue == 0)
            {
                printf("Line %d: bad value in increment\n", line);
                exit(-1);
            }
            q->t = Postfix;
            q->n = makeNode(Postfix);
            if (token == Inc)
                ((UnaryOpNode *)(q->n))->op[0] =
                    ((UnaryOpNode *)(q->n))->op[1] = '+';
            else
                ((UnaryOpNode *)(q->n))->op[0] =
                    ((UnaryOpNode *)(q->n))->op[1] = '-';
            match(token);
            isLvalue = 0; // important
        }
        else if (token == Brak)
        {
            q->t = Bracket;
            q->n = makeNode(Bracket);
            match(Brak);
            ((BinaryOpNode *)(q->n))->ro = expression(Assign);
            match(']');
            if (tmp < PTR)
            {
                printf("Line %d: pointer type expected\n", line);
                exit(-1);
            }
            exprType = tmp - PTR;
            isLvalue = 1;
        }
        else if (token == Lor)
        {
            match(Lor);
            q->t = Binary;
            q->n = makeNode(Binary);
            ((BinaryOpNode *)(q->n))->op[1] =
                ((BinaryOpNode *)(q->n))->op[0] = '|';
            ((BinaryOpNode *)(q->n))->ro = expression(Lan);
            exprType = INT;
        }
        else if (token == Lan)
        {
            match(Lan);
            q->t = Binary;
            q->n = makeNode(Binary);
            ((BinaryOpNode *)(q->n))->op[1] =
                ((BinaryOpNode *)(q->n))->op[0] = '&';
            ((BinaryOpNode *)(q->n))->ro = expression(Or);
            exprType = INT;
        }
        else if (token == Or)
        {
            match(Lan);
            q->t = Binary;
            q->n = makeNode(Binary);
            ((BinaryOpNode *)(q->n))->op[0] = '|';
            ((BinaryOpNode *)(q->n))->ro = expression(Xor);
            exprType = INT;
        }
        else if (token == Xor)
        {
            match(Xor);
            q->t = Binary;
            q->n = makeNode(Binary);
            ((BinaryOpNode *)(q->n))->op[0] = '^';
            ((BinaryOpNode *)(q->n))->ro = expression(And);
            exprType = INT;
        }
        else if (token == And)
        {
            match(And);
            q->t = Binary;
            q->n = makeNode(Binary);
            ((BinaryOpNode *)(q->n))->op[0] = '&';
            ((BinaryOpNode *)(q->n))->ro = expression(Eq);
            exprType = INT;
        }
        else if (token == Eq)
        {
            match(Eq);
            q->t = Binary;
            q->n = makeNode(Binary);
            ((BinaryOpNode *)(q->n))->op[1] =
                ((BinaryOpNode *)(q->n))->op[0] = '=';
            ((BinaryOpNode *)(q->n))->ro = expression(Ne);
            exprType = INT;
        }
        else if (token == Ne)
        {
            match(Ne);
            q->t = Binary;
            q->n = makeNode(Binary);
            ((BinaryOpNode *)(q->n))->op[0] = '!';
            ((BinaryOpNode *)(q->n))->op[1] = '=';
            ((BinaryOpNode *)(q->n))->ro = expression(Lt);
            exprType = INT;
        }
        else if (token == Lt)
        {
            match(Lt);
            q->t = Binary;
            q->n = makeNode(Binary);
            ((BinaryOpNode *)(q->n))->op[0] = '<';
            ((BinaryOpNode *)(q->n))->ro = expression(Shl);
            exprType = INT;
        }
        else if (token == Gt)
        {
            match(Gt);
            q->t = Binary;
            q->n = makeNode(Binary);
            ((BinaryOpNode *)(q->n))->op[0] = '>';
            ((BinaryOpNode *)(q->n))->ro = expression(Shl);
            exprType = INT;
        }
        else if (token == Le)
        {
            match(Le);
            q->t = Binary;
            q->n = makeNode(Binary);
            ((BinaryOpNode *)(q->n))->op[0] = '<';
            ((BinaryOpNode *)(q->n))->op[1] = '=';
            ((BinaryOpNode *)(q->n))->ro = expression(Shl);
            exprType = INT;
        }
        else if (token == Ge)
        {
            match(Ge);
            q->t = Binary;
            q->n = makeNode(Binary);
            ((BinaryOpNode *)(q->n))->op[0] = '>';
            ((BinaryOpNode *)(q->n))->op[1] = '=';
            ((BinaryOpNode *)(q->n))->ro = expression(Shl);
            exprType = INT;
        }
        else if (token == Shl)
        {
            match(Shl);
            q->t = Binary;
            q->n = makeNode(Binary);
            ((BinaryOpNode *)(q->n))->op[0] =
                ((BinaryOpNode *)(q->n))->op[1] = '<';
            ((BinaryOpNode *)(q->n))->ro = expression(Add);
            exprType = INT;
        }
        else if (token == Shr)
        {
            match(Shr);
            q->t = Binary;
            q->n = makeNode(Binary);
            ((BinaryOpNode *)(q->n))->op[0] =
                ((BinaryOpNode *)(q->n))->op[1] = '>';
            ((BinaryOpNode *)(q->n))->ro = expression(Add);
            exprType = INT;
        }
        else if (token == Add)
        {
            match(Add);
            q->t = Binary;
            q->n = makeNode(Binary);
            ((BinaryOpNode *)(q->n))->op[0] = '+';
            ((BinaryOpNode *)(q->n))->ro = expression(Mul);
            exprType = tmp;
        }
        else if (token == Sub)
        {
            match(Sub);
            q->t = Binary;
            q->n = makeNode(Binary);
            ((BinaryOpNode *)(q->n))->op[0] = '-';
            ((BinaryOpNode *)(q->n))->ro = expression(Mul);
            if (tmp > PTR)
            {
                if (tmp == exprType) // pointer subtraction
                    exprType = INT;
                else
                    exprType = tmp; // pointer movement
            }
            else
                exprType = tmp; // numeral substraction
        }
        else if (token == Mul)
        {
            match(Mul);
            q->t = Binary;
            q->n = makeNode(Binary);
            ((BinaryOpNode *)(q->n))->op[0] = '*';
            ((BinaryOpNode *)(q->n))->ro = expression(Inc);
            exprType = tmp;
        }
        else if (token == Div)
        {
            match(Div);
            q->t = Binary;
            q->n = makeNode(Binary);
            ((BinaryOpNode *)(q->n))->op[0] = '/';
            ((BinaryOpNode *)(q->n))->ro = expression(Inc);
            exprType = tmp;
        }
        else if (token == Mod)
        {
            match(Mod);
            q->t = Binary;
            q->n = makeNode(Binary);
            ((BinaryOpNode *)(q->n))->op[0] = '%';
            ((BinaryOpNode *)(q->n))->ro = expression(Inc);
            exprType = tmp;
        }
        else
        {
            printf("Line %d: compiler error, token = %c\n", line, token);
            exit(-1);
        }

        if (last != NULL)
        {
            if (q->t == Binary || q->t == Bracket)
                ((BinaryOpNode *)(q->n))->lo = last;
            else if (q->t == Ternary)
                ((TernaryOpNode *)(q->n))->c = last;
            else if (q->t == Postfix)
                ((UnaryOpNode *)(q->n))->o = last;
        }
        last = q;
        q = NULL;
    }
    return last;
}

static ExprNode *expression(int level)
{
    ExprNode *last, *p, *q;
    p = makeNode(EXPR);
    preUnaryOpExpr(p);
    q = makeNode(EXPR);
    last = binaryOpExpr(level, q);
    if (q->t == 0)
        return p;
    else
    {
        if (q->t == Binary || q->t == Bracket)
            ((BinaryOpNode *)(q->n))->lo = p;
        else if (q->t == Ternary)
            ((TernaryOpNode *)(q->n))->c = p;
        else if (q->t == Postfix)
            ((UnaryOpNode *)(q->n))->o = p;
        return last;
    }
}
// forward declaration
static void statement(StateNode *p);
static void ifStm(IfNode *p)
{
    match(If);
    match('(');
    p->c = expression(Assign);
    match(')');

    statement(&(p->a));
    if (token == Else)
    {
        match(Else);
        statement(&(p->b));
    }
    else
    {
        p->b.t = EMPTY;
        p->b.n = NULL;
    }
}

static void whileStm(WhileNode *p)
{
    match(While);

    match('(');
    p->c = expression(Assign);
    match(')');

    statement(&(p->s));
}

static void blockStm(BlockNode *p)
{
    StateNode *sl = p->sl;
    match('{');
    while (token != '}')
    {
        statement(sl);
        sl++;
    }
    match('}');
}

static void statement(StateNode *p)
{
    p->l = line;
    if (token == If)
    {
        p->t = IF;
        p->n = makeNode(IF);
        ifStm(p->n);
    }
    else if (token == While)
    {
        p->t = WHILE;
        p->n = makeNode(WHILE);
        whileStm(p->n);
    }
    else if (token == '{')
    {
        p->t = BLOCK;
        p->n = makeNode(BLOCK);
        blockStm(p->n);
    }
    else if (token == ';')
    {
        p->t = EMPTY;
        p->n = NULL;
        match(';');
    }
    else if (token == Break)
    {
        match(Break);
        p->t = BREAK;
        p->n = NULL;
        match(';');
    }
    else if (token == Continue)
    {
        match(Continue);
        p->t = CONTINUE;
        p->n = NULL;
        match(';');
    }
    else if (token == Return)
    {
        p->t = RETURN;
        match(Return);
        if (token != ';')
            p->n = expression(Assign);
        match(';');
    }
    else
    {
        p->t = EXPR;
        p->n = expression(Assign);
        match(';');
    }
}

static void functionBody()
{
    int baseType;
    int type;
    LocDecNode *ln;
    while (token != '}')
    {
        if (token == Int || token == Char || token == Void)
        {
            if (token == Int)
                baseType = INT;
            else if (token == Void)
                baseType = VOID;
            else if (token == Char)
                baseType = CHAR;

            match(token);

            ln = setNode(LocDec);
            vl = ln->vl;
            ln->l = line;
            while (token != ';')
            {
                type = baseType;
                while (token == Mul)
                {
                    match(Mul);
                    type = type + PTR;
                }

                if (token != Id)
                {
                    printf("Line %d: bad local declaration\n", line);
                    exit(-1);
                }

                if (currentId[Class])
                {
                    if (currentId[Class] == Num)
                    {
                        printf("Line %d: redeclaration of different type\n", line);
                        exit(-1);
                    }
                    if (currentId[Class] == Loc)
                    {
                        printf("Line %d: redeclaration of local variable\n", line);
                        exit(-1);
                    }
                }

                match(Id);

                // store the local variable
                currentId[BClass] = currentId[Class];
                currentId[Class] = Loc;
                currentId[BType] = currentId[Type];
                currentId[Type] = type;
                currentId[BValue] = currentId[Value];
                // currentId[Value] = ++(position of local ps); // index of current parameter

                memcpy(vl->id, (char *)currentId[Name], currentId[Len]);
                vl->t = type;
                vl++;

                if (token == ',')
                    match(',');
            }
            match(';');
        }
        else
        {
            cn = setNode(Statement);
            statement(cn);
        }
    }

    // exit function
    // cover the local variables in function body with global variables which have the same name
    currentId = symbols;
    while (currentId[Token])
    {
        if (currentId[Class] == Loc)
        {
            currentId[Class] = currentId[BClass];
            currentId[Type] = currentId[BType];
            currentId[Value] = currentId[BValue];
        }
        currentId = currentId + IdSize;
    }
}

static void globalDeclaration()
{
    int baseType;
    int type;
    int i;
    int *tmp;

    if (token == Enum)
    {
        cn = setNode(EnumDec);

        match(Enum);
        if (token != '{')
        {
            match(Id);
            memcpy(((EnumDecNode *)cn)->id, (char *)currentId[Name], currentId[Len]);
        }

        if (token == '{')
        {
            match('{');
            enumDeclaration();
            match('}');
        }
        match(';');
        return;
    }

    // parse type name
    // Example: int | char [*]
    if (token == Int)
        baseType = INT;
    else if (token == Char)
        baseType = CHAR;
    else if (token == Void)
        baseType = VOID;
    match(token);

    cn = setNode(GloDec);
    vl = ((GloDecNode *)cn)->vl;
    fl = ((GloDecNode *)cn)->fl;

    // parse the comma seperated variable declaration
    // Example: int [*] a, b, ... ;
    while (token != ';' && token != '}')
    {
        type = baseType;

        while (token == Mul)
        {
            match(Mul);
            type = type + PTR;
        }
        if (token != Id)
        {
            // invalid declaration
            printf("Line %d: bad global declaration\n", line);
            exit(-1);
        }

        // redeclaration
        if (currentId[Class] == Num)
        {
            printf("Line %d: redeclaration of deferent type\n", line);
            exit(-1);
        }
        if (currentId[Class] == Glo)
        {
            printf("Line %d: redeclaration of global variable\n", line);
            exit(-1);
        }

        match(Id);
        currentId[Type] = type;
        if (token == '(')
        {
            currentId[Class] = Fun;
            tmp = currentId;
            memcpy(fl->id, (char *)currentId[Name], currentId[Len]);
            fl->rt = type;

            // currentId[Value] = (int)(text + 1); // memory address of function or entry
            match('(');
            functionParameter();
            tmp[Value] = params;
            match(')');

            fl++;
            if (token == '{')
            {
                match('{');
                changeToFuncDef();
                functionBody();
            }
        }
        else
        {
            if (type == VOID)
            {
                printf("Line %d: variable cannot have void type\n", line);
                exit(-1);
            }
            currentId[Class] = Glo; // global variable
            // currentId[Value] = (int)data; // assign memory
            // data = data + sizeof(int);

            memcpy(vl->id, (char *)currentId[Name], currentId[Len]);
            vl->t = type;
            vl++;
        }
        if (token == ',')
            match(',');
    }
    nextToken();
}

void syntaxAnalysis()
{
    initSymbolTab();
    curNode = ast;

    nextToken();
    while (token > 0)
        globalDeclaration();
}