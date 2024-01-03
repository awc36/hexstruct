#pragma once
#include "vector.h"

void _v_onerr(char* errtype, vec v, long pos)
{
    printf("Fehler bei %s\n", errtype);
}
void (*v_onerr)(char* errtype, vec v, long pos) = _v_onerr;

vector *_v_init(long type)
{
    vector *v = (vector*)malloc(sizeof(vector));
    v->cap = 10;
    v->len = 0;
    v->type = type;
    v->values = malloc(10 * type);
    return v;
}

//type
#define v_init(type) _v_init(sizeof(type))

void _v_add(vector *v, void *val, long pos)
{
    if(pos < 0 || pos > v->len)
    {
        v_onerr("v_add", v, pos);
        exit(1);
    }
    v->len++;
    if(v->len == v->cap)
    {
        v->cap *= 2;
        v->values = realloc(v->values, v->cap * v->type);
    }
    void *dest = v->values + (pos+1) * v->type;
    void *src = v->values + (pos) * v->type;
    memmove(dest, src, (v->len-pos-1) * v->type);
    memcpy(src, val, v->type);
}

//vector, value, position
#define v_add(v, val, pos) _v_add(v, (void*)&val, pos)

void v_addrange(vector *dest, vector *src, long pos)
{
    if(pos < 0 || pos > dest->len || src->type != dest->type)
    {
        v_onerr("v_addrange", dest, pos);
        exit(1);
    }
    dest->len += src->len;
    while(dest->len+src->len >= dest->cap)
    {
        dest->cap *= 2;
        dest->values = realloc(dest->values, dest->cap);
    }
    void *ddest = dest->values + (pos+src->len) * dest->type;
    void *dsrc = dest->values + (pos) * dest->type;
    memmove(ddest, dsrc, (dest->len-pos-src->len) * dest->type);
    memcpy(dsrc, src->values, dest->type * src->len);
}

void v_remove(vector *v, long pos)
{
    if(pos < 0 || pos >= v->len || v->len == 0)
    {
        v_onerr("v_remove", v, pos);
        exit(1);
    }

    v->len--;
    if(v->len * 4 == v->cap && v->cap > 5)
    {
        v->cap /= 2;
        v->values = realloc(v->values, v->cap * v->type);
    }
    void **dest = v->values + (pos) * v->type;
    void **src = v->values + (pos+1) * v->type;

    memmove(dest, src, (v->len-pos) * v->type);
}

vector* v_copy(vector *v)
{
    vector *out = malloc(sizeof(vector));
    out->values=malloc(v->cap*v->type);
    out->len=v->len;
    out->cap=v->cap;
    out->type=v->type;
    memcpy(out->values, v->values, v->type);
    return out;
}

void* _v_get(vector *v, long pos, long type)
{
    if(pos < 0 || pos >= v->len || type != v->type)
    {
        v_onerr("v_get", v, pos);
        exit(1);
    }
    return v->values + pos * v->type;
}

#define v_get(v, i, type)  (*(type*)_v_get(v, i, sizeof(type)))

#define v_push(v, val) _v_add(v, (void*)&val, (v)->len)
#define v_pushrange(dest, src) v_addrange(dest, src, 0)
#define v_pop(v) v_remove(v, v->len-1)

void v_free(vector *v)
{
    free(v->values);
    free(v);
}

void v_clear(vector *v)
{
    v->values = realloc(v->values, 10 * v->type);
    v->cap = 10;
    v->len = 0;
}