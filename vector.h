#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct vector
{
    void *values;
    long len;
    long cap;
    long type;
} vector;

#define vec vector*

void (*v_onerr)(char* errtype, vec v, long pos);

#include "vector.c"

//type
#define v_init(type) _v_init(sizeof(type))
#define v_add(v, val, pos) _v_add(v, (void*)&val, pos)
void v_addrange(vector *dest, vector *src, long pos);
void v_remove(vec v, long pos);
vec v_copy(vec v);
#define v_get(v, i, type)  (*(type*)_v_get(v, i, sizeof(type)))

#define v_push(v, val) _v_add(v, (void*)&val, (v)->len)
#define v_pushrange(dest, src) v_addrange(dest, src, 0)
#define v_pop(v) v_remove(v, v->len-1)
void v_free(vec v);
void v_clear(vec v);