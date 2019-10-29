/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_PB_PARAMS_H_
#define INCLUDE_PB_PARAMS_H_

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <pb/errors.h>

#define PB_PARAM_MAX_SIZE 64
#define PB_PARAM_MAX_IDENT_SIZE 20
#define PB_PARAM_MAX_COUNT 128

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
    char identifier[PB_PARAM_MAX_IDENT_SIZE+1];
    uint8_t reserved[40];
    uint8_t data[PB_PARAM_MAX_SIZE+1];
};

#define PARAM(__kind, __ident, __data) \
            ({ .kind = __kind, .identifier = __ident, .data = __data}, )


#define foreach_param(__p, __params) \
        for (struct param *__p = __params; __p->kind != PB_PARAM_END; __p++)

static inline uint32_t param_get_by_id(struct param *params,
                                       const char *id,
                                       struct param **out)
{
    *out = NULL;

    foreach_param(p, params)
    {
        if (strcmp(p->identifier, id) == 0)
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

static inline uint32_t param_get_u16(struct param *p, uint16_t *v)
{
    if (p->kind != PB_PARAM_U16)
        return PB_ERR;
    uint16_t *ptr = (uint16_t *) p->data;
    (*v) = *ptr;
    return PB_OK;
}


static inline uint32_t param_add_u32(struct param *p,
                                     const char *ident,
                                     uint32_t v)
{
    p->kind = PB_PARAM_U32;

    uint32_t *ptr = (uint32_t *) p->data;
    (*ptr) = v;

    memcpy(p->identifier, ident, PB_PARAM_MAX_IDENT_SIZE);

    return PB_OK;
}


static inline uint32_t param_add_str(struct param *p,
                                     const char *ident,
                                     char *str)
{
    uint32_t str_len = strlen(str);

    p->kind = PB_PARAM_STR;

    if (str_len > PB_PARAM_MAX_SIZE)
        str_len = PB_PARAM_MAX_SIZE;

    memcpy(p->identifier, ident, PB_PARAM_MAX_IDENT_SIZE);

    memcpy(p->data, str, str_len);
    p->data[str_len] = 0;

    return PB_OK;
}


static inline uint32_t param_add_uuid(struct param *p,
                                     const char *ident,
                                     char *uuid)
{
    p->kind = PB_PARAM_UUID;
    memcpy(p->identifier, ident, PB_PARAM_MAX_IDENT_SIZE);
    memcpy(p->data, uuid, 16);

    return PB_OK;
}


static inline uint32_t param_terminate(struct param *p)
{
    p->kind = PB_PARAM_END;
    return PB_OK;
}


#endif  // INCLUDE_PB_PARAMS_H_
