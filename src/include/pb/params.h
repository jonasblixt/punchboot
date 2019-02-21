#ifndef __PB_PARAMS_H__
#define __PB_PARAMS_H__

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <pb/errors.h>

#define PB_PARAM_MAX_SIZE 64
#define PB_PARAM_MAX_IDENT_SIZE 20

enum
{
    PB_PARAM_END = 0,
    PB_PARAM_U08,
    PB_PARAM_U16,
    PB_PARAM_U32,
    PB_PARAM_U64,
    PB_PARAM_S08,
    PB_PARAM_S16,
    PB_PARAM_S32,
    PB_PARAM_S64,
    PB_PARAM_BOOL,
    PB_PARAM_UUID,
    PB_PARAM_TIMESTAMP,
    PB_PARAM_STR,
};

struct param
{
    uint32_t kind;
    char identifier[PB_PARAM_MAX_IDENT_SIZE];
    uint8_t reserved[40];
    uint8_t data[PB_PARAM_MAX_SIZE];
};

#define PARAM(__kind, __ident, __data) \
            ({ .kind = __kind, .identifier = __ident, .data = __data},)


#define foreach_param(__p,__params) \
        for (struct param *__p = __params; __p->kind != PB_PARAM_END; __p++)

static inline uint32_t param_get_by_id(struct param *params, 
                                       const char *id,
                                       struct param **out)
{
    *out = NULL;

    foreach_param(p, params)
    {
        if (strcmp(p->identifier,id) == 0)
        {
            (*out) = p;
            return PB_OK;
        }
    }
    
    return PB_ERR;
}

static inline uint32_t param_get_u32(struct param *p, uint32_t *v)
{
    if (p->kind != PB_PARAM_U32)
        return PB_ERR;
    uint32_t *ptr = (uint32_t *) p->data;
    (*v) = *ptr;
    return PB_OK;
}

#endif
