#define usemath
//#define caching

#include <asm-generic/errno-base.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef usemath
#include <math.h>
#endif
#include "string.h"
#include "vector.h"

#define lenof(x) sizeof(x)/sizeof(*x)
#define notvar L" \t\n\r-+/*\"$()[]{}&|^.?#=<>;"
//if tablen == -1 then use tabs (instead of spaces)
#define tablen 4

enum type
{
    none,

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

enum evaltypetype
{
    evalnone=0,
    evalint=1,
    evalfloat=2,
    evalstr=3
};

union evaltype
{
    struct
    {
        enum evaltypetype type;//1
        __int128 x;
    } i;

    struct
    {
        enum evaltypetype type;//2
        double x;
    } f;

    struct
    {
        enum evaltypetype type;//3
        string x;
    } s;
};

struct dict
{
    long len;
    union evaltype* keys;
    union evaltype* values;
};

struct prevar
{
    str plocation;//prefix
    str pinline;//prefix
    str psimplify;//prefix
    str pendian;//prefix
    struct dict* pvalues;//prefix
    str pvalueonly;//prefix
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
    struct dict* pvalues;//prefix
    char pvalueonly;//prefix
};

struct assignedvar
{
    char _type;//3
    char* name;
    enum type type;
    unsigned __int128 _value;
    struct dict* pvalues;//prefix
    char pvalueonly;//prefix
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

void parseCommand(int argc, char** args);
int readchar();
int seek(long pos);
void error(str substr, int offset, char* msg);
void berror(str substr, int offset, char* msg);
char setPrefix(char* defname, str* prefix, char* errmsg, string* s);
char setDictPrefix(char* defname, struct dict* prefix, char* errmsg, string* s);
void setPrefixes(struct prevar* var, int** index, str x, char type);
void invalidtypeerror(str substr, char* operator, union evaltype left, union evaltype right);
union evaltype eval(str s, struct tree this);
char contains(int c, int* chars);
unsigned __int128 readbytes(enum type type, str entryname);
void setValue(char type, struct prevar entry, struct unassignedvar* v, union evaltype value, char* svalue, int slen, int start, int k);
enum type parsetype(str stype);
void addVar(struct tree* this, struct strct strct);
void printnum(unsigned __int128 x);
void printhexnum(unsigned __int128 x);
void printchar(u_int32_t c);
char doesprinttree(struct tree* this);
void printValue(struct assignedvar var, int index);
int getValueIndex(struct assignedvar var);
void printtree(struct tree* this, int ident);
void freetree(struct tree* t);

FILE* input;
str structstr;
char endian = 'l';
char ignoreend = 'n';
char standartvalueonly = 'n';
long maxarraysize = 0;
struct tree root;
vec structs;//struct strct*
vec stacktrace;//int
#ifdef caching
unsigned char cache[2<<20];
long cachebase = 0;//in the file
int cachelen = 0;//maximal sizeof(cache)
#endif
long filepos = 0;

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
    char instr = 0;
    char comment = -2;//-2=none, -1=singleline, 0>=multiline und comment=Anzahl der '\n's
    int lasti = 0;
    for(int i = 0; i < structstr->len; i++)
    {
        switch(comment)
        {
            case -2:
            {
                if(structstr->values[i] == '"')
                {
                    if(structstr->values[i-1] == '\\')
                        if(structstr->values[i-2] != '\\')
                            continue;
                    instr = !instr;
                }
                if(structstr->values[i] == '/' && !instr)
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
    if(instr)
        error(structstr, structstr->len-1, "Unterminated string");
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
        if(structstr->values[i] == '"')
        {
            if(structstr->values[i-1] == '\\')
                if(structstr->values[i-2] != '\\')
                    continue;
            instr = !instr;
        }
        if(instr)
            continue;
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
    if(instr)
        error(structstr, structstr->len-1, "Unterminated string");
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
            if(x->len >= 3)
            {
                switch (x->values[1])
                {
                    case 'e':
                    {
                        if(x->len != 3)
                            error(x, 0, "The syntax for a parameter is:#pa;");
                        if(x->values[2] == 'h' || x->values[2] == 'l')
                            endian = x->values[2];
                        else
                            error(x, 2, "Invalid argument");
                    }
                    break;
                    
                    case 'i':
                    {
                        if(x->len != 3)
                            error(x, 0, "The syntax for a parameter is:#pa;");
                        if(x->values[2] == 'y' || x->values[2] == 'n')
                            ignoreend = x->values[2];
                        else
                            error(x, 2, "Invalid argument");
                    }
                    break;
                    
                    case 'v':
                    {
                        if(x->len != 3)
                            error(x, 0, "The syntax for a parameter is:#pa;");
                        if(x->values[2] == 'y' || x->values[2] == 'n')
                            standartvalueonly = x->values[2];
                        else
                            error(x, 2, "Invalid argument");
                    }
                    break;
                    
                    case 'm':
                    {
                        maxarraysize = 0;
                        if(x->len > 11)
                        {
                            error(x, 2, "too high value");
                            exit(1);
                        }
                        for(int j = 2; j < x->len; j++)
                        {
                            if(args[i][j] >= 0x30 && args[i][j] <= 0x39)
                                maxarraysize = maxarraysize * 10 + args[i][j]-0x30;
                            else
                            {
                                error(x, 0, "The syntax for a parameter is:#pa; (a must be an integer)");
                                exit(1);
                            }
                        }
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
                struct prevar* var = malloc(sizeof(*var));
                setPrefixes(var, &index, x, 0);

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
                for(; *index != ';' && !instr; index++)
                {
                    if(index-x->values >= x->len) error(x, index-x->values, "Unterminated '{'");
                    if(*index == '\n') error(x, index-x->values, "Unallowed newline at value argument");
                    if(structstr->values[i] == '"')
                    {
                        if(structstr->values[i-1] == '\\')
                            if(structstr->values[i-2] != '\\')
                                continue;
                        instr = !instr;
                        continue;
                    }
                    if(*index == '}') error(x, index-x->values, "Expected ';'");
                }

                value->len = index-value->values;
                index++;
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
            struct prevar* var = malloc(sizeof(*var));
            setPrefixes(var, &index, x, 1);

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
            for(; *index != ';'&& !instr; index++)
            {
                if(*index == '\n') error(x, index-x->values, "Unallowed newline at value argument");
                if(structstr->values[i] == '"')
                {
                    if(structstr->values[i-1] == '\\')
                        if(structstr->values[i-2] != '\\')
                            continue;
                    instr = !instr;
                }
            }

            value->len = index-value->values;
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

#ifdef caching
    cachelen = fread(cache, sizeof(cache), 1, input) * sizeof(cache);
#endif

    struct strct t;
    t.entries = prevars;
    stacktrace = v_init(int);
    addVar(&root, t);

    int readlen = filepos;
    if(ignoreend == 'n')
        if(readchar() != -1)
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

//belongs to main
void parseCommand(int argc, char** args)
{
    for(int i = 1; i < argc; i++)
    {
        if(!strcmp(args[i], "-h"))
        {
            printf("hexedit\n\n"
                   "Arguments:\n"
                   "    -e[h/l]     sets the endian to high/low default:low\n"
                   "    -i[y/n]     ignore bytes after end? default:no\n"
                   "    -v[y/n]     set default for \"valueonly\" default:no\n"
                   "    -s          the path to the struct file\n"
                   "    -m[number]  the maximum size of an array must be below 1'000'000'000 default:0(no limit)\n"
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
                   "The array types can be assigned a number for all variables or a string for s... types.\n\n"
                   "Expressions can have the following operands:+(unary&binary),-(unary&binary),*,/,^(power, evaluation right to left, only when \"usemath\" is defined in the sourcecode),<,<=,==,>,>=,&&,||,^^(xor),$(unary)\n"
                   "There isn't any error detection for expressions. So be sure that the absolute value of any operation is in the bounds of i128 or f64 (the internal number formats).\n"
                   "Therefore u128, x128, c128 and s128 are treated as signed in expressions."
                   "int*str=str repeated\n"
                   "You need to convert types in an expression explicit with: int(...), float(...), char(...)\n"
                   "But you can't convert char back to int."
                   "Another expression-function is exp(...) for int/float"
                   "You can also use initialized local and global variables.\n"
                   "To do that, just type the global path into the expression or use \"this.\" to use a local variable.\n"
                   "To get the value of an array element, just type: name[expression]\n"
                   "To get the value of an s... array, just type: name\n\n"
                   "The Stacktrace is a list of the current array indecies.\n"
                   "The stacktrace operator \"$n\" returns the n-th index in the stacktrace\n"
                   "$_ returns the length of the stacktrace.\n\n"
                   "Variables are initialized from top to bottom.\n"
                   "You can access the current file address in an expression with \"here\". (before the current variable)"
                   "To hide a variable the name must start with an underscore. But you can still use it in an expression.\n"
                   "Variables can have prefixes before the type name. Variable definitions then have the following syntax:\n"
                   "    (prefix1=value, prefix2=value ,...) type name;\n"
                   "The following prefixes are available:\n"
                   "    inline      The values of a variable of type struct replace the variable. (But the paths aren't inline.)\n"
                   "    location    Set the absolute location of the variable. Doesn't affect the byte pointer for the next variables.\n"
                   "    simplify    If true (the default) discard the array ending if start=0 and end=1\n"
                   "    endian      Set the endian for the following variables. (including the current variable)\n"
                   "    value[...]  Set a string to be displayed when the value is \"...\".\n"
                   "    valueonly   Output only the string defined by \"value\", when a string is defined.\n"
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
        else if(!strcmp(args[i], "-vy"))
        {
            standartvalueonly = 'y';
        }
        else if(!strcmp(args[i], "-vn"))
        {
            standartvalueonly = 'n';
        }
        else if(!strcmp(args[i], "-m"))
        {
            maxarraysize = 0;
            int len = strlen(args[i]);
            if(len > 11)
            {
                fprintf(stderr, "hexstruct: too high value for \"-m\"\n");
                exit(1);
            }
            for(int j = 2; j < len; j++)
            {
                if(args[i][j] >= 0x30 && args[i][j] <= 0x39)
                    maxarraysize = maxarraysize * 10 + args[i][j]-0x30;
                else
                {
                    fprintf(stderr, "hexstruct: invalid value for \"-m\"\n");
                    exit(1);
                }
            }
        }
        else
        {
            FILE* stream = fopen(args[i], "rb");
            if(!stream)
            {
                fprintf(stderr, "hexstruct: invalid binary path\n");
                exit(1);
            }
            input = stream;
        }
    }
}

int readchar()
{
    #ifdef caching
    if(filepos < cachebase || filepos >= cachebase + sizeof(cache))
    {
        cachebase = filepos / sizeof(cache) * sizeof(cache);
        if(ftell(input) != cachebase)
        {
            if(fseek(input, cachebase, SEEK_SET) == ESPIPE)
                berror(NULL, 0, "Error in readchar. Disable caching for more information.");
        }
        cachelen = fread(cache, sizeof(cache), 1, input) * sizeof(cache);
    }
    if(filepos >= cachebase + cachelen)
        return -1;
    return cache[filepos++ - cachebase];
    #else
    filepos++;
    return fgetc(input);
    #endif
}

int seek(long pos)
{
    #ifdef caching
    filepos = pos;
    return 0;
    #else
    filepos = pos;
    return fseek(input, pos, SEEK_SET);
    #endif
}

void error(str substr, int offset, char* msg)
{
    if(substr == NULL)
    {
        fprintf(stderr, "hexstruct: %s\n", msg);
        exit(1);
    }
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
    if(!substr)
    {
        int byteindex = filepos;
        int inputlen;
        if(fseek(input, 0, SEEK_END) == ESPIPE)
            inputlen = -1;
        else
        {
            inputlen = filepos;
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
            fprintf(stderr, "\n");
        }
        else
        {
            if(inputlen == -1)
                fprintf(stderr, "hexstruct: %s at Byte %i\n", msg, byteindex);
            else
                fprintf(stderr, "hexstruct: %s at Byte %i/-%i\n", msg, byteindex, inputlen-byteindex);
        }
        exit(1);
    }

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

    int byteindex = filepos;
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

//belongs to setPrefixes
char setPrefix(char* defname, str* prefix, char* errmsg, string* s)
{
    if(s_cmp_str(s, defname) == 1)
    {
        int defsize = strlen(defname);
        if(*prefix)
            error(s, 0, errmsg);
        s->values += defsize;
        s->len -= defsize;
        str t = malloc(sizeof(*t));
        t->cap = 0;
        t->values = s->values;
        t->len = 0;
        int ident = 1;
        char instr = 0;
        while(1)
        {
            if(t->values[t->len] == '"')
                instr = !instr;
            else if(instr)
            {
                t->len++;
                continue;
            }
            if(t->values[t->len] == '(')
                ident++;
            if(t->values[t->len] == ')')
            {
                if(ident > 1)
                    ident--;
                else
                    break;
            }
            if(t->values[t->len] == ',' && ident == 1)
                break;
            t->len++;
        }
        *prefix = t;
        s->values += t->len+1;
        s->len -= t->len+1;
        return 1;
    }
    else
        return 0;
}

//belongs to setPrefixes
char setDictPrefix(char* defname, struct dict* prefix, char* errmsg, string* s)
{
    if(s_cmp_str(s, defname) == 1)
    {
        prefix->len++;
        prefix->values = realloc(prefix->values, prefix->len*sizeof(union evaltype));
        if(!prefix->values)
            error(s, 0, "Out of memory");
        prefix->keys = realloc(prefix->keys, prefix->len*sizeof(union evaltype));
        if(!prefix->keys)
            error(s, 0, "Out of memory");

        int defsize = strlen(defname);
        s->values += defsize+1;
        s->len -= defsize+1;
        if(s->values[-1] != '[')
            error(s, -1, "Expected '[");
        string t;
        t.values = s->values;
        t.cap = 0;
        t.len = 0;
        int ident = 1;
        char instr = 0;
        while(1)
        {
            if(t.values[t.len] == '"')
                instr = !instr;
            else if(instr)
            {
                t.len++;
                continue;
            }
            if(t.values[t.len] == '[')
                ident++;
            if(t.values[t.len] == ']')
            {
                if(ident > 1)
                    ident--;
                else
                    break;
            }
            t.len++;
        }
        for(int i = 0; i < prefix->len-1; i++)
            if(s_cmp(&prefix->keys[i].s.x, &t) == 0)
                error(s, 0, errmsg);
        prefix->keys[prefix->len-1].s.x = t;
        s->values += t.len+2;
        s->len -= t.len+2;

        if(s->values[-1] != '=')
            error(s, -1, "Expected '='");
        
        t.cap = 0;
        t.values = s->values;
        t.len = 0;
        while(1)
        {
            if(t.values[t.len] == '"')
                instr = !instr;
            else if(instr)
            {
                t.len++;
                continue;
            }
            if(t.values[t.len] == '(')
                ident++;
            if(t.values[t.len] == ')')
            {
                if(ident > 1)
                    ident--;
                else
                    break;
            }
            if(t.values[t.len] == ',' && ident == 1)
                break;
            t.len++;
        }
        prefix->values[prefix->len-1].s.x = t;
        s->values += t.len+1;
        s->len -= t.len+1;
        return 1;
    }
    else
        return 0;
}

//belongs to main
void setPrefixes(struct prevar* var, int** index, str x, char type)
{
    var->pinline = NULL;
    var->plocation = NULL;
    var->psimplify = NULL;
    var->pendian = NULL;
    var->pvalues = malloc(sizeof(*var->pvalues));
    var->pvalues->len = 0;
    var->pvalues->keys = NULL;
    var->pvalues->values = NULL;
    var->pvalueonly = NULL;
    if(**index == '(')
    {
        (*index)++;
        string s;
        s.cap = 0;
        s.values = *index;
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
            if(!setPrefix("location=", &var->plocation, "Double definition of the \"location\" prefix", &s))
            if(!setPrefix("inline=", &var->pinline, "Double definition of the \"inline\" prefix", &s))
            if(!setPrefix("simplify=", &var->psimplify, "Double definition of the \"simplify\" prefix", &s))
            if(!setPrefix("endian=", &var->pendian, "Double definition of the \"endian\" prefix", &s))
            if(!setPrefix("valueonly=", &var->pvalueonly, "Double definition of the \"valueonly\" prefix", &s))
            if(!setDictPrefix("value", var->pvalues, "Double definition a key of the \"value\" prefix", &s))
                error(&s, 0, "Invalid prefix");
        }
        *index = s.values + s.len + 1;
        for(; contains(**index, L" \t\n\r"); (*index)++)
        {
            if(*index-x->values >= x->len)
            {
                if(type == 0)
                    error(x, *index-x->values, "Unterminated '{'");
                else
                    error(x, *index-x->values, "Unterminated ')'");
            }
        }
    }
}

//len(operator)<=4 for binary
//len(operator)<=9 for unary
void invalidtypeerror(str substr, char* operator, union evaltype left, union evaltype right)
{
    const char* lstr;
    switch (left.i.type)
    {
        case evalint:
        {
            lstr = "int";
        }
        break;

        case evalfloat:
        {
            lstr = "float";
        }
        break;

        case evalstr:
        {
            lstr = "str";
        }
        break;
    }

    const char* rstr;
    switch (right.i.type)
    {
        case evalnone:
        {
            rstr = NULL;
        }
        break;

        case evalint:
        {
            rstr = "int";
        }
        break;

        case evalfloat:
        {
            rstr = "float";
        }
        break;

        case evalstr:
        {
            rstr = "str";
        }
        break;
    }

    char msg[30];
    if(!rstr)
        sprintf(msg, "Invalid type: %s%s", operator, lstr);
    else
        sprintf(msg, "Invalid types: %s%s%s", lstr, operator, rstr);
    error(substr, 0, msg);
}

//belongs to addVar
union evaltype eval(str s, struct tree this)
{
    for(; contains(s->values[0], L" \t\n\r") && s->len > 0; s->values++, s->len--);
    for(; contains(s->values[s->len-1], L" \t\n\r") && s->len > 0; s->len--);
    while(s->values[s->len-1] == ')')
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
        if(s->values[0] == '(')
        {
            s->values++;
            s->len -= 2;
            continue;
        }
        #ifdef usemath
        else if(s_cmp_str(s, "exp(") == 1)
        {
            s->values += 4;
            s->len -= 5;
            double out;
            union evaltype val = eval(s, this);
            switch (val.i.type)
            {
                case evalint:
                {
                    out = val.i.x;
                }
                break;

                case evalfloat:
                {
                    out = val.f.x;
                }
                break;

                case evalstr:
                {
                    invalidtypeerror(s, "exp ", val, (union evaltype)(typeof(val.i)){0, 0});
                }
                break;
            }
            out = exp(out);
            val.f.type = evalfloat;
            val.f.x = out;
            return val;
        }
        #endif
        else if(s_cmp_str(s, "int(") == 1)
        {
            s->values += 4;
            s->len -= 5;
            __int128 out;
            union evaltype val = eval(s, this);
            switch (val.i.type)
            {
                case evalint:
                {
                    out = val.i.x;
                }
                break;

                case evalfloat:
                {
                    #ifdef usemath
                    out = floor(val.f.x);
                    #else
                    out = val.f.x;
                    #endif
                }
                break;

                case evalstr:
                {
                    invalidtypeerror(s, "int ", val, (union evaltype)(typeof(val.i)){0, 0});
                }
                break;
            }
            val.i.type = evalint;
            val.i.x = out;
            return val;
        }
        else if(s_cmp_str(s, "float(") == 1)
        {
            s->values += 6;
            s->len -= 7;
            double out;
            union evaltype val = eval(s, this);
            switch (val.i.type)
            {
                case evalint:
                {
                    out = val.i.x;
                }
                break;

                case evalfloat:
                {
                    out = val.f.x;
                }
                break;

                case evalstr:
                {
                    invalidtypeerror(s, "float ", val, (union evaltype)(typeof(val.i)){0, 0});
                }
                break;
            }
            val.f.type = evalfloat;
            val.f.x = out;
        }
        else if(s_cmp_str(s, "char(") == 1)
        {
            s->values += 5;
            s->len -= 6;
            __int128 out;
            union evaltype val = eval(s, this);
            switch (val.i.type)
            {
                case evalint:
                {
                    out = val.i.x;
                }
                break;

                case evalfloat:
                {
                    #ifdef usemath
                    out = floor(val.f.x);
                    #else
                    out = val.f.x;
                    #endif
                }
                break;

                case evalstr:
                {
                    invalidtypeerror(s, "char ", val, (union evaltype)(typeof(val.i)){0, 0});
                }
                break;
            }
            string t;
            t.cap = 1;
            t.len = 1;
            t.values[0] = out;
            val.s.type = evalstr;
            val.s.x = t;
            return val;
        }
        break;
    }
    after2:;
    if(s->values[0] == '"' && s->values[s->len-1] == '"')
    {
        for(int i = 1; i < s->len-1; i++) if(s->values[i] == '"' && s->values[i-1] != '\\') goto after3;
        struct string out;
        out.len = 0;
        out.cap = s->len-2;
        out.values = malloc(out.cap*sizeof(*out.values));
        for(int i = 1; i < s->len-1; i++)
        {
            if(s->values[i] == '\\')
            {
                i++;
                switch (s->values[i])
                {
                    case '0':
                    case '1':
                    case '2':
                    case '3':
                    case '4':
                    case '5':
                    case '6':
                    {
                        out.values[out.len++] = s->values[i]-'0';
                    }
                    break;

                    case 'a':
                    {
                        out.values[out.len++] = 7;
                    }
                    break;

                    case 'b':
                    {
                        out.values[out.len++] = 8;
                    }
                    break;
                    
                    case 't':
                    {
                        out.values[out.len++] = 9;
                    }
                    break;
                    
                    case 'n':
                    {
                        out.values[out.len++] = 10;
                    }
                    break;
                    
                    case 'v':
                    {
                        out.values[out.len++] = 11;
                    }
                    break;
                    
                    case 'f':
                    {
                        out.values[out.len++] = 12;
                    }
                    break;
                    
                    case 'r':
                    {
                        out.values[out.len++] = 13;
                    }
                    break;
                    
                    case 'e':
                    {
                        out.values[out.len++] = 27;
                    }
                    break;
                    
                    case 'x':
                    {
                        unsigned int num = 0;
                        for(int j = 0; j < 2; j++)
                        {
                            i++;
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
                                    error(s, i, "Invalid escape sequence");
                            }
                        }
                        out.values[out.len++] = num;
                    }
                    break;
                    
                    case 'u':
                    {
                        unsigned int num = 0;
                        for(int j = 0; j < 4; j++)
                        {
                            i++;
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
                                    error(s, i, "Invalid escape sequence");
                            }
                        }
                        out.values[out.len++] = num;
                    }
                    break;
                    
                    case 'U':
                    {
                        unsigned int num = 0;
                        for(int j = 0; j < 8; j++)
                        {
                            i++;
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
                                    error(s, i, "Invalid escape sequence");
                            }
                        }
                        out.values[out.len++] = num;
                    }
                    break;

                    default:
                    {
                        out.values[out.len++] = s->values[i];
                    }
                    break;
                }
            }
            else
            {
                out.values[out.len++] = s->values[i];
            }
        }
        union evaltype out2;
        out2.s.type = evalstr;
        out2.s.x = out;
        return out2;
    }
    after3:;
    int ident = 0;
    char instr = 0;
    for(int i = s->len-1; i >= 1; i--)
    {
        if(s->values[i] == '"')
            instr = !instr;
        if(instr)
            continue;
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
                    union evaltype x1 = eval(s1, this);
                    free(s1);
                    union evaltype out;
                    out.i.type = evalint;
                    if(x1.i.type != evalint)
                        error(s, 0, "\"&&\" expects int");
                    if(!x1.i.x)
                    {
                        out.i.x = 0;
                        return out;
                    }
                    str s2 = s_substring(s, i+1, s->len-i-1);
                    union evaltype x2 = eval(s2, this);
                    free(s2);
                    if(x2.i.type != evalint)
                        error(s, i+1, "\"&&\" expects int");
                    out.i.x = !!x2.i.x;
                    return out;
                }
                error(s, i, "'&' isn't a valid operator");
            }
            else if(s->values[i] == '|')
            {
                if(s->values[i-1] == '|')
                {
                    str s1 = s_substring(s, 0, i-1);
                    union evaltype x1 = eval(s1, this);
                    free(s1);
                    union evaltype out;
                    out.i.type = evalint;
                    if(x1.i.type != evalint)
                        error(s, 0, "\"||\" expects int");
                    if(x1.i.x)
                    {
                        out.i.x = 1;
                        return out;
                    }
                    str s2 = s_substring(s, i+1, s->len-i-1);
                    union evaltype x2 = eval(s2, this);
                    free(s2);
                    if(x2.i.type != evalint)
                        error(s, i+1, "\"||\" expects int");
                    out.i.x = !!x2.i.x;
                    return out;
                }
                error(s, i, "'|' isn't a valid operator");
            }
            else if(s->values[i] == '^')
            {
                if(s->values[i-1] == '^')
                {
                    str s1 = s_substring(s, 0, i-1);
                    union evaltype x1 = eval(s1, this);
                    free(s1);
                    union evaltype out;
                    out.i.type = evalint;
                    if(x1.i.type != evalint)
                        error(s, 0, "\"^^\" expects int");
                    str s2 = s_substring(s, i+1, s->len-i-1);
                    union evaltype x2 = eval(s2, this);
                    free(s2);
                    if(x2.i.type != evalint)
                        error(s, i+1, "\"^^\" expects int");
                    out.i.x = !!x1.i.x ^ !!x2.i.x;
                    return out;
                }
                error(s, i, "'^' isn't a valid operator");
            }
        }
    }
    ident = 0;
    instr = 0;
    for(int i = s->len-1; i >= 1; i--)
    {
        if(s->values[i] == '"')
            instr = !instr;
        if(instr)
            continue;
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
                union evaltype x1 = eval(s1, this);
                union evaltype x2 = eval(s2, this);
                if(x1.i.type != x2.i.type)
                    invalidtypeerror(s1, "<", x1, x2);
                free(s1);
                free(s2);
                char val;
                switch (x1.i.type)
                {
                    case evalint:
                    {
                        val = x1.i.x < x2.i.x;
                    }
                    break;
                    
                    case evalfloat:
                    {
                        val = x1.f.x < x2.f.x;
                    }
                    break;
                    
                    case evalstr:
                    {
                        val = s_cmp(&x1.s.x, &x2.s.x) < 0;
                        free(x1.s.x.values);
                        free(x2.s.x.values);
                    }
                    break;
                    
                }
                x1.i.type = evalint;
                x1.i.x = val;
                return x1;
            }
            else if(s->values[i] == '>')
            {
                str s1 = s_substring(s, 0, i);
                str s2 = s_substring(s, i+1, s->len-i-1);
                union evaltype x1 = eval(s1, this);
                union evaltype x2 = eval(s2, this);
                if(x1.i.type != x2.i.type)
                    invalidtypeerror(s1, ">", x1, x2);
                free(s1);
                free(s2);
                char val;
                switch (x1.i.type)
                {
                    case evalint:
                    {
                        val = x1.i.x > x2.i.x;
                    }
                    break;
                    
                    case evalfloat:
                    {
                        val = x1.f.x > x2.f.x;
                    }
                    break;
                    
                    case evalstr:
                    {
                        val = s_cmp(&x1.s.x, &x2.s.x) > 0;
                        free(x1.s.x.values);
                        free(x2.s.x.values);
                    }
                    break;
                    
                }
                x1.i.type = evalint;
                x1.i.x = val;
                return x1;
            }
            else if(s->values[i] == '=')
            {
                if(--i >= 0)
                {
                    if(s->values[i] == '=')
                    {
                        str s1 = s_substring(s, 0, i);
                        str s2 = s_substring(s, i+2, s->len-i-2);
                        union evaltype x1 = eval(s1, this);
                        union evaltype x2 = eval(s2, this);
                        if(x1.i.type != x2.i.type)
                            invalidtypeerror(s1, "==", x1, x2);
                        free(s1);
                        free(s2);
                        char val;
                        switch (x1.i.type)
                        {
                            case evalint:
                            {
                                val = x1.i.x == x2.i.x;
                            }
                            break;
                            
                            case evalfloat:
                            {
                                val = x1.f.x == x2.f.x;
                            }
                            break;
                            
                            case evalstr:
                            {
                                val = s_cmp(&x1.s.x, &x2.s.x) == 0;
                                free(x1.s.x.values);
                                free(x2.s.x.values);
                            }
                            break;
                            
                        }
                        x1.i.type = evalint;
                        x1.i.x = val;
                        return x1;
                    }
                    else if(s->values[i] == '>')
                    {
                        str s1 = s_substring(s, 0, i);
                        str s2 = s_substring(s, i+2, s->len-i-2);
                        union evaltype x1 = eval(s1, this);
                        union evaltype x2 = eval(s2, this);
                        if(x1.i.type != x2.i.type)
                            invalidtypeerror(s1, ">=", x1, x2);
                        free(s1);
                        free(s2);
                        char val;
                        switch (x1.i.type)
                        {
                            case evalint:
                            {
                                val = x1.i.x >= x2.i.x;
                            }
                            break;
                            
                            case evalfloat:
                            {
                                val = x1.f.x >= x2.f.x;
                            }
                            break;
                            
                            case evalstr:
                            {
                                val = s_cmp(&x1.s.x, &x2.s.x) >= 0;
                                free(x1.s.x.values);
                                free(x2.s.x.values);
                            }
                            break;
                            
                        }
                        x1.i.type = evalint;
                        x1.i.x = val;
                        return x1;
                    }
                    else if(s->values[i] == '<')
                    {
                        str s1 = s_substring(s, 0, i);
                        str s2 = s_substring(s, i+2, s->len-i-2);
                        union evaltype x1 = eval(s1, this);
                        union evaltype x2 = eval(s2, this);
                        if(x1.i.type != x2.i.type)
                            invalidtypeerror(s1, "<=", x1, x2);
                        free(s1);
                        free(s2);
                        char val;
                        switch (x1.i.type)
                        {
                            case evalint:
                            {
                                val = x1.i.x <= x2.i.x;
                            }
                            break;
                            
                            case evalfloat:
                            {
                                val = x1.f.x <= x2.f.x;
                            }
                            break;
                            
                            case evalstr:
                            {
                                val = s_cmp(&x1.s.x, &x2.s.x) <= 0;
                                free(x1.s.x.values);
                                free(x2.s.x.values);
                            }
                            break;
                            
                        }
                        x1.i.type = evalint;
                        x1.i.x = val;
                        return x1;
                    }
                    else if(s->values[i] == '!')
                    {
                        str s1 = s_substring(s, 0, i);
                        str s2 = s_substring(s, i+2, s->len-i-2);
                        union evaltype x1 = eval(s1, this);
                        union evaltype x2 = eval(s2, this);
                        if(x1.i.type != x2.i.type)
                            invalidtypeerror(s1, "!=", x1, x2);
                        free(s1);
                        free(s2);
                        char val;
                        switch (x1.i.type)
                        {
                            case evalint:
                            {
                                val = x1.i.x != x2.i.x;
                            }
                            break;
                            
                            case evalfloat:
                            {
                                val = x1.f.x != x2.f.x;
                            }
                            break;
                            
                            case evalstr:
                            {
                                val = s_cmp(&x1.s.x, &x2.s.x) != 0;
                                free(x1.s.x.values);
                                free(x2.s.x.values);
                            }
                            break;
                            
                        }
                        x1.i.type = evalint;
                        x1.i.x = val;
                        return x1;
                    }
                }
                error(s, i, "'=' isn't a valid operator");
            }
        }
    }
    ident = 0;
    instr = 0;
    for(int i = s->len-1; i >= 1; i--)
    {
        if(s->values[i] == '"')
            instr = !instr;
        if(instr)
            continue;
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
                union evaltype x1 = eval(s1, this);
                union evaltype x2 = eval(s2, this);
                if(x1.i.type != x2.i.type)
                    invalidtypeerror(s1, "+", x1, x2);
                free(s1);
                free(s2);
                switch (x1.i.type)
                {
                    case evalint:
                    {
                        __int128 val = x1.i.x + x2.i.x;
                        x1.i.type = evalint;
                        x1.i.x = val;
                        return x1;
                    }
                    break;
                    
                    case evalfloat:
                    {
                        double val = x1.f.x + x2.f.x;
                        x1.f.type = evalfloat;
                        x1.f.x = val;
                        return x1;
                    }
                    break;
                    
                    case evalstr:
                    {
                        string s;
                        s.len = x1.s.x.len + x2.s.x.len;
                        s.cap = s.len;
                        s.values = malloc(s.len * sizeof(*s.values));
                        memcpy(s.values, x1.s.x.values, x1.s.x.len * sizeof(*s.values));
                        memcpy(&s.values[x1.s.x.len], x2.s.x.values, x2.s.x.len * sizeof(*s.values));
                        free(x1.s.x.values);
                        free(x2.s.x.values);
                        x1.s.type = evalstr;
                        x1.s.x = s;
                        return x1;
                    }
                    break;
                }
            }
            else if(s->values[i] == '-')
            {
                str s1 = s_substring(s, 0, i);
                str s2 = s_substring(s, i+1, s->len-i-1);
                union evaltype x1 = eval(s1, this);
                union evaltype x2 = eval(s2, this);
                if(x1.i.type != x2.i.type || x1.i.type == evalstr)
                    invalidtypeerror(s1, "-", x1, x2);
                free(s1);
                free(s2);
                switch (x1.i.type)
                {
                    case evalint:
                    {
                        __int128 val = x1.i.x - x2.i.x;
                        x1.i.type = evalint;
                        x1.i.x = val;
                        return x1;
                    }
                    break;
                    
                    case evalfloat:
                    {
                        double val = x1.f.x - x2.f.x;
                        x1.f.type = evalfloat;
                        x1.f.x = val;
                        return x1;
                    }
                    break;
                }
            }
        }
    }
    ident = 0;
    instr = 0;
    for(int i = s->len-1; i >= 1; i--)
    {
        if(s->values[i] == '"')
            instr = !instr;
        if(instr)
            continue;
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
                union evaltype x1 = eval(s1, this);
                union evaltype x2 = eval(s2, this);
                if(x1.i.type == evalint && x2.i.type == evalstr)
                {
                    string s;
                    s.len = x1.i.x * x2.s.x.len;
                    if(s.len > 100000)
                        error(s1, i, "Maximum string length for multiplication exeeded.");
                    s.cap = s.len;
                    s.values = malloc(s.len * sizeof(*s.values));
                    for(int i = 0; i < x1.i.x; i++)
                        memcpy(&s.values[x2.s.x.len * i], x2.s.x.values, x2.s.x.len * sizeof(*s.values));
                    free(x2.s.x.values);
                    x1.s.type = evalstr;
                    x1.s.x = s;
                    return x1;
                }
                if(x1.i.type == evalstr && x2.i.type == evalint)
                {
                    string s;
                    s.len = x1.s.x.len * x2.i.x;
                    if(s.len > 100000)
                        error(s1, i, "Maximum string length for multiplication exeeded.");
                    s.cap = s.len;
                    s.values = malloc(s.len * sizeof(*s.values));
                    for(int i = 0; i < x2.i.x; i++)
                        memcpy(&s.values[x1.s.x.len * i], x1.s.x.values, x1.s.x.len * sizeof(*s.values));
                    free(x1.s.x.values);
                    x1.s.type = evalstr;
                    x1.s.x = s;
                    return x1;
                }
                if(x1.i.type != x2.i.type || x1.i.type == evalstr)
                    invalidtypeerror(s1, "*", x1, x2);
                free(s1);
                free(s2);
                switch (x1.i.type)
                {
                    case evalint:
                    {
                        __int128 val = x1.i.x * x2.i.x;
                        x1.i.type = evalint;
                        x1.i.x = val;
                        return x1;
                    }
                    break;
                    
                    case evalfloat:
                    {
                        double val = x1.f.x * x2.f.x;
                        x1.f.type = evalfloat;
                        x1.f.x = val;
                        return x1;
                    }
                    break;
                }
            }
            else if(s->values[i] == '/')
            {
                str s1 = s_substring(s, 0, i);
                str s2 = s_substring(s, i+1, s->len-i-1);
                union evaltype x1 = eval(s1, this);
                union evaltype x2 = eval(s2, this);
                if(x1.i.type != x2.i.type || x1.i.type == evalstr)
                    invalidtypeerror(s1, "/", x1, x2);
                free(s1);
                free(s2);
                switch (x1.i.type)
                {
                    case evalint:
                    {
                        __int128 val = x1.i.x / x2.i.x;
                        x1.i.type = evalint;
                        x1.i.x = val;
                        return x1;
                    }
                    break;
                    
                    case evalfloat:
                    {
                        double val = x1.f.x / x2.f.x;
                        x1.f.type = evalfloat;
                        x1.f.x = val;
                        return x1;
                    }
                    break;
                }
            }
        }
    }
    #ifdef usemath
    ident = 0;
    instr = 0;
    for(int i = 1; i < s->len; i++)
    {
        if(s->values[i] == '"')
            instr = !instr;
        if(instr)
            continue;
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
            union evaltype x1 = eval(s1, this);
            union evaltype x2 = eval(s2, this);
            if(x1.i.type != x2.i.type || x1.i.type == evalstr)
                invalidtypeerror(s1, "^", x1, x2);
            free(s1);
            free(s2);
            switch (x1.i.type)
            {
                case evalint:
                {
                    __int128 val = 1;
                    unsigned __int128 exp = x2.i.x;
                    if(exp < ((unsigned __int128)1 << 127))
                        error(s2, 0, "Exponent of integer multiplication is negative.");
                    exp = (exp << 1) & (exp >> 127);
                    int bits = 128;
                    for(; !(exp & 1); exp = (exp << 1) & (exp >> 127), bits--);
                    for(int i = 0; i < bits; i++)
                    {
                        val = val * val;
                        if(exp & 1)
                            val *= x1.i.x;
                        exp = (exp << 1) & (exp >> 127);
                    }
                    x1.i.type = evalint;
                    x1.i.x = val;
                    return x1;
                }
                break;
                
                case evalfloat:
                {
                    double val = pow(x1.f.x, x2.f.x);
                    x1.f.type = evalfloat;
                    x1.f.x = val;
                    return x1;
                }
                break;
            }
        }
    }
    #endif
    if(s->values[0] == L'$')
    {
        if(s->values[1] == L'_' && s->len == 2)
        {
            union evaltype t;
            t.i.type = evalint;
            t.i.x = stacktrace->len;
            return t;
        }
        s->values++;
        s->len--;
        union evaltype i = eval(s, this);
        if(i.i.type != evalint)
            invalidtypeerror(s, "$", i, (union evaltype)(typeof(i.i)){0, 0});
        union evaltype t;
        t.i.type = evalint;
        t.i.x = v_get(stacktrace, i.i.x, int);
        return t;
    }
    if(s->values[0] == '+')
    {
        str s1 = s_substring(s, 1, s->len-1);
        union evaltype x1 = eval(s1, this);
        if(x1.i.type != evalint && x1.i.type != evalfloat)
            invalidtypeerror(s1, "+", x1, (union evaltype)(typeof(x1.i)){0, 0});
        free(s1);
        switch (x1.i.type)
        {
            case evalint:
            case evalfloat:
            {
                return x1;
            }
            break;
        }
    }
    if(s->values[0] == '-')
    {
        str s1 = s_substring(s, 1, s->len-1);
        union evaltype x1 = eval(s1, this);
        if(x1.i.type != evalint && x1.i.type != evalfloat)
            invalidtypeerror(s1, "-", x1, (union evaltype)(typeof(x1.i)){0, 0});
        free(s1);
        switch (x1.i.type)
        {
            case evalint:
            {
                x1.i.x = -x1.i.x;
            }
            break;

            case evalfloat:
            {
                x1.f.x = -x1.f.x;
            }
            break;
        }
        return x1;
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
                    union evaltype out;
                    out.i.type = evalint;
                    out.i.x = num;
                    return out;
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
                    union evaltype out;
                    out.i.type = evalint;
                    out.i.x = num;
                    return out;
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
                    union evaltype out;
                    out.i.type = evalint;
                    out.i.x = num;
                    return out;
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
                    union evaltype out;
                    out.i.type = evalint;
                    out.i.x = num;
                    return out;
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
        union evaltype out;
        out.i.type = evalint;
        out.i.x = num;
        return out;
    }
    next:;
    if(s_cmp_str(s, "here") == 0)
    {
        union evaltype out;
        out.i.type = evalint;
        out.i.x = filepos;
        return out;
    }
    if(s_cmp_str(s, "true") == 0)
    {
        union evaltype out;
        out.i.type = evalint;
        out.i.x = 1;
        return out;
    }
    if(s_cmp_str(s, "false") == 0)
    {
        union evaltype out;
        out.i.type = evalint;
        out.i.x = 0;
        return out;
    }
    union baum _ast;
    union baum* ast = &_ast;
    union baum* prevast;
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

            union evaltype _index = eval(&_t, this);
            if(_index.i.type != evalint)
                invalidtypeerror(&_t, "[] ", _index, (union evaltype)(typeof(_index.i)){0, 0});
            free(sub.values);
            if(_index.i.x < 0)
                error(s, 0, "Index is below 0");
            int i = 0;
            for(; _index.i.x < potenzen[i]; i++);
            if(potenzen[i] == 0)
                s_append(t, '0');
            else
            {
                for(; i < lenof(potenzen)-1; i++)
                {
                    s_append(t, _index.i.x/potenzen[i]+0x30);
                    _index.i.x %= potenzen[i];
                }
            }
            s_append(t, ']');
        }

        for(int i = 0; i < ast->tree.entries->len; i++)
        {
            char res = s_cmp_str(t, v_get(ast->tree.entries, i, union baum*)->tree.name);
            if(res == 0 || res == -1)
            {
                if(v_get(ast->tree.entries, i, union baum*)->tree._type == 1)
                {
                    if(res != 0)
                        continue;
                    ast = v_get(ast->tree.entries, i, union baum*);
                    if(index == -1)
                        error(s, index, "Struct has no value");
                    s->values += index+1;
                    s->len -= index+1;
                    goto after;
                }
                else
                {
                    prevast = ast;
                    ast = v_get(ast->tree.entries, i, union baum*);
                    if(res == -1 && ast->assigned.type != s8 && ast->assigned.type != s16 && ast->assigned.type != s32)
                        continue;
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
    union evaltype out;
    switch (ast->assigned.type)
    {
        case s8:
        case s16:
        case s32:
        {
            int geslen = strlen(ast->assigned.name);
            int baselen = 0;
            for(; ast->assigned.name[baselen] != '['; baselen++) if(geslen <= baselen) goto intdef;
            baselen++;
            char* base = ast->assigned.name;
            int strstart = 0;
            for(; v_get(prevast->tree.entries, strstart, union baum*) != ast; strstart++);
            int strend = strstart;
            for(; prevast->tree.entries->len < strend; strend++)
            {
                union baum* temp = v_get(prevast->tree.entries, strend, union baum*);
                if(strncmp(temp->assigned.name, base, baselen) != 0 || temp->assigned._type == 1 || temp->assigned.type != ast->assigned.type)
                    break;
            }
            if(ast->assigned.type == s8)
            {
                char* tstr = malloc(strend-strstart+1);
                for(int i = strstart; i < strend; i++)
                    tstr[i] = v_get(prevast->tree.entries, strend, union baum*)->assigned._value;
                tstr[strend-strstart] = 0;
                union evaltype out;
                out.s.type = evalstr;
                str tstring = s_init_cstr(tstr);
                out.s.x = *tstring;
                free(tstr);
                free(tstring);
            }
            else
            {
                out.s.type = evalstr;
                out.s.x.len = strend-strstart;
                out.s.x.cap = out.s.x.len;
                out.s.x.values = malloc(out.s.x.len*sizeof(*out.s.x.values));
                for(int i = strstart; i < strend; i++)
                    out.s.x.values[i] = v_get(prevast->tree.entries, strend, union baum*)->assigned._value;
            }
        }
        break;

        case i8:
        case i16:
        case i32:
        case i64:
        case i128:
        case u8:
        case u16:
        case u32:
        case u64:
        case u128:
        case x8:
        case x16:
        case x32:
        case x64:
        case x128:
        case c8:
        case c16:
        case c32:
        {
            intdef:;
            out.i.type = evalint;
            out.i.x = ast->assigned._value;
        }
        break;

        case f32:
        {
            out.f.type = evalfloat;
            out.f.x = *(float*)(__int128*)&ast->assigned._value;
        }
        break;

        case f64:
        {
            out.f.type = evalfloat;
            out.f.x = *(double*)(__int128*)&ast->assigned._value;
        }
        break;

        default:
        error(NULL, 0, "Invalid type (bug)");
    }
    return out;
}

char contains(int c, int* chars)
{
    for(int i = 0; chars[i]; i++)
        if(chars[i] == c)
            return 1;
    return 0;
}

//belongs to addVar
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

        default:
        error(entryname, 0, "Invalid type (bug)");
    }

    if(endian == 'l')
    {
        for(int i = 0; i < 8 * size; i += 8)
        {
            int byte = readchar();
            if(byte == -1) berror(entryname, 0, "Unexpected end of binary file");
            var += (unsigned __int128)(unsigned char)byte << i;
        }
    }
    else
    {
        for(int i = 0; i < size; i++)
        {
            int byte = readchar();
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

//belongs to addVar
void setValue(char type, struct prevar entry, struct unassignedvar* v, union evaltype value, char* svalue, int slen, int start, int k)
{
    if(entry.value != NULL)
    {
        v->_type = 3;
        switch (v->type)
        {
            case s8:
            case s16:
            case s32:
            {
                if(type == 1)
                    goto chardef;
                if(value.i.type == evalstr)
                {
                    if(v->type == s8)
                    {
                        if(slen > k-start)
                            v->_value = svalue[k-start];
                        else
                            v->_type = 2;
                    }
                    else if(value.s.x.len > k-start)
                    {
                        v->_value = value.s.x.values[k-start];
                    }
                    else
                        v->_type = 2;
                    break;
                }
                else
                    goto intdef;
            }
            break;

            case c8:
            case c16:
            case c32:
            {
                chardef:;
                if(value.i.type == evalstr)
                {
                    if(value.s.x.len == 1)
                    {
                        uint32_t c = value.s.x.values[0];
                        if(v->type == c8 && c >= 0x100)
                            error(entry.value, 0, "size exeeds char size.");
                        if(v->type == c16 && c >= 0x10000)
                            error(entry.value, 0, "size exeeds char size.");
                        v->_value = c;
                        break;
                    }
                }
            }
            case i8:
            case i16:
            case i32:
            case i64:
            case i128:
            case u8:
            case u16:
            case u32:
            case u64:
            case u128:
            case x8:
            case x16:
            case x32:
            case x64:
            case x128:
            {
                intdef:;
                if(value.i.type != evalint)
                    error(entry.value, 0, "Invalid type");
                switch (v->type)
                {
                    case i8:
                    {
                        if(((unsigned __int128)value.i.x & ~(unsigned __int128)1 << 127) > 1 << 7)
                            error(entry.value, 0, "size exeeds int size.");
                    }
                    break;

                    case u8:
                    case x8:
                    case c8:
                    case s8:
                    {
                        if((unsigned __int128)value.i.x > 1 << 8)
                            error(entry.value, 0, "size exeeds int size.");
                    }
                    break;
                    
                    case i16:
                    {
                        if(((unsigned __int128)value.i.x & ~(unsigned __int128)1 << 127) > 1 << 15)
                            error(entry.value, 0, "size exeeds int size.");
                    }
                    break;

                    case u16:
                    case x16:
                    case c16:
                    case s16:
                    {
                        if((unsigned __int128)value.i.x > 1 << 16)
                            error(entry.value, 0, "size exeeds int size.");
                    }
                    break;
                    
                    case i32:
                    {
                        if(((unsigned __int128)value.i.x & ~(unsigned __int128)1 << 127) > (unsigned __int128)1 << 31)
                            error(entry.value, 0, "size exeeds int size.");
                    }
                    break;

                    case u32:
                    case x32:
                    case c32:
                    case s32:
                    {
                        if((unsigned __int128)value.i.x > (unsigned __int128)1 << 32)
                            error(entry.value, 0, "size exeeds int size.");
                    }
                    break;
                    
                    case i64:
                    {
                        if(((unsigned __int128)value.i.x & ~(unsigned __int128)1 << 127) > (unsigned __int128)1 << 63)
                            error(entry.value, 0, "size exeeds int size.");
                    }
                    break;

                    case u64:
                    case x64:
                    {
                        if((unsigned __int128)value.i.x > (unsigned __int128)1 << 64)
                            error(entry.value, 0, "size exeeds int size.");
                    }
                    break;

                    default:
                    error(entry.name, 0, "Invalid type (bug)");
                }

                v->_value = value.i.x;
            }
            break;

            case f32:
            case f64:
            {
                if(value.i.type != evalfloat)
                    error(entry.value, 0, "Invalid type");
                *(double*)&v->_value = value.f.x;
            }
            break;

            default:
            error(entry.name, 0, "Invalid type (bug)");
        }
    }
    else
    {
        v->_type = 2;
    }
}

//belongs to addVar
enum type parsetype(str stype)
{
    if(s_cmp_str(stype, "i8") == 0)
    {
        return i8;
    }
    else if(s_cmp_str(stype, "i16") == 0)
    {
        return i16;
    }
    else if(s_cmp_str(stype, "i32") == 0)
    {
        return i32;
    }
    else if(s_cmp_str(stype, "i64") == 0)
    {
        return i64;
    }
    else if(s_cmp_str(stype, "i128") == 0)
    {
        return i128;
    }
    else if(s_cmp_str(stype, "u8") == 0)
    {
        return u8;
    }
    else if(s_cmp_str(stype, "u16") == 0)
    {
        return u16;
    }
    else if(s_cmp_str(stype, "u32") == 0)
    {
        return u32;
    }
    else if(s_cmp_str(stype, "u64") == 0)
    {
        return u64;
    }
    else if(s_cmp_str(stype, "u128") == 0)
    {
        return u128;
    }
    else if(s_cmp_str(stype, "x8") == 0)
    {
        return x8;
    }
    else if(s_cmp_str(stype, "x16") == 0)
    {
        return x16;
    }
    else if(s_cmp_str(stype, "x32") == 0)
    {
        return x32;
    }
    else if(s_cmp_str(stype, "x64") == 0)
    {
        return x64;
    }
    else if(s_cmp_str(stype, "x128") == 0)
    {
        return x128;
    }
    else if(s_cmp_str(stype, "f32") == 0)
    {
        return f32;
    }
    else if(s_cmp_str(stype, "f64") == 0)
    {
        return f64;
    }
    else if(s_cmp_str(stype, "c8") == 0)
    {
        return c8;
    }
    else if(s_cmp_str(stype, "c16") == 0)
    {
        return c16;
    }
    else if(s_cmp_str(stype, "c32") == 0)
    {
        return c32;
    }
    else if(s_cmp_str(stype, "s8") == 0)
    {
        return s8;
    }
    else if(s_cmp_str(stype, "s16") == 0)
    {
        return s16;
    }
    else if(s_cmp_str(stype, "s32") == 0)
    {
        return s32;
    }
    else
    {
        return none;
    }
}

//belongs to main
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
            urindex = filepos;
            union evaltype pos = eval(entry->plocation, *this);
            if(pos.i.type != evalint)
                error(entry->plocation, 0, "The location prefix expects an int.");
            int err = seek(pos.i.x);
            if(err == ESPIPE)
                error(entry->plocation, 0, "You can't use the prefix \"location\" for an unseekable stream.\n");
            else if(err == EINVAL)
                error(entry->plocation, 0, "\"location\" argument is outside the file.\n");
            else if(err)
                error(entry->plocation, 0, "Unknown error from fseek.\n");
        }
        if(entry->pendian)
        {
            union evaltype temp = eval(entry->pendian, *this);
            if(temp.i.type == evalstr)
            {
                if(temp.s.x.len != 1)
                    error(entry->pendian, 0, "endian expects string of size 1");
                if(!contains(temp.s.x.values[0], L"lhLH"))
                    error(entry->pendian, 0, "endian only accepts 'l' and 'h'");
                if(temp.s.x.values[0] == 'L')
                    endian = 'l';
                else if(temp.s.x.values[0] == 'H')
                    endian = 'h';
                else
                    endian = temp.s.x.values[0];
            }
            else if(temp.i.type == evalint)
            {
                if(temp.i.x < 0 || temp.i.x > 1)
                    error(entry->pendian, 0, "endian only accepts boolean values.");
                if(temp.i.x)
                    endian = 'h';
                else
                    endian = 'l';
            }
            else
                error(entry->pendian, 0, "endian doesn't accept double.");
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
                union evaltype temp = eval(&s1, *this);
                if(temp.i.type != evalint)
                    error(&s1, 0, "Array boundry must be of type int.");
                start = temp.i.x;
                
                string s2;
                s2.values = &entry->type->values[start2];
                s2.cap = 0;
                s2.len = end2-start2-1;
                temp = eval(&s2, *this);
                if(temp.i.type != evalint)
                    error(&s2, 0, "Array boundry must be of type int.");
                end = temp.i.x;
            }
            else
            {
                start = 0;
                string s1;
                s1.values = &entry->type->values[start1];
                s1.cap = 0;
                s1.len = entry->type->len-start1-1;
                union evaltype temp = eval(&s1, *this);
                if(temp.i.type != evalint)
                    error(&s1, 0, "Array boundry must be of type int.");
                end = temp.i.x;
            }
            if(start == 0 && end == 1)
            {
                if(entry->psimplify)
                {
                    union evaltype pos = eval(entry->psimplify, *this);
                    if(pos.i.type != evalint)
                        error(entry->plocation, 0, "The simplify prefix expects an int.");
                    if(!pos.i.x)
                        goto ignore;
                }
                realtypelen = start1-1;

                goto noarray;
            }
            ignore:;
            if(end-start > maxarraysize && maxarraysize != 0)
            {
                char errmsg[] = "exceeded the maximal array size: 0000000/0000000";
                int realsize = end-start;
                int maxsize = maxarraysize;
                if(realsize > 9999999 || maxsize > 9999999)
                    errmsg[33] = 0;
                else
                {
                    for(int i = 33, j = 41, k = 12; i < 40; i++, j++, k++)
                    {
                        errmsg[i] = realsize / potenzen[k]+0x30;
                        realsize %= potenzen[k];
                        errmsg[j] = maxsize / potenzen[k]+0x30;
                        maxsize %= potenzen[k];
                    }
                }
                error(entry->type, 0, errmsg);
            }

            string typestring = *entry->type;
            typestring.len = start1-1;
            enum type type = parsetype(&typestring);
            if(entry->pinline && type != none)
                error(entry->pinline, 0, "Standart types can't have the prefix \"inline\".");
            if(entry->pvalueonly && type == none)
                error(entry->pvalueonly, 0, "Array types can't have the prefix \"valueonly\".");
            union evaltype value;
            char* svalue;
            long slen;
            if(entry->value != NULL)
            {
                value = eval(entry->value, *this);
                if(type == s8 && value.i.type == evalstr)
                {
                    svalue = s_cstr(&value.s.x);
                    slen = strlen(svalue);
                }
                else
                    slen = 0;
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
                if(type != none)
                {
                    v->type = type;

                    v->pvalues = malloc(sizeof(struct dict));
                    v->pvalues->len = entry->pvalues->len;
                    v->pvalues->keys = malloc(v->pvalues->len * sizeof(union evaltype));
                    v->pvalues->values = malloc(v->pvalues->len * sizeof(union evaltype));
                    for(int i = 0; i < entry->pvalues->len; i++)
                    {
                        union evaltype key = eval(&entry->pvalues->keys[i].s.x, *this);
                        union evaltype value = eval(&entry->pvalues->values[i].s.x, *this);
                        if(value.i.type != evalstr)
                            error(&entry->pvalues->values[i].s.x, 0, "The value prefix expects a string as value");
                        entry->pvalues->keys[i];
                        v->pvalues->keys[i] = key;
                        v->pvalues->values[i] = value;
                    }
                    if(entry->pvalueonly)
                    {
                        union evaltype temp = eval(entry->pvalueonly, *this);
                        if(temp.i.type == evalstr)
                        {
                            if(temp.s.x.len != 1)
                                error(entry->pvalueonly, 0, "valueonly expects string of size 1");
                            if(!contains(temp.s.x.values[0], L"ynYN"))
                                error(entry->pvalueonly, 0, "valueonly only accepts 'y' and 'n'");
                            if(temp.s.x.values[0] == 'Y')
                                v->pvalueonly = 'y';
                            else if(temp.s.x.values[0] == 'N')
                                v->pvalueonly = 'n';
                            else
                                v->pvalueonly = temp.s.x.values[0];
                        }
                        else if(temp.i.type == evalint)
                        {
                            if(temp.i.x < 0 || temp.i.x > 1)
                                error(entry->pvalueonly, 0, "valueonly only accepts boolean values.");
                            if(temp.i.x)
                                v->pvalueonly = 'y';
                            else
                                v->pvalueonly = 'n';
                        }
                        else
                            error(entry->pvalueonly, 0, "valueonly doesn't accept double.");
                    }
                    else
                        v->pvalueonly = standartvalueonly;
                }
                else
                {
                    v_push(stacktrace, k);
                    struct tree* t = (struct tree*)v;
                    if(entry->pinline)
                    {
                        union evaltype temp = eval(entry->pinline, *this);
                        if(temp.i.type != evalint)
                            error(entry->pinline, 0, "The inline prefix expects an int.");
                        t->pinline = temp.i.x;
                    }
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
                setValue(0, *entry, v, value, svalue, slen, start, k);
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
            v->type = parsetype(&realtype);
            if(v->type == none)
            {
                if(entry->pvalueonly)
                    error(entry->pvalueonly, 0, "Array types can't have the prefix \"valueonly\".");

                struct tree* t = (struct tree*)v;
                if(entry->pinline)
                {
                    union evaltype temp = eval(entry->pinline, *this);
                    if(temp.i.type != evalint)
                        error(entry->pinline, 0, "The inline prefix expects an int.");
                    t->pinline = temp.i.x;
                }
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
                error(entry->type, 0, "Unknown type");
                after2:;
                if(entry->value != NULL)
                    error(entry->value, 0, "Struct can't have a value");
                t->_type = 1;
                v_push(this->entries, t);
                continue;
            }
            else if(entry->pinline)
                error(entry->pinline, 0, "Standart types can't have the prefix \"inline\".");
            else
            {
                v->pvalues = malloc(sizeof(struct dict));
                v->pvalues->len = entry->pvalues->len;
                v->pvalues->keys = malloc(v->pvalues->len * sizeof(union evaltype));
                v->pvalues->values = malloc(v->pvalues->len * sizeof(union evaltype));
                for(int i = 0; i < entry->pvalues->len; i++)
                {
                    union evaltype key = eval(&entry->pvalues->keys[i].s.x, *this);
                    union evaltype value = eval(&entry->pvalues->values[i].s.x, *this);
                    if(value.i.type != evalstr)
                        error(&entry->pvalues->values[i].s.x, 0, "The value prefix expects a string as value");
                    entry->pvalues->keys[i];
                    v->pvalues->keys[i] = key;
                    v->pvalues->values[i] = value;
                }

                if(entry->pvalueonly)
                {
                    union evaltype temp = eval(entry->pvalueonly, *this);
                    if(temp.i.type == evalstr)
                    {
                        if(temp.s.x.len != 1)
                            error(entry->pvalueonly, 0, "valueonly expects string of size 1");
                        if(!contains(temp.s.x.values[0], L"ynYN"))
                            error(entry->pvalueonly, 0, "valueonly only accepts 'y' and 'n'");
                        if(temp.s.x.values[0] == 'Y')
                            v->pvalueonly = 'y';
                        else if(temp.s.x.values[0] == 'N')
                            v->pvalueonly = 'n';
                        else
                            v->pvalueonly = temp.s.x.values[0];
                    }
                    else if(temp.i.type == evalint)
                    {
                        if(temp.i.x < 0 || temp.i.x > 1)
                            error(entry->pvalueonly, 0, "valueonly only accepts boolean values.");
                        if(temp.i.x)
                            v->pvalueonly = 'y';
                        else
                            v->pvalueonly = 'n';
                    }
                    else
                        error(entry->pvalueonly, 0, "valueonly doesn't accept double.");
                }
                else
                    v->pvalueonly = standartvalueonly;
            }
            union evaltype temp;
            if(entry->value != NULL)
            {
                temp = eval(entry->value, *this);
            }
            setValue(1, *entry, v, temp, NULL, 0, 0, 0);
            unsigned __int128 x = readbytes(v->type, entry->name);
            if(v->_type == 2)
                v->_value = x;
            else if(v->_value != x)
                berror(entry->value, 0, "Document has a different byte before, not");
            v_push(this->entries, v);
        }
        if(entry->plocation)
        {
            int err = seek(urindex);
            if(err == ESPIPE)
                error(entry->plocation, 0, "You can't use the prefix \"location\" for an unseekable stream.\n");
            else if(err == EINVAL)
                error(entry->plocation, 0, "\"location\" argument is outside the file.\n");
            else if(err)
                error(entry->plocation, 0, "Unknown error from fseek.\n");
        }
    }
}

//belongs to printtree
void printnum(unsigned __int128 x)
{
    if(x == 0)
    {
        printf("0");
        return;
    }
    int i = 0;
    for(; x < potenzen[i]; i++);
    for(; i < lenof(potenzen)-1; i++)
    {
        printf("%i", (int)(x / potenzen[i]));
        x %= potenzen[i];
    }
}

//belongs to printtree
void printhexnum(unsigned __int128 x)
{
    if(x == 0)
    {
        printf("0");
        return;
    }
    unsigned __int128 i = (__int128)1 << 124;
    for(; x < i; i >>= 4);
    for(; i; i >>= 4)
    {
        printf("%X", (int)(x / i));
        x %= i;
    }
}

//belongs to printtree
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

//belongs to printtree
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

//belongs to printtree
int getValueIndex(struct assignedvar var)
{
    int vartype;
    switch (var.type)
    {
        case i8:
        case u8:
        case x8:
        case c8:
        case s8:
        case i16:
        case u16:
        case x16:
        case c16:
        case s16:
        case i32:
        case u32:
        case x32:
        case c32:
        case s32:
        case i64:
        case u64:
        case x64:
        case i128:
        case u128:
        case x128:
        {
            vartype = 1;
        }
        break;

        case f32:
        case f64:
        {
            vartype = 2;
        }
        break;
    }
    for(int i = 0; i < var.pvalues->len; i++)
    {
        if(var.pvalues->keys[i].i.type != vartype)
            error(NULL, 0, "Invalid key type of the \"value\" prefix");
        
        if(var.pvalues->keys[i].i.x == var._value)
        {
            return i;
        }
    }
    return -1;
}

//belongs to printtree
//pvalues
void printValue(struct assignedvar var, int index)
{
    if(index != -1)
    {
        printf("<");
        for(int j = 0; j < var.pvalues->values[index].s.x.len; j++)
            printchar(var.pvalues->values[index].s.x.values[j]);
        printf(">");
    }
}

//belongs to main
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
                #ifndef tablen
                for(int i = 0; i < ident; i++) printf("\t");
                #else
                for(int i = 0; i < tablen*ident; i++) printf(" ");
                #endif
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
                        int index = getValueIndex(b->assigned);
                        if(b->assigned.pvalueonly == 'n' || index == -1)
                        {
                            printnum(b->assigned._value);
                            printf(" ");
                        }
                        printValue(b->assigned, index);
                        printf("\n");
                    }
                    break;

                    case c8:
                    {
                        printf("c8 %s=0x%X ", b->assigned.name, (int)b->assigned._value);
                        int index = getValueIndex(b->assigned);
                        if(b->assigned.pvalueonly == 'n' || index == -1)
                        {
                            printf("'");
                            printchar(b->assigned._value);
                            printf("'");
                            printf(" ");
                        }
                        printValue(b->assigned, index);
                        printf("\n");
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
                        int index = getValueIndex(b->assigned);
                        if(b->assigned.pvalueonly == 'n' || index == -1)
                        {
                            printnum(b->assigned._value);
                            printf(" ");
                        }
                        printValue(b->assigned, index);
                        printf("\n");
                    }
                    break;

                    case x8:
                    {
                        printf("x8 %s=", b->assigned.name);
                        int index = getValueIndex(b->assigned);
                        if(b->assigned.pvalueonly == 'n' || index == -1)
                        {
                            printhexnum(b->assigned._value);
                            printf(" ");
                        }
                        printValue(b->assigned, index);
                        printf("\n");
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
                        int index = getValueIndex(b->assigned);
                        if(b->assigned.pvalueonly == 'n' || index == -1)
                        {
                            printnum(b->assigned._value);
                            printf(" ");
                        }
                        printValue(b->assigned, index);
                        printf("\n");
                    }
                    break;

                    /*case f16:
                    {
                        printf("f16 %s=%f\n", b->assigned.name, (float)*(_Float16*)&b->assigned._value);
                    }
                    break;*/ //not supported on x86

                    case c16:
                    {
                        printf("c16 %s=0x%X ", b->assigned.name, (int)b->assigned._value);
                        int index = getValueIndex(b->assigned);
                        if(b->assigned.pvalueonly == 'n' || index == -1)
                        {
                            printf("'");
                            printchar(b->assigned._value);
                            printf("'");
                            printf(" ");
                        }
                        printValue(b->assigned, index);
                        printf("\n");
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
                        int index = getValueIndex(b->assigned);
                        if(b->assigned.pvalueonly == 'n' || index == -1)
                        {
                            printnum(b->assigned._value);
                            printf(" ");
                        }
                        printValue(b->assigned, index);
                        printf("\n");
                    }
                    break;

                    case x16:
                    {
                        printf("x16 %s=", b->assigned.name);
                        int index = getValueIndex(b->assigned);
                        if(b->assigned.pvalueonly == 'n' || index == -1)
                        {
                            printhexnum(b->assigned._value);
                            printf(" ");
                        }
                        printValue(b->assigned, index);
                        printf("\n");
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
                        int index = getValueIndex(b->assigned);
                        if(b->assigned.pvalueonly == 'n' || index == -1)
                        {
                            printnum(b->assigned._value);
                            printf(" ");
                        }
                        printValue(b->assigned, index);
                        printf("\n");
                    }
                    break;

                    case f32:
                    {
                        printf("f32 %s=%f\n", b->assigned.name, *(float*)&b->assigned._value);
                    }
                    break;

                    case c32:
                    {
                        printf("c32 %s=0x%X ", b->assigned.name, (unsigned int)b->assigned._value);
                        int index = getValueIndex(b->assigned);
                        if(b->assigned.pvalueonly == 'n' || index == -1)
                        {
                            printf("'");
                            printchar(b->assigned._value);
                            printf("'");
                            printf(" ");
                        }
                        printValue(b->assigned, index);
                        printf("\n");
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
                        int index = getValueIndex(b->assigned);
                        if(b->assigned.pvalueonly == 'n' || index == -1)
                        {
                            printnum(b->assigned._value);
                            printf(" ");
                        }
                        printValue(b->assigned, index);
                        printf("\n");
                    }
                    break;

                    case x32:
                    {
                        printf("x32 %s=", b->assigned.name);
                        int index = getValueIndex(b->assigned);
                        if(b->assigned.pvalueonly == 'n' || index == -1)
                        {
                            printhexnum(b->assigned._value);
                            printf(" ");
                        }
                        printValue(b->assigned, index);
                        printf("\n");
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
                        int index = getValueIndex(b->assigned);
                        if(b->assigned.pvalueonly == 'n' || index == -1)
                        {
                            printnum(b->assigned._value);
                            printf(" ");
                        }
                        printValue(b->assigned, index);
                        printf("\n");
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
                        int index = getValueIndex(b->assigned);
                        if(b->assigned.pvalueonly == 'n' || index == -1)
                        {
                            printnum(b->assigned._value);
                            printf(" ");
                        }
                        printValue(b->assigned, index);
                        printf("\n");
                    }
                    break;

                    case x64:
                    {
                        printf("x64 %s=", b->assigned.name);
                        int index = getValueIndex(b->assigned);
                        if(b->assigned.pvalueonly == 'n' || index == -1)
                        {
                            printhexnum(b->assigned._value);
                            printf(" ");
                        }
                        printValue(b->assigned, index);
                        printf("\n");
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
                        int index = getValueIndex(b->assigned);
                        if(b->assigned.pvalueonly == 'n' || index == -1)
                        {
                            printnum(b->assigned._value);
                            printf(" ");
                        }
                        printValue(b->assigned, index);
                        printf("\n");
                    }
                    break;

                    case u128:
                    {
                        printf("u128 %s=", b->assigned.name);
                        int index = getValueIndex(b->assigned);
                        if(b->assigned.pvalueonly == 'n' || index == -1)
                        {
                            printnum(b->assigned._value);
                            printf(" ");
                        }
                        printValue(b->assigned, index);
                        printf("\n");
                    }
                    break;

                    case x128:
                    {
                        printf("x128 %s=", b->assigned.name);
                        int index = getValueIndex(b->assigned);
                        if(b->assigned.pvalueonly == 'n' || index == -1)
                        {
                            printhexnum(b->assigned._value);
                            printf(" ");
                        }
                        printValue(b->assigned, index);
                        printf("\n");
                    }
                    break;

                    default:
                    error(NULL, 0, "Invalid type (bug)");
                }
            }
        }
    }
}

//belongs to main
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
