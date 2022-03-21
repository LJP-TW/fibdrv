#ifndef _SSO_H
#define _SSO_H

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/types.h>

#define SSO_CAPACITY 0x17

typedef union {
    char data[0x18];

    struct {
        uint8_t filler[SSO_CAPACITY];

        union {
            uint8_t flags;

            struct {
                uint8_t is_ptr : 1, reserved : 2,
                    left : 5;  // SSO_CAPACITY - size
            };
        };
    };

    struct {
        char *ptr;
        size_t size;
        size_t capacity;
    };
} sso_s;

void sso_release(sso_s *sso);

int sso_extend_capacity(sso_s *sso);
void sso_assign(sso_s *to, sso_s *from);

void sso_clear_flag(sso_s *sso);

void sso_set_capacity(sso_s *sso, size_t capacity);
size_t sso_get_capacity(sso_s *sso);

static inline void sso_set_size(sso_s *sso, size_t size)
{
    if (sso->is_ptr) {
        sso->size = size;
    } else {
        // TODO: what if SSO_CAPACITY < size ?
        sso->left = SSO_CAPACITY - size;
    }
}

static inline size_t sso_get_size(sso_s *sso)
{
    if (sso->is_ptr) {
        return sso->size;
    } else {
        return SSO_CAPACITY - sso->left;
    }
}

static inline char *sso_get_data(sso_s *sso)
{
    if (sso->is_ptr) {
        return sso->ptr;
    } else {
        return sso->filler;
    }
}

#endif /* _SSO_H */