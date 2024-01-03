//#define usepow

#include <asm-generic/errno-base.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef usepow
#include <math.h>
#endif
#include "string.h"
#include "vector.h"

#define lenof(x) sizeof(x)/sizeof(*x)
#define notvar L" \t\n\r-+/*\"$()[]{}^.?#=<>;"
#define tablen 4

struct tree;
union baum;
struct strct;

void parseCommand(int argc, char** args);
void error(str substr, int offset, char* msg);
void berror(str substr, int offset, char* msg);
__int128 ieval(str s, struct tree this);
double feval(str s, struct tree this, int byteindex);
char contains(int c, int* chars);
void addVar(struct tree* this, struct strct strct);
void printnum(unsigned __int128 x);
char doesprinttree(struct tree* this);
void printtree(struct tree* this, int ident);
void freetree(struct tree* t);

enum type
{
    i8,
    i16,
    i32,
    i64,
    i128,

    u8,
    u16,
    u32,
    u64,
    u128,

    x8,
    x16,
    x32,
    x64,
    x128,

    f32,
    f64,

    c8,
    c16,
    c32,

    s8,
    s16,
    s32,
};

struct prevar
{
    str plocation;//prefix
    str pinline;//prefix
    str type;
    str name;
    str value;
};

struct strct
{
    str name;
    vec entries;//struct prevar*
};

struct unassignedvar
{
    char _type;//2
    char* name;
    enum type type;
    unsigned __int128 _value;
};

struct assignedvar
{
    char _type;//3
    char* name;
    enum type type;
    unsigned __int128 _value;
};

struct tree
{
    char _type;//1
    char* name;
    vec entries;//union baum*
    char pinline;//prefix
};

union baum//Alle Einträge müssen 32 Byte groß sein.
{
    struct tree tree;
    struct unassignedvar unassignedvar;
    struct assignedvar assigned;
};

FILE* input;
str structstr;
char endian = 'l';
char ignoreend = 'n';
struct tree root;
vec structs;//struct strct*
vec stacktrace;//int

long potenzen[] = {1000000000000000000, 
                   100000000000000000, 
                   10000000000000000, 
                   1000000000000000, 
                   100000000000000, 
                   10000000000000, 
                   1000000000000, 
                   100000000000, 
                   10000000000, 
                   1000000000, 
                   100000000, 
                   10000000, 
                   1000000, 
                   100000, 
                   10000, 
                   1000, 
                   100, 
                   10, 
                   1, 
                   0};

void main(int argc, char** args)
{
    parseCommand(argc, args);
    char comment = -2;//-2=none, -1=singleline, 0>=multiline und comment=Anzahl der '\n's
    int lasti = 0;
    for(int i = 0; i < structstr->len; i++)
    {
        switch(comment)
        {
            case -2:
            {
                if(structstr->values[i] == '/')
                {
                    if(i < structstr->len)
                    {
                        switch(structstr->values[++i])
                        {
                            case '*':
                            {
                                lasti = i-1;
                                comment = 0;
                            }
                            break;
                            
                            case '/':
                            {
                                lasti = i-1;
                                comment = -1;
                            }
                            break;
                        }
                    }
                }
            }
            break;

            case -1:
            {
                if(structstr->values[i] == '\n')
                {
                    str t;
                    s_init_const_cstr(t, "");
                    s_replace_range(structstr, lasti, i, t);
                    comment = -2;
                    i = lasti;
                }
            }
            break;

            default:
            {
                if(structstr->values[i] == '\n')
                {
                    comment++;
                }
                if(structstr->values[i] == '*')
                {
                    if(++i < structstr->len)
                    {
                        if(structstr->values[i] == '/')
                        {
                            string t;
                            t.cap = 0;
                            t.len = comment;
                            t.values = malloc(t.len*sizeof(t.values));
                            for(int i = 0; i < t.len; i++)  t.values[i] = '\n';
                            s_replace_range(structstr, lasti, i+1, &t);
                            free(t.values);
                            i = lasti+comment-1;
                            comment = -2;
                        }
                    }
                }
            }
            break;
        }
    }
    if(comment >= 0)
        error(structstr, structstr->len-1, "Unterminated multiline comment");
    if(comment == -1)
    {
        str t;
        s_init_const_cstr(t, "");
        s_replace_range(structstr, lasti, structstr->len, t);
    }
    
    vec structlines = v_init(void*);
    char inbracket = 0;
    lasti = 0;
    for(int i = 0; i < structstr->len; i++)
    {
        if(structstr->values[i] == '{')
        {
            if(inbracket)
                error(structstr, i, "Double '{'");
            inbracket = 1;
        }
        else if(structstr->values[i] == '}')
        {
            if(!inbracket)
                error(structstr, i, "Unmatching '}'");
            inbracket = 0;
        }
        else if(structstr->values[i] == ';' && !inbracket)
        {
            str x = s_substring(structstr, lasti, i-lasti);
            v_push(structlines, x);
            lasti = i+1;
        }
    }
    str x = s_substring(structstr, lasti, structstr->len-lasti-1);
    for(int i = 0; i < x->len; i++) if(!contains(x->values[i], L" \t\n\r")) error(x, i, "Expected ';'");
    free(x);

    vec prevars = v_init(void*);
    structs = v_init(void*);
    for(int i = 0; i < structlines->len; i++)
    {
        str x = v_get(structlines, i, str);
        while(1)
        {
            if(contains(*x->values, L" \t\n\r"))
            {
                x->values++;
                x->len--;
            }
            else
                break;
        }
        while(1)
        {
            if(contains(*(x->values+x->len-1), L" \t\n\r"))
            {
                x->len--;
            }
            else
                break;
        }

        if(x->values[0] == '#')
        {
            if(x->len == 3)
            {
                switch (x->values[1])
                {
                    case 'e':
                    {
                        if(x->values[2] == 'h' || x->values[2] == 'l')
                            endian = x->values[2];
                        else
                            error(x, 2, "Invalid argument");
                    }
                    break;
                    
                    case 'i':
                    {
                        if(x->values[2] == 'y' || x->values[2] == 'n')
                            ignoreend = x->values[2];
                        else
                            error(x, 2, "Invalid argument");
                    }
                    break;
                    
                    default:
                    {
                        error(x, 1, "Invalid parameter");
                    }
                }
            }
            else
                error(x, 0, "The syntax for a parameter is:#pa;");
            continue;
        }
        str t;
        s_init_const_cstr(t, "struct");
        char res = s_cmp(x, t);
        if(res == 0 || res == 1)
        {
            vec entries = v_init(void*);
            str name = malloc(sizeof(string));
            name->cap = 0;
            int* index = x->values;
            index += 6;
            if(!contains(*index, L" \t\n\r"))
                goto after;
            index++;
            for(; contains(*index, L" \t\n\r"); index++) if(index-x->values >= x->len) error(x, index-x->values, "Expected struct name");
            name->values = index;
            for(; !contains(*index, notvar); index++) if(index-x->values >= x->len) error(x, index-x->values, "Expected '{'");
            name->len = index-name->values;
            for(; *index != '{'; index++)
            {
                if(index-x->values >= x->len) error(x, index-x->values, "Expected '{'");
                if(!contains(*index, L" \t\n\r")) error(x, index-x->values, "Unallowed struct name");
            }
            index++;
            while(1)
            {
                for(; contains(*index, L" \t\n\r"); index++) if(index-x->values >= x->len) error(x, index-x->values, "Unterminated '{'");
                if(*index == '}')
                    break;

                str pinline = NULL;
                str plocation = NULL;
                if(index[0] == '(')
                {
                    index++;
                    string s;
                    s.cap = 0;
                    s.values = index;
                    s.len = 0;
                    int ident = 1;
                    while(1)
                    {
                        if(s.values[s.len] == '(')
                            ident++;
                        if(s.values[s.len] == ')')
                        {
                            if(ident > 1)
                                ident--;
                            else
                                break;
                        }
                        s.len++;
                    }
                    while(s.len > 0)
                    {
                        if(s_cmp_str(&s, "location=") == 1)
                        {
                            if(plocation)
                                error(&s, 0, "Double definition of the \"location\" prefix");
                            s.values += 9;
                            s.len -= 9;
                            str t = malloc(sizeof(*t));
                            t->cap = 0;
                            t->values = s.values;
                            t->len = 0;
                            int ident = 0;
                            while(1)
                            {
                                if(t->values[t->len] == '(')
                                    ident++;
                                if(t->values[t->len] == ')')
                                {
                                    if(ident > 0)
                                        ident--;
                                    else
                                        break;
                                }
                                if(ident == 0 && t->values[t->len] == ',')
                                    break;
                                t->len++;
                            }
                            plocation = t;
                            s.values += t->len+1;
                            s.len -= t->len+1;
                        }
                        else if(s_cmp_str(&s, "inline=") == 1)
                        {
                            if(pinline)
                                error(&s, 0, "Double definition of the \"inline\" prefix");
                            s.values += 7;
                            s.len -= 7;
                            str t = malloc(sizeof(*t));
                            t->cap = 0;
                            t->values = s.values;
                            t->len = 0;
                            int ident = 0;
                            while(1)
                            {
                                if(t->values[t->len] == '(')
                                    ident++;
                                if(t->values[t->len] == ')')
                                {
                                    if(ident > 1)
                                        ident--;
                                    else
                                        break;
                                }
                                if(ident == 1 && t->values[t->len] == ',')
                                    break;
                                t->len++;
                            }
                            pinline = t;
                            s.values += t->len+1;
                            s.len -= t->len+1;
                        }
                        else
                            error(&s, 0, "Invalid prefix");
                    }
                    index = s.values + s.len + 1;
                    for(; contains(*index, L" \t\n\r"); index++) if(index-x->values >= x->len) error(x, index-x->values, "Unterminated '{'");
                }

                str typename = malloc(sizeof(string));
                typename->cap = 0;
                typename->values = index;
                for(; !contains(*index, L" \t\n\r"); index++)
                {
                    if(index-x->values >= x->len) error(x, index-x->values, "Unterminated '{'");
                    if(*index == '}') error(x, index-x->values, "Expected name after type");
                }
                typename->len = index-typename->values;
                for(; contains(*index, L" \t\n\r"); index++) if(index-x->values >= x->len) error(x, index-x->values, "Unterminated '{'");
                if(*index == '}') error(x, index-x->values, "Expected name after type");

                str name = malloc(sizeof(string));
                name->cap = 0;
                name->values = index;
                for(; !contains(*index, notvar); index++)
                {
                    if(index-x->values >= x->len) error(x, index-x->values, "Unterminated '{'");
                    if(*index == '}') error(x, index-x->values, "Expected ';'");
                }
                name->len = index-name->values;
                for(; contains(*index, L" \t\n\r"); index++) if(index-x->values >= x->len) error(x, index-x->values, "Unterminated '{'");
                if(*index == ';')
                {
                    index++;
                    struct prevar* var = malloc(sizeof(*var));
                    var->pinline = pinline;
                    var->plocation = plocation;
                    var->type = typename;
                    var->name = name;
                    var->value = NULL;
                    v_push(entries, var);
                    continue;
                }
                if(*index != '=') error(x, index-x->values, "Expected '=' or ';' after variable name (maybe due to an invalid variable name)");
                index++;
                for(; contains(*index, L" \t\n\r"); index++) if(index-x->values >= x->len) error(x, index-x->values, "Unterminated '{'");
                if(*index == '}') error(x, index-x->values, "Expected value after '='");

                str value = malloc(sizeof(string));
                value->cap = 0;
                value->values = index;
                for(; *index != ';'; index++)
                {
                    if(index-x->values >= x->len) error(x, index-x->values, "Unterminated '{'");
                    if(*index == '}') error(x, index-x->values, "Expected ';'");
                    if(*index == '\n') error(x, index-x->values, "Unallowed newline at value argument");
                }

                value->len = index-value->values;
                index++;
                struct prevar* var = malloc(sizeof(*var));
                var->pinline = pinline;
                var->plocation = plocation;
                var->type = typename;
                var->name = name;
                var->value = value;
                v_push(entries, var);
            }

            index++;
            for(; contains(*index, L" \t\n\r"); index++);
            if(*index != ';')
                error(structstr, index-structstr->values, "Expected ';'");
            struct strct* s = malloc(sizeof(*s));
            s->name = name;
            s->entries = entries;
            v_push(structs, s);
        }
        else
        {
            after:;
            int* index = x->values;
            str plocation = NULL;
            str pinline = NULL;

            if(index[0] == '(')
            {
                index++;
                string s;
                s.cap = 0;
                s.values = index;
                s.len = 0;
                int ident = 1;
                while(1)
                {
                    if(s.values[s.len] == '(')
                        ident++;
                    if(s.values[s.len] == ')')
                    {
                        if(ident > 1)
                            ident--;
                        else
                            break;
                    }
                    s.len++;
                }
                while(s.len > 0)
                {
                    if(s_cmp_str(&s, "location=") == 1)
                    {
                        if(plocation)
                            error(&s, 0, "Double definition of the \"location\" prefix");
                        s.values += 9;
                        s.len -= 9;
                        str t = malloc(sizeof(*t));
                        t->cap = 0;
                        t->values = s.values;
                        t->len = 0;
                        int ident = 0;
                        while(1)
                        {
                            if(t->values[t->len] == '(')
                                ident++;
                            if(t->values[t->len] == ')')
                            {
                                if(ident > 0)
                                    ident--;
                            }
                            if(ident == 0 && t->values[t->len] == ',')
                                break;
                            t->len++;
                        }
                        plocation = t;
                        s.values += t->len+1;
                        s.len -= t->len+1;
                    }
                    else if(s_cmp_str(&s, "inline=") == 1)
                    {
                        if(pinline)
                            error(&s, 0, "Double definition of the \"inline\" prefix");
                        s.values += 7;
                        s.len -= 7;
                        str t = malloc(sizeof(*t));
                        t->cap = 0;
                        t->values = s.values;
                        t->len = 0;
                        int ident = 0;
                        while(1)
                        {
                            if(t->values[t->len] == '(')
                                ident++;
                            if(t->values[t->len] == ')')
                            {
                                if(ident > 1)
                                    ident--;
                                else
                                    break;
                            }
                            if(ident == 1 && t->values[t->len] == ',')
                                break;
                            t->len++;
                        }
                        pinline = t;
                        s.values += t->len+1;
                        s.len -= t->len+1;
                    }
                    else
                        error(&s, 0, "Invalid prefix");
                }
                index = s.values + s.len + 1;
                for(; contains(*index, L" \t\n\r"); index++) if(index-x->values >= x->len) error(x, index-x->values, "Unterminated '{'");
            }

            str typename = malloc(sizeof(string));
            typename->cap = 0;
            typename->values = index;
            for(; !contains(*index, L" \t\n\r"); index++)
            {
                if(index-x->values >= x->len) error(x, index-x->values, "Expected name after type");
            }
            typename->len = index-typename->values;
            for(; contains(*index, L" \t\n\r"); index++) if(index-x->values >= x->len) error(x, index-x->values, "Expected name after type");

            str name = malloc(sizeof(string));
            name->cap = 0;
            name->values = index;
            for(; !contains(*index, notvar); index++);
            name->len = index-name->values;
            for(; contains(*index, L" \t\n\r"); index++);
            if(*index == ';')
            {
                index++;
                struct prevar* var = malloc(sizeof(*var));
                var->pinline = pinline;
                var->plocation = plocation;
                var->type = typename;
                var->name = name;
                var->value = NULL;
                v_push(prevars, var);
                continue;
            }
            if(*index != '=') error(x, index-x->values, "Expected '=' or ';' after variable name");
            index++;
            for(; contains(*index, L" \t\n\r"); index++) if(index-x->values >= x->len) error(x, index-x->values, "Expected value after '='");

            str value = malloc(sizeof(string));
            value->cap = 0;
            value->values = index;
            for(; *index != ';'; index++)
            {
                if(*index == '\n') error(x, index-x->values, "Unallowed newline at value argument");
            }

            value->len = index-value->values;
            struct prevar* var = malloc(sizeof(*var));
            var->pinline = pinline;
            var->plocation = plocation;
            var->type = typename;
            var->name = name;
            var->value = value;
            v_push(prevars, var);
        }
    }

    for(int i = 0; i < structlines->len; i++)
    {
        str s = ((str*)structlines->values)[i];
        free(s);
    }
    free(structlines->values);
    free(structlines);

    struct strct t;
    t.entries = prevars;
    stacktrace = v_init(int);
    addVar(&root, t);

    int readlen = ftell(input);
    if(ignoreend == 'n')
        if(fgetc(input) != -1)
            berror(structstr, structstr->len-1, "Unexpected bytes after end");

    fclose(input);

    for(int i = 0; i < prevars->len; i++)
    {
        struct prevar* p = ((struct prevar**)prevars->values)[i];
        free(p->pinline);
        free(p->plocation);
        free(p->type);
        free(p->name);
        free(p->value);
        free(p);
    }
    free(prevars->values);
    free(prevars);

    for(int i = 0; i < structs->len; i++)
    {
        struct strct* s = ((struct strct**)structs->values)[i];
        free(s->name);
        for(int i = 0; i < s->entries->len; i++)
        {
            struct prevar* p = ((struct prevar**)s->entries->values)[i];
            free(p->pinline);
            free(p->plocation);
            free(p->type);
            free(p->name);
            free(p->value);
            free(p);
        }
        free(s->entries->values);
        free(s->entries);
        free(s);
    }
    free(structs->values);
    free(structs);

    v_free(stacktrace);

    printtree(&root, 0);
    if(ignoreend == 'y')
    {
        printf("Read %i/0x%X bytes\n", readlen, readlen);
    }


    /*
    //To free everything at the end.
    freetree(&root);
    s_free(structstr);
    //*/
}

void parseCommand(int argc, char** args)
{
    for(int i = 1; i < argc; i++)
    {
        if(!strcmp(args[i], "-h"))
        {
            printf("hexedit 2.0\n\n"
                   "Arguments:\n"
                   "    -e[h/l]     sets the endian to high/low default:low\n"
                   "    -i[y/n]     ignore bytes after end? default:no\n"
                   "    -s          the path to the struct file\n\n"
                   "You need to give a struct path and a binary path (without prefix).\n"
                   "The binary file can be a file, a device or a pipe. When using /dev/stdin you cannot use the \"location\" prefix\n"
                   "The struct file stream must be seekable\n"
                   "Hexstruct doesn't distinguish between stream errors. (So the error message for a permission error is invalid path.)\n"
                   "You can set the parameters endian and ignore-end in the code with:#pa; (don't forget the ';')\n"
                   "Where p is e/i and a is h/l/y/n\n\n"
                   "You can use structs to build new types.\n\n"
                   "Variable definitions have the following syntax:\n"
                   "    type name;\n"
                   "    type name[expession];\n"
                   "    type name[start][end];  start:incl, end:excl\n"
                   "    type name = expression;\n\n"
                   "Struct definitions have the following syntax:\n"
                   "    \"struct\" name { variabledefinition1;variabledefinition2;... };\n\n"
                   "The standart types are:i8,i16,i32,i64,i128,u8,u16,u32,u64,u128,x8,x16,x32,x64,x128,f32,f64,c8,c16,c32,s8,s16,s32\n"
                   "The f... types can't be assigned.(No error but undefined value when assigning an integer and error when assigning a float)\n"
                   "The array types can't be assigned.(including s... types)\n\n"
                   "Expressions can have the following operands:+(unary&binary),-(unary&binary),*,/,^(power, evaluation right to left, only when \"usepow\" is defined in the sourcecode),<,<=,==,>,>=,&&,||,^^(xor)\n"
                   "There isn't any error detection for expressions. So be sure that the absolute value of any operation is below 2^127.\n"
                   "Therefore u128, x128, c128 and s128 are treated as signed."
                   "You can also use initialized local and global variables.\n"
                   "To do that, just type the global path into the expression or use \"this.\" to use a local variable.\n"
                   "To get the value of an array element, just type: name[expression]\n\n"
                   "Variables are initialized from top to bottom.\n"
                   "You can access the current file address in an expression with \"here\". (before the current variable)"
                   "To hide a variable the name must start with an underscore. But you can still use it in an expression.\n"
                   "Variables can have prefixes before the type name. Variable definitions then have the following syntax:\n"
                   "    (prefix1=value, prefix2=value ,...) type name;\n"
                   "The following prefixes are available:\n"
                   "    inline      The values of a variable of type struct replace the variable. (But the paths aren't inline.)\n"
                   "    location    sets the absolute location of the variable. Doesn't affect the byte pointer\n\n"
                   "Numbers can have the following prefixes (case insensitive):\n"
                   "    b   binary\n"
                   "    o   octal\n"
                   "    d   duodecimal\n"
                   "    h   hexadecimal\n\n"
                   "\"true\" and \"false\" (case sensitive) just mean 1 and 0.\n\n"
                   "Return:\n"
                   "    0   success\n"
                   "    1   something went wrong\n");
            exit(0);
        }
        else if(!strcmp(args[i], "-s"))
        {
            i++;
            if(i >= argc)
            {
                printf("hexstruct: \"-s\" expects an argument\n");
                exit(1);
            }
            FILE* stream = fopen(args[i], "r");
            if(!stream)
            {
                printf("hexstruct: invalid struct path\n");
                exit(1);
            }
            fseek(stream, 0, SEEK_END);
            long len = ftell(stream)+1;
            fseek(stream, 0, SEEK_SET);
            char* tstructstr = malloc(len);
            fread(tstructstr, len-1, 1, stream);
            tstructstr[len-1] = 0;
            fclose(stream);
            structstr = s_init_cstr(tstructstr);
            free(tstructstr);
        }
        else if(!strcmp(args[i], "-el"))
        {
            endian = 'l';
        }
        else if(!strcmp(args[i], "-eh"))
        {
            endian = 'h';
        }
        else if(!strcmp(args[i], "-iy"))
        {
            ignoreend = 'y';
        }
        else if(!strcmp(args[i], "-in"))
        {
            ignoreend = 'n';
        }
        else
        {
            FILE* stream = fopen(args[i], "rb");
            if(!stream)
            {
                printf("hexstruct: invalid binary path\n");
                exit(1);
            }
            input = stream;
        }
    }
}

void error(str substr, int offset, char* msg)
{
    int i = 1, j = 0;
    int linestart = 0;
    if((unsigned long)(substr->values+offset-structstr->values) <= (unsigned long)structstr->len)
    {
        for(; j < substr->values+offset-structstr->values; j++)
        {
            if(structstr->values[j] == L'\n')
            {
                i++;
                linestart = j;
            }
        }
        for(; j < structstr->len; j++) if(structstr->values[j] == '\n') break;
    }
    string s;
    s.len = j-linestart;
    s.cap = 0;
    s.values = structstr->values+linestart+1;
    str reals = s_init_string(&s);
    s_replace(reals, "\t", "    ");
    fprintf(stderr, "hexstruct: %s\n%5i |%.*ls\n", msg, i, (int)reals->len, reals->values);
    exit(1);
}

void berror(str substr, int offset, char* msg)
{
    int i = 1, j = 0;
    int linestart = 0;
    if((unsigned long)(substr->values+offset-structstr->values) <= (unsigned long)structstr->len)
    {
        for(; j < substr->values+offset-structstr->values; j++)
        {
            if(structstr->values[j] == L'\n')
            {
                i++;
                linestart = j;
            }
        }
        for(; j < structstr->len; j++) if(structstr->values[j] == '\n') break;
    }

    string s;
    s.len = j-linestart;
    s.cap = 0;
    s.values = structstr->values+linestart+1;
    str reals = s_init_string(&s);
    s_replace(reals, "\t", "    ");

    int byteindex = ftell(input);
    int inputlen;
    if(fseek(input, 0, SEEK_END) == ESPIPE)
        inputlen = -1;
    else
    {
        inputlen = ftell(input);
    }
    if(stacktrace->len)
    {
        if(inputlen == -1)
            fprintf(stderr, "hexstruct: %s at Byte %i (Stacktrace:", msg, byteindex);
        else
            fprintf(stderr, "hexstruct: %s at Byte %i/-%i (Stacktrace:", msg, byteindex, inputlen-byteindex);
        fprintf(stderr, "%i", v_get(stacktrace, 0, int));
        for(int i = 1; i < stacktrace->len; i++)
            fprintf(stderr, ",%i", v_get(stacktrace, i, int));
        fprintf(stderr, ")\n%5i |%*.ls\n", i, (int)reals->len, reals->values);
    }
    else
    {
        if(inputlen == -1)
            fprintf(stderr, "hexstruct: %s at Byte %i\n%5i |%.*ls\n", msg, byteindex, i, (int)reals->len, reals->values);
        else
            fprintf(stderr, "hexstruct: %s at Byte %i/-%i\n%5i |%.*ls\n", msg, byteindex, inputlen-byteindex, i, (int)reals->len, reals->values);
    }
    exit(1);
}

__int128 ieval(str s, struct tree this)
{
    for(; contains(s->values[0], L" \t\n\r") && s->len > 0; s->values++, s->len--);
    for(; contains(s->values[s->len-1], L" \t\n\r") && s->len > 0; s->len--);
    while(s->values[0] == '(' && s->values[s->len-1] == ')')
    {
        int ident = 1;
        for(int i = 1; i < s->len-1; i++)
        {
            if(contains(s->values[i], L"(["))
                ident++;
            if(contains(s->values[i], L")]"))
            {
                if(ident > 1)
                    ident--;
                else
                    goto after2;
            }
        }
        s->values++;
        s->len -= 2;
    }
    after2:;
    int ident = 0;
    for(int i = s->len-1; i >= 1; i--)
    {
        if(contains(s->values[i], L"(["))
        {
            if(ident > 0)
                ident--;
            else
                error(s, i, "Too many '(' or '['");
        }
        if(contains(s->values[i], L")]"))
            ident++;
        if(ident == 0)
        {
            if(s->values[i] == '&')
            {
                if(s->values[i-1] == '&')
                {
                    str s1 = s_substring(s, 0, i-1);
                    str s2 = s_substring(s, i+1, s->len-i-1);
                    __int128 x = ieval(s1, this) && ieval(s2, this);
                    free(s1);
                    free(s2);
                    return x;
                }
                error(s, i, "'&' isn't a valid operator");
            }
            else if(s->values[i] == '|')
            {
                if(s->values[i-1] == '|')
                {
                    str s1 = s_substring(s, 0, i-1);
                    str s2 = s_substring(s, i+1, s->len-i-1);
                    __int128 x = ieval(s1, this) && ieval(s2, this);
                    free(s1);
                    free(s2);
                    return x;
                }
                error(s, i, "'|' isn't a valid operator");
            }
            else if(s->values[i] == '^')
            {
                if(s->values[i-1] == '^')
                {
                    str s1 = s_substring(s, 0, i-1);
                    str s2 = s_substring(s, i+1, s->len-i-1);
                    __int128 x = ieval(s1, this) && ieval(s2, this);
                    free(s1);
                    free(s2);
                    return x;
                }
                error(s, i, "'^' isn't a valid operator");
            }
        }
    }
    ident = 0;
    for(int i = s->len-1; i >= 1; i--)
    {
        if(contains(s->values[i], L"(["))
        {
            if(ident > 0)
                ident--;
            else
                error(s, i, "Too many '(' or '['");
        }
        if(contains(s->values[i], L")]"))
            ident++;
        if(ident == 0)
        {
            if(s->values[i] == '<')
            {
                str s1 = s_substring(s, 0, i);
                str s2 = s_substring(s, i+1, s->len-i-1);
                __int128 x = ieval(s1, this) < ieval(s2, this);
                free(s1);
                free(s2);
                return x;
            }
            else if(s->values[i] == '>')
            {
                str s1 = s_substring(s, 0, i);
                str s2 = s_substring(s, i+1, s->len-i-1);
                __int128 x = ieval(s1, this) > ieval(s2, this);
                free(s1);
                free(s2);
                return x;
            }
            else if(s->values[i] == '=')
            {
                if(--i >= 0)
                {
                    if(s->values[i] == '=')
                    {
                        str s1 = s_substring(s, 0, i);
                        str s2 = s_substring(s, i+2, s->len-i-2);
                        __int128 x = ieval(s1, this) == ieval(s2, this);
                        free(s1);
                        free(s2);
                        return x;
                    }
                    else if(s->values[i] == '>')
                    {
                        str s1 = s_substring(s, 0, i);
                        str s2 = s_substring(s, i+2, s->len-i-2);
                        __int128 x = ieval(s1, this) >= ieval(s2, this);
                        free(s1);
                        free(s2);
                        return x;
                    }
                    else if(s->values[i] == '<')
                    {
                        str s1 = s_substring(s, 0, i);
                        str s2 = s_substring(s, i+2, s->len-i-2);
                        __int128 x = ieval(s1, this) <= ieval(s2, this);
                        free(s1);
                        free(s2);
                        return x;
                    }
                    else if(s->values[i] == '!')
                    {
                        str s1 = s_substring(s, 0, i);
                        str s2 = s_substring(s, i+2, s->len-i-2);
                        __int128 x = ieval(s1, this) != ieval(s2, this);
                        free(s1);
                        free(s2);
                        return x;
                    }
                }
                error(s, i, "'=' isn't a valid operator");
            }
        }
    }
    ident = 0;
    for(int i = s->len-1; i >= 1; i--)
    {
        if(contains(s->values[i], L"(["))
        {
            if(ident > 0)
                ident--;
            else
                error(s, i, "Too many '(' or '['");
        }
        if(contains(s->values[i], L")]"))
            ident++;
        if(ident == 0)
        {
            if(s->values[i] == '+')
            {
                str s1 = s_substring(s, 0, i);
                str s2 = s_substring(s, i+1, s->len-i-1);
                __int128 x = ieval(s1, this) + ieval(s2, this);
                free(s1);
                free(s2);
                return x;
            }
            else if(s->values[i] == '-')
            {
                str s1 = s_substring(s, 0, i);
                str s2 = s_substring(s, i+1, s->len-i-1);
                __int128 x = ieval(s1, this) - ieval(s2, this);
                free(s1);
                free(s2);
                return x;
            }
        }
    }
    ident = 0;
    for(int i = s->len-1; i >= 1; i--)
    {
        if(contains(s->values[i], L"(["))
        {
            if(ident > 0)
                ident--;
            else
                error(s, i, "Too many '(' or '['");
        }
        if(contains(s->values[i], L")]"))
            ident++;
        if(ident == 0)
        {
            if(s->values[i] == '*')
            {
                str s1 = s_substring(s, 0, i);
                str s2 = s_substring(s, i+1, s->len-i-1);
                __int128 x = ieval(s1, this) * ieval(s2, this);
                free(s1);
                free(s2);
                return x;
            }
            else if(s->values[i] == '/')
            {
                str s1 = s_substring(s, 0, i);
                str s2 = s_substring(s, i+1, s->len-i-1);
                __int128 x = ieval(s1, this) / ieval(s2, this);
                free(s1);
                free(s2);
                return x;
            }
        }
    }
    #ifdef usepow
    ident = 0;
    for(int i = 1; i < s->len; i++)
    {
        if(contains(s->values[i], L"(["))
            ident++;
        if(contains(s->values[i], L")]"))
        {
            if(ident > 0)
                ident--;
            else
                error(s, i, "Too many ')'");
        }
        if(s->values[i] == '^' && ident == 0)
        {
            str s1 = s_substring(s, 0, i);
            str s2 = s_substring(s, i+1, s->len-i-1);
            __int128 x = pow(ieval(s1, this), ieval(s2, this));
            free(s1);
            free(s2);
            return x;
        }
    }
    #endif
    if(s->values[0] == L'$')
    {
        s->values++;
        s->len--;
        int i = ieval(s, this);
        return v_get(stacktrace, i, int);
    }
    if(s->values[0] == '+')
    {
        string t;
        t.cap = 0;
        t.len = s->len-1;
        t.values = s->values+1;
        return ieval(&t, this);
    }
    if(s->values[0] == '-')
    {
        string t;
        t.cap = 0;
        t.len = s->len-1;
        t.values = s->values+1;
        return -ieval(&t, this);
    }
    if(s->values[0] == '0')
    {
        if(s->len >= 3)
        {
            switch(s->values[1])
            {
                case 'b':
                case 'B':
                {
                    unsigned __int128 num = 0;
                    for(int i = 2; i < s->len; i++)
                    {
                        switch(s->values[i])
                        {
                            case '0':
                            case '1':
                            {
                                num = 2 * num + (s->values[i]-0x30);
                            }
                            break;

                            default:
                                goto next;
                        }
                    }
                    return num;
                }
                break;
                
                case 'o':
                case 'O':
                {
                    unsigned __int128 num = 0;
                    for(int i = 2; i < s->len; i++)
                    {
                        switch(s->values[i])
                        {
                            case '0':
                            case '1':
                            case '2':
                            case '3':
                            case '4':
                            case '5':
                            case '6':
                            case '7':
                            {
                                num = 8 * num + (s->values[i]-0x30);
                            }
                            break;

                            default:
                                goto next;
                        }
                    }
                    return num;
                }
                break;
                
                case 'd':
                case 'D':
                {
                    unsigned __int128 num = 0;
                    for(int i = 2; i < s->len; i++)
                    {
                        switch(s->values[i])
                        {
                            case 'a':
                            case 'b':
                            {
                                num = 12 * num + (s->values[i]-'a'+10);
                            }
                            break;

                            case 'A':
                            case 'B':
                            {
                                num = 12 * num + (s->values[i]-'A'+10);
                            }
                            break;

                            case '0':
                            case '1':
                            case '2':
                            case '3':
                            case '4':
                            case '5':
                            case '6':
                            case '7':
                            case '8':
                            case '9':
                            {
                                num = 12 * num + (s->values[i]-0x30);
                            }
                            break;

                            default:
                                goto next;
                        }
                    }
                    return num;
                }
                break;
                
                case 'x':
                case 'X':
                {
                    unsigned __int128 num = 0;
                    for(int i = 2; i < s->len; i++)
                    {
                        switch(s->values[i])
                        {
                            case 'a':
                            case 'b':
                            case 'c':
                            case 'd':
                            case 'e':
                            case 'f':
                            {
                                num = 16 * num + (s->values[i]-'a'+10);
                            }
                            break;

                            case 'A':
                            case 'B':
                            case 'C':
                            case 'D':
                            case 'E':
                            case 'F':
                            {
                                num = 16 * num + (s->values[i]-'A'+10);
                            }
                            break;

                            case '0':
                            case '1':
                            case '2':
                            case '3':
                            case '4':
                            case '5':
                            case '6':
                            case '7':
                            case '8':
                            case '9':
                            {
                                num = 16 * num + (s->values[i]-0x30);
                            }
                            break;

                            default:
                                goto next;
                        }
                    }
                    return num;
                }
                break;

                default:
                    goto dec;
            }
        }
        else
            goto dec;
    }
    if(s->values[0] == '1' ||
       s->values[0] == '2' ||
       s->values[0] == '3' ||
       s->values[0] == '4' ||
       s->values[0] == '5' ||
       s->values[0] == '6' ||
       s->values[0] == '7' ||
       s->values[0] == '8' ||
       s->values[0] == '9')
    {
        dec:;
        unsigned __int128 num = 0;
        for(int i = s->len-1, j = lenof(potenzen)-2; i >= 0; i--, j--)
        {
            if(j < 0)
                berror(s, i, "too large number");
            switch(s->values[i])
            {
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9':
                {
                    num += (s->values[i]-0x30) * potenzen[j];
                }
                break;

                default:
                    goto next;
            }
        }
        return num;
    }
    next:;
    if(s_cmp_str(s, "here") == 0)
        return ftell(input);
    if(s_cmp_str(s, "true") == 0)
        return 1;
    if(s_cmp_str(s, "false") == 0)
        return 0;
    union baum _ast;
    union baum* ast = &_ast;
    char res = s_cmp_str(s, "this.");
    if(res == 0 || res == 1)
    {
        s->values += 5;
        s->len -= 5;
        ast->tree = this;
    }
    else
        ast->tree = root;
    if(s->len == 0)
        error(s, 0, "Syntax error");
    while(s->len != 0)
    {
        string _t;
        int index = s_indexof_cstr(s, ".");
        if(index == -1)
            _t = *s;
        else
        {
            _t.values = s->values;
            _t.len = index;
        }
        str t = s_init_string(&_t);
        if(t->values[t->len-1] == ']')
        {
            string _t;
            _t.cap = 0;
            _t.len = t->len-1;
            _t.values = t->values;
            for(; _t.values[0] != '['; _t.values++, _t.len--) if(_t.values[0] == ']') error(&_t, 0, "Unexpected ']'");
            _t.values++;
            _t.len--;
            
            string sub;
            sub.cap = 0;
            sub.len = _t.values-t->values;
            sub.values = t->values;
            free(t);
            t = s_init_string(&sub);

            int index = ieval(&_t, this);
            free(sub.values);
            if(index < 0)
                error(s, 0, "Index is below 0");
            int i = 0;
            for(; index < potenzen[i]; i++);
            if(potenzen[i] == 0)
                s_append(t, '0');
            else
            {
                for(; i < lenof(potenzen)-1; i++)
                {
                    s_append(t, index/potenzen[i]+0x30);
                    index %= potenzen[i];
                }
            }
            s_append(t, ']');
        }

        for(int i = 0; i < ast->tree.entries->len; i++)
        {
            if(s_cmp_str(t, v_get(ast->tree.entries, i, union baum*)->tree.name) == 0)
            {
                if(v_get(ast->tree.entries, i, union baum*)->tree._type == 1)
                {
                    ast = v_get(ast->tree.entries, i, union baum*);
                    if(index == -1)
                        error(s, index, "Struct has no value");
                    s->values += index+1;
                    s->len -= index+1;
                    goto after;
                }
                else
                {
                    ast = v_get(ast->tree.entries, i, union baum*);
                    if(index != -1)
                        error(s, index, "Variable is not subscriptable");
                    s->values += s->len;
                    s->len = 0;
                    goto after;
                }
            }
        }
        error(s, 0, "Element not found");
        after:;
        s_free(t);
    }
    _ast.tree._type = 0;
    return ast->assigned._value;
}

//not used and not updated
double feval(str s, struct tree this, int byteindex)
{
    for(; contains(s->values[0], L" \t\n\r") && s->len > 0; s->values++, s->len--);
    for(; contains(s->values[s->len-1], L" \t\n\r") && s->len > 0; s->len--);
    while(s->values[0] == '(' && s->values[s->len-1] == ')')
    {
        s->values++;
        s->len -= 2;
    }
    int ident = 0;
    for(int i = s->len-1; i >= 1; i--)
    {
        if(contains(s->values[i], L"(["))
        {
            if(ident > 0)
                ident--;
            else
                error(s, i, "Too many ')' or ']'");
        }
        if(contains(s->values[i], L")]"))
            ident++;
        if(s->values[i] == '+')
        {
            str s1 = s_substring(s, 0, i);
            str s2 = s_substring(s, i+1, s->len-i-1);
            double x = feval(s1, this, byteindex) + feval(s2, this, byteindex);
            free(s1);
            free(s2);
            return x;
        }
        else if(s->values[i] == '-')
        {
            str s1 = s_substring(s, 0, i);
            str s2 = s_substring(s, i+1, s->len-i-1);
            double x = feval(s1, this, byteindex) - feval(s2, this, byteindex);
            free(s1);
            free(s2);
            return x;
        }
    }
    for(int i = s->len-1; i >= 1; i--)
    {
        if(contains(s->values[i], L"(["))
        {
            if(ident > 0)
                ident--;
            else
                error(s, i, "Too many ')' or ']'");
        }
        if(contains(s->values[i], L")]"))
            ident++;
        if(s->values[i] == '*')
        {
            str s1 = s_substring(s, 0, i);
            str s2 = s_substring(s, i+1, s->len-i-1);
            double x = feval(s1, this, byteindex) * feval(s2, this, byteindex);
            free(s1);
            free(s2);
            return x;
        }
        else if(s->values[i] == '/')
        {
            str s1 = s_substring(s, 0, i);
            str s2 = s_substring(s, i+1, s->len-i-1);
            double x = feval(s1, this, byteindex) / feval(s2, this, byteindex);
            free(s1);
            free(s2);
            return x;
        }
    }
    #ifdef usepow
    for(int i = 1; i < s->len; i++)
    {
        if(contains(s->values[i], L"(["))
            ident++;
        if(contains(s->values[i], L")]"))
        {
            if(ident > 0)
                ident--;
            else
                error(s, i, "Too many ')'");
        }
        if(s->values[i] == '^')
        {
            str s1 = s_substring(s, 0, i);
            str s2 = s_substring(s, i+1, s->len-i-1);
            double x = pow(feval(s1, this, byteindex), feval(s2, this, byteindex));
            free(s1);
            free(s2);
            return x;
        }
    }
    #endif
    if(s->values[0] == L'$')
    {
        s->values++;
        int i = ieval(s, this);
        return v_get(stacktrace, i, int);
    }
    if(s->values[s->len-1] == ']')
    {
        string t;
        t.cap = 0;
        t.len = s->len-1;
        t.values = t.values;
        for(; t.values[0] != '['; t.values++, t.len--) if(t.values[0] == ']') error(&t, 0, "Unexpected ']'");
        t.values++;
        t.len--;
        
        string sub;
        sub.cap = 0;
        sub.len = t.values-s->values;
        sub.values = s->values;
        s = s_init_string(&sub);

        int index = ieval(&t, this);
        if(index < 0)
            error(&t, 0, "Index is below 0");
        int i = 0;
        for(; index < potenzen[i]; i++)
        if(potenzen[i] == 0)
            s_append(s, '0');
        else
        {
            for(; i < lenof(potenzen)-1; i++)
            {
                s_append(s, index/potenzen[i]+0x30);
                index %= potenzen[i];
            }
        }
        s_append(s, ']');
    }
    union baum* ast;
    char res = s_cmp_str(s, "this.");
    if(res == 0 || res == 1)
    {
        s->values += 5;
        s->len -= 5;
        ast->tree = this;
    }
    else
        ast->tree = root;
    if(s->len == 0)
        error(s, 0, "Syntax error");
    while(s->len != 0)
    {
        string t;
        int index = s_indexof_cstr(s, ".");
        if(index == -1)
            t = *s;
        else
        {
            t.values = s->values;
            t.len = index;
        }
        t.cap = 0;
        for(int i = 0; i < this.entries->len; i++)
        {
            if(s_cmp_str(&t, v_get(this.entries, i, union baum*)->tree.name))
            {
                if(v_get(this.entries, i, union baum*)->tree._type == 1)
                {
                    ast = v_get(this.entries, i, union baum*);
                    if(index == -1)
                        error(s, index, "Struct has no value");
                    s->values += index+1;
                    s->len -= index+1;
                    goto after;
                }
                else
                {
                    if(index != -1)
                        error(s, index, "Variable is not subscriptable");
                    s->values += s->len;
                    s->len = 0;
                    goto after;
                }
            }
        }
        error(&t, 0, "Element not found");
        after:;
    }
    if(ast->tree._type == 1)
        error(s, 0, "Struct has no value");
    return ast->assigned._value;
}

char contains(int c, int* chars)
{
    for(int i = 0; chars[i]; i++)
        if(chars[i] == c)
            return 1;
    return 0;
}

unsigned __int128 readbytes(enum type type, str entryname)
{
    unsigned __int128 var = 0;
    int size;
    switch(type)
    {
        case i8:
        case u8:
        case x8:
        case c8:
        case s8:
        {
            size = 1;
        }
        break;
   
        case i16:
        case u16:
        case x16:
        case c16:
        case s16:
        {
            size = 2;
        }
        break;
   
        case i32:
        case u32:
        case x32:
        case c32:
        case s32:
        case f32:
        {
            size = 4;
        }
        break;
   
        case i64:
        case u64:
        case x64:
        case f64:
        {
            size = 8;
        }
        break;
   
        case i128:
        case u128:
        case x128:
        {
            size = 16;
        }
        break;
    }

    if(endian == 'l')
    {
        for(int i = 0; i < 8 * size; i += 8)
        {
            int byte = fgetc(input);
            if(byte == -1) berror(entryname, 0, "Unexpected end of binary file");
            var += (unsigned __int128)(unsigned char)byte << i;
        }
    }
    else
    {
        for(int i = 0; i < size; i++)
        {
            int byte = fgetc(input);
            if(byte == -1) berror(entryname, 0, "Unexpected end of binary file");
            var = (var << 8) + (unsigned __int128)(unsigned char)byte;
        }
    }
    switch (type)
    {
        case i8:
        case i16:
        case i32:
        case i64:
        {
            if(var >> (8*size-1))
            {
                unsigned __int128 x = 0xff << (8*size);
                for(int i = 0; i < 16-size; i++)
                {
                    var |= x;
                    x <<= 8;
                }
            }
        }
        break;
        default:
            break;
    }
    return var;
}

void addVar(struct tree* this, struct strct strct)
{
    this->_type = 1;
    this->entries = v_init(void*);

    for(int i = 0; i < strct.entries->len; i++)
    {
        struct prevar* entry = v_get(strct.entries, i, struct prevar*);
        int urindex;
        if(entry->plocation)
        {
            urindex = ftell(input);
            int err = fseek(input, ieval(entry->plocation, *this), SEEK_SET);
            if(err == ESPIPE)
                error(entry->plocation, 0, "You can't use the prefix \"location\" for an unseekable stream.\n");
            else if(err == EINVAL)
                error(entry->plocation, 0, "\"location\" argument is outside the file.\n");
            else if(err)
                error(entry->plocation, 0, "Unknown error from fseek.\n");
        }
        int realtypelen = entry->type->len;
        if(entry->type->values[entry->type->len-1] == ']')
        {
            int start1 = 0;
            for(; entry->type->values[start1] != '['; start1++) if(entry->type->values[start1] == ']') error(entry->type, entry->type->len-1, "Unexpected ']'");
            start1++;
            int end1 = start1;
            int ident = 1;
            for(; ident && end1 < entry->type->len; end1++)
            {
                if(entry->type->values[end1] == '[') ident++;
                if(entry->type->values[end1] == ']') ident--;
            }
            if(ident)
                error(entry->type, end1, "Unmatchig '[]'");
            
            int start;
            int end;
            if(end1 != entry->type->len)
            {
                int start2 = end1;
                for(; entry->type->values[start2] != '['; start2++) if(entry->type->values[start2] == ']') error(entry->type, entry->type->len-1, "Unexpected ']'");
                start2++;
                int end2 = entry->type->len;
                
                string s1;
                s1.values = &entry->type->values[start1];
                s1.cap = 0;
                s1.len = end1-start1-1;
                start = ieval(&s1, *this);
                
                string s2;
                s2.values = &entry->type->values[start2];
                s2.cap = 0;
                s2.len = end2-start2-1;
                end = ieval(&s2, *this);
            }
            else
            {
                start = 0;
                string s1;
                s1.values = &entry->type->values[start1];
                s1.cap = 0;
                s1.len = entry->type->len-start1-1;
                end = ieval(&s1, *this);
            }
            if(start == 0 && end == 1)
            {
                realtypelen = start1-1;
                goto noarray;
            }
            char* stammname = s_cstr(entry->name);
            for(int k = start; k < end; k++)
            {
                struct unassignedvar* v = malloc(sizeof(*v));
                int index = strlen(stammname);
                int i = 0;
                for(; k < potenzen[i]; i++);
                int len;
                if(i == lenof(potenzen)-1)
                    len = index+3+1;
                else
                    len = index+3+(lenof(potenzen)-i-1);
                v->name = malloc(len);
                strcpy(v->name, stammname);
                v->name[index++] = '[';
                if(i == lenof(potenzen)-1)
                {
                    v->name[index++] = '0';
                }
                else
                {
                    int l = k;
                    for(; i < lenof(potenzen)-1; i++, index++)
                    {
                        v->name[index] = l / potenzen[i] + 0x30;
                        l %= potenzen[i];
                    }
                }
                v->name[index++] = ']';
                v->name[index++] = 0;
                string typestring = *entry->type;
                typestring.len = start1-1;
                if(s_cmp_str(&typestring, "i8") == 0)
                {
                    v->type = i8;
                }
                else if(s_cmp_str(&typestring, "i16") == 0)
                {
                    v->type = i16;
                }
                else if(s_cmp_str(&typestring, "i32") == 0)
                {
                    v->type = i32;
                }
                else if(s_cmp_str(&typestring, "i64") == 0)
                {
                    v->type = i64;
                }
                else if(s_cmp_str(&typestring, "i128") == 0)
                {
                    v->type = i128;
                }
                else if(s_cmp_str(&typestring, "u8") == 0)
                {
                    v->type = u8;
                }
                else if(s_cmp_str(&typestring, "u16") == 0)
                {
                    v->type = u16;
                }
                else if(s_cmp_str(&typestring, "u32") == 0)
                {
                    v->type = u32;
                }
                else if(s_cmp_str(&typestring, "u64") == 0)
                {
                    v->type = u64;
                }
                else if(s_cmp_str(&typestring, "u128") == 0)
                {
                    v->type = u128;
                }
                else if(s_cmp_str(&typestring, "x8") == 0)
                {
                    v->type = x8;
                }
                else if(s_cmp_str(&typestring, "x16") == 0)
                {
                    v->type = x16;
                }
                else if(s_cmp_str(&typestring, "x32") == 0)
                {
                    v->type = x32;
                }
                else if(s_cmp_str(&typestring, "x64") == 0)
                {
                    v->type = x64;
                }
                else if(s_cmp_str(&typestring, "x128") == 0)
                {
                    v->type = x128;
                }
                else if(s_cmp_str(&typestring, "f32") == 0)
                {
                    v->type = f32;
                }
                else if(s_cmp_str(&typestring, "f64") == 0)
                {
                    v->type = f64;
                }
                else if(s_cmp_str(&typestring, "c8") == 0)
                {
                    v->type = c8;
                }
                else if(s_cmp_str(&typestring, "c16") == 0)
                {
                    v->type = c16;
                }
                else if(s_cmp_str(&typestring, "c32") == 0)
                {
                    v->type = c32;
                }
                else if(s_cmp_str(&typestring, "s8") == 0)
                {
                    v->type = s8;
                }
                else if(s_cmp_str(&typestring, "s16") == 0)
                {
                    v->type = s16;
                }
                else if(s_cmp_str(&typestring, "s32") == 0)
                {
                    v->type = s32;
                }
                else
                {
                    v_push(stacktrace, k);
                    struct tree* t = (struct tree*)v;
                    if(entry->pinline)
                        t->pinline = ieval(entry->pinline, *this);
                    else
                        t->pinline = 0;
                    for(int i = 0; i < structs->len; i++)
                    {
                        if(s_cmp(&typestring, v_get(structs, i, struct strct*)->name) == 0)
                        {
                            addVar(t, *v_get(structs, i, struct strct*));
                            goto after;
                        }
                    }
                    error(&typestring, 0, "Unknown type");
                    after:;
                    v_pop(stacktrace);
                    if(entry->value != NULL)
                        error(entry->value, 0, "Struct can't have a value");
                    t->_type = 1;
                    v_push(this->entries, t);
                    continue;
                }
                if(entry->pinline)
                    error(entry->pinline, 0, "Standart types can't have the prefix \"inline\".");
                if(entry->value != NULL)
                {
                    v->_type = 3;
                    v->_value = ieval(entry->value, *this);
                }
                else
                {
                    v->_type = 2;
                }
                unsigned __int128 x = readbytes(v->type, entry->name);
                if(v->_type == 2)
                    v->_value = x;
                else if(v->_value != x)
                    berror(entry->value, 0, "Document has a different byte before, not");
                v_push(this->entries, v);
            }
            free(stammname);
        }
        else
        {
            noarray:;
            struct unassignedvar* v = malloc(sizeof(*v));
            v->name = s_cstr(entry->name);
            string realtype = *entry->type;
            realtype.len = realtypelen;
            if(s_cmp_str(&realtype, "i8") == 0)
            {
                v->type = i8;
            }
            else if(s_cmp_str(&realtype, "i16") == 0)
            {
                v->type = i16;
            }
            else if(s_cmp_str(&realtype, "i32") == 0)
            {
                v->type = i32;
            }
            else if(s_cmp_str(&realtype, "i64") == 0)
            {
                v->type = i64;
            }
            else if(s_cmp_str(&realtype, "i128") == 0)
            {
                v->type = i128;
            }
            else if(s_cmp_str(&realtype, "u8") == 0)
            {
                v->type = u8;
            }
            else if(s_cmp_str(&realtype, "u16") == 0)
            {
                v->type = u16;
            }
            else if(s_cmp_str(&realtype, "u32") == 0)
            {
                v->type = u32;
            }
            else if(s_cmp_str(&realtype, "u64") == 0)
            {
                v->type = u64;
            }
            else if(s_cmp_str(&realtype, "u128") == 0)
            {
                v->type = u128;
            }
            else if(s_cmp_str(&realtype, "x8") == 0)
            {
                v->type = x8;
            }
            else if(s_cmp_str(&realtype, "x16") == 0)
            {
                v->type = x16;
            }
            else if(s_cmp_str(&realtype, "x32") == 0)
            {
                v->type = x32;
            }
            else if(s_cmp_str(&realtype, "x64") == 0)
            {
                v->type = x64;
            }
            else if(s_cmp_str(&realtype, "x128") == 0)
            {
                v->type = x128;
            }
            else if(s_cmp_str(&realtype, "f32") == 0)
            {
                v->type = f32;
            }
            else if(s_cmp_str(&realtype, "f64") == 0)
            {
                v->type = f64;
            }
            else if(s_cmp_str(&realtype, "c8") == 0)
            {
                v->type = c8;
            }
            else if(s_cmp_str(&realtype, "c16") == 0)
            {
                v->type = c16;
            }
            else if(s_cmp_str(&realtype, "c32") == 0)
            {
                v->type = c32;
            }
            else if(s_cmp_str(&realtype, "s8") == 0)
            {
                v->type = s8;
            }
            else if(s_cmp_str(&realtype, "s16") == 0)
            {
                v->type = s16;
            }
            else if(s_cmp_str(&realtype, "s32") == 0)
            {
                v->type = s32;
            }
            else
            {

                struct tree* t = (struct tree*)v;
                if(entry->pinline)
                    t->pinline = ieval(entry->pinline, *this);
                else
                    t->pinline = 0;
                for(int i = 0; i < structs->len; i++)
                {
                    if(s_cmp(&realtype, v_get(structs, i, struct strct*)->name) == 0)
                    {
                        addVar(t, *v_get(structs, i, struct strct*));
                        goto after2;
                    }
                }
                error(&realtype, 0, "Unknown type");
                after2:;
                if(entry->value != NULL)
                    error(entry->value, 0, "Struct can't have a value");
                t->_type = 1;
                v_push(this->entries, t);
                continue;
            }
            if(entry->pinline)
                error(entry->pinline, 0, "Standart types can't have the prefix \"inline\".");
            if(entry->value != NULL)
            {
                v->_type = 3;
                v->_value = ieval(entry->value, *this);
            }
            else
            {
                v->_type = 2;
            }
            unsigned __int128 x = readbytes(v->type, entry->name);
            if(v->_type == 2)
                v->_value = x;
            else if(v->_value != x)
                berror(entry->value, 0, "Document has a different byte before, not");
            v_push(this->entries, v);
        }
        if(entry->plocation)
        {
            int err = fseek(input, urindex, SEEK_SET);
            if(err == ESPIPE)
                error(entry->plocation, 0, "You can't use the prefix \"location\" for an unseekable stream.\n");
            else if(err == EINVAL)
                error(entry->plocation, 0, "\"location\" argument is outside the file.\n");
            else if(err)
                error(entry->plocation, 0, "Unknown error from fseek.\n");
        }
    }
}

void printnum(unsigned __int128 x)
{
    if(x == 0)
    {
        printf("0\n");
        return;
    }
    int i = 0;
    for(; x < potenzen[i]; i++);
    for(; i < lenof(potenzen)-1; i++)
    {
        printf("%i", (int)(x / potenzen[i]));
        x %= potenzen[i];
    }
    printf("\n");
}

void printhexnum(unsigned __int128 x)
{
    if(x == 0)
    {
        printf("0\n");
        return;
    }
    unsigned __int128 i = (__int128)1 << 124;
    for(; x < i; i >>= 4);
    for(; i; i >>= 4)
    {
        printf("%X", (int)(x / i));
        x %= i;
    }
    printf("\n");
}

void printchar(u_int32_t c)
{
    switch (c)
    {
        case 7:
            printf("\\a");
        break;
        
        case 8:
            printf("\\b");
        break;
        
        case 9:
            printf("\\t");
        break;
        
        case 10:
            printf("\\n");
        break;
        
        case 11:
            printf("\\v");
        break;
        
        case 12:
            printf("\\f");
        break;
        
        case 13:
            printf("\\r");
        break;
        
        case 27:
            printf("\\e");
        break;
        
        case '\\':
            printf("\\\\");
        break;
        
        case '"':
            printf("\\\"");
        break;
        
        case '\'':
            printf("\\'");
        break;
        
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 14:
        case 15:
            printf("\\%x", c);
        break;

        case 16:
        case 17:
        case 18:
        case 19:
        case 20:
        case 21:
        case 22:
        case 23:
        case 24:
        case 25:
        case 26:
        case 28:
        case 29:
        case 30:
        case 31:
        case 127:
        case 254:
        case 255:
            printf("\\x%x", c);
        break;

        case 65535:
            printf("\\u%x", c);
        break;

        case -1:
            printf("\\U%x", c);
        break;

        default:
            printf("%c", c);
        break;
    }
}

char doesprinttree(struct tree* this)
{
    for(int i = 0; i < this->entries->len; i++)
    {
        union baum* b = v_get(this->entries, i, union baum*);
        if(b->tree.name[0] == '_')
            continue;
        if(b->tree._type == 1)
            if(doesprinttree(&b->tree))
                return 1;
        return 1;
    }
    return 0;
}

void printtree(struct tree* this, int ident)
{
    for(int i = 0; i < this->entries->len; i++)
    {
        union baum* b = v_get(this->entries, i, union baum*);
        if(b->tree.name[0] != '_')
        {
            if(b->tree._type == 1)
            {
                if(b->tree.pinline)
                {
                    printtree(&b->tree, ident);
                }
                else
                {
                    if(!doesprinttree(&b->tree))
                        continue;
                    for(int i = 0; i < ident; i++) printf("\t");
                    printf("%s\n", b->tree.name);
                    printtree(&b->tree, ident+1);
                    printf("\n");
                }
            }
            else
            {
                for(int i = 0; i < ident; i++) printf("\t");
                switch(b->assigned.type)
                {
                    case i8:
                    {
                        printf("i8 %s=", b->assigned.name);
                        if(b->assigned._value > 0x80)
                        {
                            printf("-");
                            b->assigned._value = -b->assigned._value;
                        }
                        printnum(b->assigned._value);
                    }
                    break;

                    case c8:
                    {
                        printf("c8 %s=0x%X '", b->assigned.name, (int)b->assigned._value);
                        printchar(b->assigned._value);
                        printf("'\n");
                    }
                    break;

                    case s8:
                    {
                        int len = strlen(b->assigned.name);
                        if(b->assigned.name[len-1] != ']')
                        {
                            printf("s8 %s=\"", b->assigned.name);
                            printchar(b->assigned._value);
                            printf("\"\n");
                        }
                        int starti = i;
                        int endi = starti;
                        int startnum = 0;
                        int j;
                        for(j = len-2; b->assigned.name[j] != '['; j--) startnum = startnum * 10 + b->assigned.name[j] - 0x30;
                        j++;
                        int endnum = startnum;
                        while(1)
                        {
                            endi++,endnum++;
                            if(endi >= this->entries->len)
                                break;
                            union baum* tb = v_get(this->entries, endi, union baum*);
                            if(strncmp(b->assigned.name, tb->assigned.name, j))
                                break;
                            int realnum = 0;
                            for(int k = j; tb->assigned.name[k] != ']'; k++)
                                realnum = realnum * 10 + tb->assigned.name[k] - 0x30;
                            if(realnum != endnum)
                                break;
                        }
                        int k = endi-1;
                        for(; k >= starti; k--)
                            if(v_get(this->entries, k, union baum*)->assigned._value)
                                break;
                        printf("s8 %.*s%i-%i]=\"", j, b->assigned.name, startnum, endnum-1);
                        for(int i = starti; i <= k; i++)
                            printchar(v_get(this->entries, i, union baum*)->assigned._value);
                        printf("\"\n");
                        i = endi-1;
                    }
                    break;

                    case u8:
                    {
                        printf("u8 %s=", b->assigned.name);
                        printnum(b->assigned._value);
                    }
                    break;

                    case x8:
                    {
                        printf("x8 %s=", b->assigned.name);
                        printhexnum(b->assigned._value);
                    }
                    break;

                    case i16:
                    {
                        printf("i16 %s=", b->assigned.name);
                        if(b->assigned._value > 0x8000)
                        {
                            printf("-");
                            b->assigned._value = -b->assigned._value;
                        }
                        printnum(b->assigned._value);
                    }
                    break;

                    /*case f16:
                    {
                        printf("f16 %s=%f\n", b->assigned.name, (float)*(_Float16*)&b->assigned._value);
                    }
                    break;*/ //not supported

                    case c16:
                    {
                        printf("c16 %s=0x%X '", b->assigned.name, (int)b->assigned._value);
                        printchar(b->assigned._value);
                        printf("'\n");
                    }
                    break;

                    case s16:
                    {
                        int len = strlen(b->assigned.name);
                        if(b->assigned.name[len-1] != ']')
                        {
                            printf("s16 %s=\"", b->assigned.name);
                            printchar(b->assigned._value);
                            printf("\"\n");
                        }
                        int starti = i;
                        int endi = starti;
                        int startnum = 0;
                        int j;
                        for(j = len-2; b->assigned.name[j] != '['; j--) startnum = startnum * 10 + b->assigned.name[j] - 0x30;
                        j++;
                        int endnum = startnum;
                        while(1)
                        {
                            endi++,endnum++;
                            if(endi >= this->entries->len)
                                break;
                            union baum* tb = v_get(this->entries, endi, union baum*);
                            if(strncmp(b->assigned.name, tb->assigned.name, j))
                                break;
                            int realnum = 0;
                            for(int k = j; tb->assigned.name[k] != ']'; k++)
                                realnum = realnum * 10 + tb->assigned.name[k] - 0x30;
                            if(realnum != endnum)
                                break;
                        }
                        int k = endi-1;
                        for(; k >= starti; k--)
                            if(v_get(this->entries, k, union baum*)->assigned._value)
                                break;
                        printf("s16 %.*s%i-%i]=\"", j, b->assigned.name, startnum, endnum-1);
                        for(int i = starti; i <= k; i++)
                            printchar(v_get(this->entries, i, union baum*)->assigned._value);
                        printf("\"\n");
                        i = endi-1;
                    }
                    break;

                    case u16:
                    {
                        printf("u16 %s=", b->assigned.name);
                        printnum(b->assigned._value);
                    }
                    break;

                    case x16:
                    {
                        printf("x16 %s=", b->assigned.name);
                        printhexnum(b->assigned._value);
                    }
                    break;

                    case i32:
                    {
                        printf("i32 %s=", b->assigned.name);
                        if(b->assigned._value > 0x80000000)
                        {
                            printf("-");
                            b->assigned._value = -b->assigned._value;
                        }
                        printnum(b->assigned._value);
                    }
                    break;

                    case f32:
                    {
                        printf("f32 %s=%f\n", b->assigned.name, *(float*)&b->assigned._value);
                    }
                    break;

                    case c32:
                    {
                        printf("c32 %s=0x%X '", b->assigned.name, (unsigned int)b->assigned._value);
                        printchar(b->assigned._value);
                        printf("'\n");
                    }
                    break;

                    case s32:
                    {
                        int len = strlen(b->assigned.name);
                        if(b->assigned.name[len-1] != ']')
                        {
                            printf("s32 %s=\"", b->assigned.name);
                            printchar(b->assigned._value);
                            printf("\"\n");
                        }
                        int starti = i;
                        int endi = starti;
                        int startnum = 0;
                        int j;
                        for(j = len-2; b->assigned.name[j] != '['; j--) startnum = startnum * 10 + b->assigned.name[j] - 0x30;
                        j++;
                        int endnum = startnum;
                        while(1)
                        {
                            endi++,endnum++;
                            if(endi >= this->entries->len)
                                break;
                            union baum* tb = v_get(this->entries, endi, union baum*);
                            if(strncmp(b->assigned.name, tb->assigned.name, j))
                                break;
                            int realnum = 0;
                            for(int k = j; tb->assigned.name[k] != ']'; k++)
                                realnum = realnum * 10 + tb->assigned.name[k] - 0x30;
                            if(realnum != endnum)
                                break;
                        }
                        int k = endi-1;
                        for(; k >= starti; k--)
                            if(v_get(this->entries, k, union baum*)->assigned._value)
                                break;
                        printf("s32 %.*s%i-%i]=\"", j, b->assigned.name, startnum, endnum-1);
                        for(int i = starti; i <= k; i++)
                            printchar(v_get(this->entries, i, union baum*)->assigned._value);
                        printf("\"\n");
                        i = endi-1;
                    }
                    break;

                    case u32:
                    {
                        printf("u32 %s=", b->assigned.name);
                        printnum(b->assigned._value);
                    }
                    break;

                    case x32:
                    {
                        printf("x32 %s=", b->assigned.name);
                        printhexnum(b->assigned._value);
                    }
                    break;

                    case i64:
                    {
                        printf("i64 %s=", b->assigned.name);
                        if(b->assigned._value > 0x8000000000000000)
                        {
                            printf("-");
                            b->assigned._value = -b->assigned._value;
                        }
                        printnum(b->assigned._value);
                    }
                    break;

                    case f64:
                    {
                        printf("f64 %s=%lf\n", b->assigned.name, *(double*)&b->assigned._value);
                    }
                    break;

                    case u64:
                    {
                        printf("u64 %s=", b->assigned.name);
                        printnum(b->assigned._value);
                    }
                    break;

                    case x64:
                    {
                        printf("x64 %s=", b->assigned.name);
                        printhexnum(b->assigned._value);
                    }
                    break;

                    case i128:
                    {
                        printf("i128 %s=", b->assigned.name);
                        if(b->assigned._value > 0x8000000000000000 << 8)
                        {
                            printf("-");
                            b->assigned._value = -b->assigned._value;
                        }
                        printnum(b->assigned._value);
                    }
                    break;

                    case u128:
                    {
                        printf("u128 %s=", b->assigned.name);
                        printnum(b->assigned._value);
                    }
                    break;

                    case x128:
                    {
                        printf("x128 %s=", b->assigned.name);
                        printhexnum(b->assigned._value);
                    }
                    break;
                }
            }
        }
    }
}

void freetree(struct tree* t)
{
    free(t->name);
    for(int i = 0; i < t->entries->len; i++)
    {
        union baum* b = ((union baum**)t->entries->values)[i];
        if(b->tree._type == 1)
        {
            freetree(&b->tree);
            free(&b->tree);
            continue;
        }
        free(b->assigned.name);
        free(&b->assigned);
    }
    free(t->entries->values);
    free(t->entries);
}