#pragma once
#include "string.h"
#include <stdio.h>
#include <string.h>


string* s_init()
{
    return s_init_cap(10);
}

string* s_init_cap(long cap)
{
    if(cap <= 0)
    {
        printf("string.h: invalid capacity(s_init_cap)\n");
        exit(1);
    }
    string *s = malloc(sizeof(string));
    s->cap = cap;
    s->len = 0;
    s->values = malloc(cap * sizeof(*s->values));
    return s;
}

string* s_init_cstr(char *cstr)
{
    long len = 0;
    for(; cstr[len] != 0; len++);
    string *s = (string*)malloc(sizeof(string));
    s->cap = len;
    s->len = len;
    s->values = malloc(s->cap * sizeof(*s->values));
    long i = 0, j = 0;
    for(; cstr[i] != 0; j++)
    {
        char first = cstr[i];
        int len = 0;
        for(; (first & 128) != 0; len++)
            first <<= 1;
        unsigned int ch;
        if(len == 0)
        {
            ch = first;
            i++;
        }
        else
        {
            first >>= len;
            ch = first;
            for(int k = 1; k < len; k++)
            {
                if((cstr[i+k] & 0xc0) != 0x80)
                {
                    free(s->values);
                    free(s);
                    return NULL;
                }
                ch <<= 6;
                ch += cstr[i+k] & 0x3f;
            }
            i += len;
        }
        s->values[j] = ch;
    }
    if(s->cap > j)
    {
        s->cap = j;
        s->len = j;
        s->values = realloc(s->values, s->cap * sizeof(*s->values));
    }
    return s;
}

string* s_init_string(string * src)
{
    string *s = (string*)malloc(sizeof(string));
    s->cap = 2*src->len;
    s->len = src->len;
    s->values = malloc(s->cap * sizeof(*s->values));
    memcpy(s->values, src->values, s->len * sizeof(*s->values));
    return s;
}

string* s_substring(string *src, long begin, long len)
{
    if(len == -1)
        len = src->len - begin;
    if(begin == -1)
        len = src->len - len;
    string *sub = malloc(sizeof(string));
    sub->cap = 0;
    sub->len = len;
    sub->values = &src->values[begin];
    return sub;
}

char* s_substring_cstr(string *src, long begin, long len)
{
    string *s = s_substring(src, begin, len);
    char *cstr = s_cstr(s);
    free(s);
    return cstr;
}

void s_insert(string *s, int val, long pos)
{
    if(s->cap <= 0)
    {
        printf("string.h: invalid capacity(s_insert)\n");
        exit(1);
    }
    s->len++;
    if(s->len == s->cap)
    {
        s->cap *= 2;
        s->values = realloc(s->values, s->cap * sizeof(*s->values));
    }
    int *dest = &((int*)s->values)[pos+1];
    int *src = &((int*)s->values)[pos];
    memmove(dest, src, (s->len-pos-1) * 4);
    *src = val;
}

void s_add_at(string *s, string *val, long pos)
{
    if(s->cap <= 0)
    {
        printf("string.h: invalid capacity(s_add_at)\n");
        exit(1);
    }
    long nlen = s->len + val->len;
    while(nlen >= s->cap)
    {
        s->cap = 2 * s->cap;
        s->values = realloc(s->values, s->cap * sizeof(*s->values));
    }
    int *dest = &s->values[pos+val->len];
    int *src = &s->values[pos];
    memmove(dest, src, (s->len-pos) * 4);
    memcpy(src, val->values, val->len * 4);
    s->len = nlen;
}

void s_remch(string *s, long pos)
{
    if(s->cap <= 0)
    {
        printf("string.h: invalid capacity(s_remch)\n");
        exit(1);
    }
    if(s->len != 0)
        s->len--;
    if(s->len * 4 == s->cap)
    {
        s->cap /= 2;
        s->values = realloc(s->values, s->cap * sizeof(*s->values));
    }
    int *dest = &s->values[pos];
    int *src = &s->values[pos+1];
    memmove(dest, src, (s->len-pos) * 4);
}

long s_indexof(string *s, string *pattern)
{
    for(long i = 0; i < s->len - pattern->len; i++)
    {
        string *sub = s_substring(s, i, pattern->len);
        if(s_cmp(sub, pattern) == 0)
        {
            free(sub);
            return i;
        }
        free(sub);
    }
    return -1;
}

long s_indexof_cstr(string *s, char *pattern)
{
    long len = strlen(pattern);
    for(long i = 0; i < s->len - len+1; i++)
    {
        string *sub = s_substring(s, i, len);
        if(s_cmp_str(sub, pattern) == 0)
        {
            free(sub);
            return i;
        }
        free(sub);
    }
    return -1;
}

void s_replace(string *s, char *pattern, char *replacement)
{
    if(s->cap <= 0)
    {
        printf("string.h: invalid capacity(s_replace)\n");
        exit(1);
    }
    string *p = s_init_cstr(pattern);
    string *r = s_init_cstr(replacement);
    long index = 0;

    for(long i = 0; i < s->len; i++)
    {
        if(s->values[i] == pattern[index])
        {
            if(++index == p->len)
            {
                char changed = 0;
                while(s->len + r->len - p->len > s->cap) {changed = 1; s->cap *= 2;}
                if(changed)
                    s->values = realloc(s->values, s->cap * sizeof(*s->values));
                i++;
                memmove(&s->values[i+r->len-p->len], &s->values[i], (s->len-i) * sizeof(*s->values));
                i -= p->len;
                memmove(&s->values[i], r->values, r->len * sizeof(*s->values));
                index = 0;
                s->len += r->len - p->len;
            }
        }
        else
            index = 0;
    }
    s_free(p);
    s_free(r);
}

//start inkl., end exkl.
void s_replace_range(string *s, long start, long end, str replacement)
{
    if(s->cap <= 0)
    {
        printf("string.h: invalid capacity(s_replace_range)\n");
        exit(1);
    }
    long index = 0;
    long len = end - start;

    char changed = 0;
    while(s->len + replacement->len - len > s->cap) {changed = 1; s->cap *= 2;}
    if(changed)
        s->values = realloc(s->values, s->cap * sizeof(*s->values));
    memmove(&s->values[start+replacement->len], &s->values[end], (s->len-end) * sizeof(*s->values));
    memmove(&s->values[start], replacement->values, replacement->len * sizeof(*s->values));
    index = 0;
    s->len += replacement->len - len;
}

char s_cmp(string *s1, string *s2)
{
    string *min = s1->len > s2->len ? s2 : s1;
    for(long i = 0; i < min->len; i++)
    {
        if(s1->values[i] > s2->values[i])
            return 2;
        else if(s1->values[i] < s2->values[i])
            return -2;
    }
    if(s1->len == s2->len)
        return 0;
    if(s1 == min)
        return -1;
    else
        return 1;
}

char s_cmp_str(string *s1, char *s2)
{
    long len = strlen(s2);
    long minlen = s1->len > len ? len : s1->len;
    void *min = s1->len > len ? (void*)s2 : (void*)s1;
    for(long i = 0; i < minlen; i++)
    {
        if(s1->values[i] > s2[i])
            return 2;
        else if(s1->values[i] < s2[i])
            return -2;
    }
    if(s1->len == len)
        return 0;
    if(s1 == min)
        return -1;
    else
        return 1;
}

void s_free(string *s)
{
    if(s->cap <= 0)
    {
        printf("string.h: invalid capacity(s_free)\n");
        exit(1);
    }
    free(s->values);
    free(s);
}

int* s_clstr(string *src)
{
    s_append(src, 0);
    src->len--;
    return src->values;
}

void _s_cstr_resize(char **code, unsigned long *len, unsigned long *cap, long add)
{
    if(*len + add >= *cap)
    {
        do
        {
            *cap *= 2;
        } while(*len + add >= *cap);
        *code = realloc(*code, *cap);
    }
}

char* s_cstr(string *src)
{
    if(src->len == 0)
    {
        char* text = malloc(1);
        text[0] = '\0';
        return text;
    }
    char *text = malloc(src->len);
    unsigned long len = 0;
    unsigned long cap = src->len;
    for(int i = 0; i < src->len; i++)
    {
        unsigned int letter = src->values[i];
        int size = 0;
        for(; letter != 0; size++){letter >>= 1;}
        letter = src->values[i];
        if(size <= 7)
        {
            text[len++] = letter;
        }
        else if(size <= 11)
        {
            _s_cstr_resize(&text, &len, &cap, 1);
            text[len++] = 0b11000000 | (letter >> 6);
            text[len++] = 0b10000000 | (0b00111111 & (letter >> 0));
        }
        else if(size <= 16)
        {
            _s_cstr_resize(&text, &len, &cap, 2);
            text[len++] = 0b11100000 | (letter >> 12);
            text[len++] = 0b10000000 | (0b00111111 & (letter >> 6));
            text[len++] = 0b10000000 | (0b00111111 & (letter >> 0));
        }
        else if(size <= 21)
        {
            _s_cstr_resize(&text, &len, &cap, 3);
            text[len++] = 0b11110000 | (letter >> 18);
            text[len++] = 0b10000000 | (0b00111111 & (letter >> 12));
            text[len++] = 0b10000000 | (0b00111111 & (letter >> 6));
            text[len++] = 0b10000000 | (0b00111111 & (letter >> 0));
        }
        else if(size <= 26)
        {
            _s_cstr_resize(&text, &len, &cap, 4);
            text[len++] = 0b11111000 | (letter >> 24);
            text[len++] = 0b10000000 | (0b00111111 & (letter >> 18));
            text[len++] = 0b10000000 | (0b00111111 & (letter >> 12));
            text[len++] = 0b10000000 | (0b00111111 & (letter >> 0));
        }
        else if(size <= 31)
        {
            _s_cstr_resize(&text, &len, &cap, 5);
            _s_cstr_resize(&text, &len, &cap, 4);
            text[len++] = 0b11111100 | (letter >> 30);
            text[len++] = 0b10000000 | (0b00111111 & (letter >> 24));
            text[len++] = 0b10000000 | (0b00111111 & (letter >> 18));
            text[len++] = 0b10000000 | (0b00111111 & (letter >> 12));
            text[len++] = 0b10000000 | (0b00111111 & (letter >> 0));
        }
    }
    _s_cstr_resize(&text, &len, &cap, 1);
    text[len] = 0;
    return realloc(text, len+1);
}

void s_printf(string *out, const char *format, ...)
{
    if(out->cap <= 0)
    {
        printf("string.h: invalid capacity(s_printf)\n");
        exit(1);
    }
    va_list args;
    va_start(args, format);
    string *sformat = s_init_cstr((char*)format);
    for(long i = 0; i < sformat->len; i++)
    {
        if(sformat->values[i] == '%')
        {
            i++;
            switch(sformat->values[i])
            {
                case 0://end of string
                {
                    s_append(out, '%');
                    return;
                }
                break;
                case 's':
                {
                    char *cstr = va_arg(args, char*);
                    string *s = s_init_cstr(cstr);
                    s_add(out, s);
                    s_free(s);
                }
                break;
                case 'c':
                {
                    int c = va_arg(args, int);
                    s_append(out, c);
                }
                break;
                case 'i':
                {
                    int i = va_arg(args, int);
                    if(i == 0)
                    {
                        s_append(out, '0');
                        break;
                    }
                    if(i < 0)
                    {
                        s_append(out, '-');
                        i = -i;
                    }
                    const int base10[] = {1000000000, 100000000, 10000000, 1000000, 100000, 10000, 1000, 100, 10, 1};
                    int pointer = 0;
                    while(base10[++pointer] > i);
                    for(; pointer < sizeof(base10) / sizeof(*base10); pointer++)
                    {
                        int num = i / base10[pointer];
                        s_append(out, num + 0x30);
                        i -= num * base10[pointer];
                    }
                }
                break;
                case 'x':
                {
                    int i = va_arg(args, int);
                    if(i == 0)
                    {
                        s_append(out, '0');
                        break;
                    }
                    if(i < 0)
                    {
                        s_append(out, '-');
                        i = -i;
                    }
                    const int base10[] = {0x10000000, 0x1000000, 0x100000, 0x10000, 0x1000, 0x100, 0x10, 0x1};
                    int pointer = 0;
                    while(base10[++pointer] > i);
                    for(; pointer < sizeof(base10) / sizeof(*base10); pointer++)
                    {
                        int num = i / base10[pointer];
                        i -= num * base10[pointer];
                        if(num > 9)
                            num += 0x27;
                        s_append(out, num + 0x30);
                    }
                }
                break;
                case 'f':
                {
                    #ifdef _GLIBCXX_MATH_H
                    double d = va_arg(args, double);
                    if(isnan(d))
                    {
                        s_printf(out, "nan(%x)", *(long*)&d);
                        break;
                    }
                    if(d < 0)
                    {
                        s_append(out, '-');
                        d = -d;
                    }
                    if(isinf(d))
                    {
                        s_printf(out, "inf", *(long*)&d);
                        break;
                    }
                    if(d == 0)
                    {
                        s_printf(out, "0.0");
                        break;
                    }
                    int exp = (int)floor(log10(d));
                    if(exp < 3 && exp > -3)
                    {
                        if(exp >= 2)
                        {
                            int num = d / 100;
                            s_append(out, num + 0x30);
                            d -= 100 * num;
                        }
                        if(exp >= 1)
                        {
                            int num = d / 10;
                            s_append(out, num + 0x30);
                            d -= 10 * num;
                        }
                        exp = 0;
                    }
                    else
                        d /= pow(10, exp);
                    double u = 1.0;

                    int num = d / u;
                    s_append(out, num + 0x30);
                    d -= u * num;
                    u /= 10;
                    s_append(out, ',');
                    for(int _ = 0; _ < 5; _++)
                    {
                        int num = d / u;
                        s_append(out, num + 0x30);
                        d -= u * num;
                        u /= 10;
                    }
                    if(exp != 0)
                    {
                        s_append(out, 'e');
                        s_printf(out, "%i", exp);
                    }
                    #else
                    fprintf(stderr, "sprintf: The %%f option  needs math.h to be included.");
                    #endif
                }
                break;
                case '%':
                {
                    s_append(out, '%');
                }
                default:
                {
                    s_append(out, '%');
                    s_append(out, sformat->values[i]);
                }
                break;
            }
        }
        else
        {
            s_append(out, sformat->values[i]);
        }
    }
}
