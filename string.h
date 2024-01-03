#pragma once
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
int printf(const char* format, ...);

typedef struct string
{
    int *values;
    long len;
    long cap;
} string;
#define str string*

string* s_init();
string* s_init_cap(long cap);
string* s_init_cstr(char *cstr);
#define s_init_const_cstr(out, cstr) out = alloca(sizeof(string));out->len = sizeof(U##cstr)/4-1;out->cap = sizeof(U##cstr)/4-1;out->values = (int*)U##cstr
string* s_init_string(string * src);
string* s_substring(string *src, long begin, long len);
char* s_substring_cstr(string *src, long begin, long len);
void s_insert(string *s, int val, long pos);
#define s_append(s, val) s_insert(s, val, s->len)
void s_add_at(string *s, string *val, long pos);
#define s_add(s, val) s_add_at(s, val, s->len)
void s_remch(string *s, long pos);
#define s_remendch(s) s_remch(s, s->len-1)
long s_indexof(string *s, string *pattern);
long s_indexof_cstr(string *s, char *pattern);
void s_replace(string *s, char *pattern, char *replacement);
void s_replace_range(string *s, long start, long end, str replacement);
char s_cmp(string *s1, string *s2);
char s_cmp_str(string *s1, char *s2);
void s_free(string *s);
int* s_clstr(string *src);
char* s_cstr(string *src);
void s_printf(string *out, const char *format, ...);

#include "string.c"