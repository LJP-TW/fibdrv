#include "sso.h"

#define sso_msg(fmt, ...) printk(KERN_ALERT fmt, ##__VA_ARGS__)

void sso_release(sso_s *sso)
{
    if (sso->is_ptr)
        kfree(sso->ptr);
}

void sso_clear_flag(sso_s *sso)
{
    sso->flags = 0;
}

void sso_set_capacity(sso_s *sso, size_t capacity)
{
    if (sso->is_ptr) {
        // Preserve flags setting
        sso->capacity = (sso->capacity & (0xff00000000000000)) | capacity;
    }
}

size_t sso_get_capacity(sso_s *sso)
{
    if (sso->is_ptr) {
        return sso->capacity & (0x00ffffffffffffff);  // Exclude flag field
    } else {
        return SSO_CAPACITY;
    }
}

int sso_extend_capacity(sso_s *sso)
{
    size_t cur_cap, new_cap, sz;
    char *new_data, *old_data;

    cur_cap = sso_get_capacity(sso);
    new_cap = ALIGN(cur_cap * 2, 0x10) - 1;

    new_data = kmalloc(new_cap + 1, GFP_KERNEL);
    if (!new_data) {
        sso_msg("[sso] Failed to allocate memory");
        return 0;
    }

    // Copy old data
    old_data = sso_get_data(sso);
    sz = sso_get_size(sso);

    memcpy(new_data, old_data, sz + 1);

    if (sso->is_ptr) {
        kfree(sso->ptr);
    }

    sso->ptr = new_data;
    sso->is_ptr = 1;

    sso_set_size(sso, sz);
    sso_set_capacity(sso, new_cap);

    return new_cap;
}

void sso_assign(sso_s *to, sso_s *from)
{
    if (from->is_ptr) {
        if (to->is_ptr) {
            if (sso_get_capacity(to) < sso_get_capacity(from)) {
                char *tmp;

                tmp = kmalloc(sso_get_capacity(from) + 1, GFP_KERNEL);
                if (!tmp) {
                    sso_msg("[sso] Failed to allocate memory.");
                    return;
                }

                kfree(to->ptr);
                to->ptr = tmp;
            }
        } else {
            char *tmp;

            tmp = kmalloc(sso_get_capacity(from) + 1, GFP_KERNEL);
            if (!tmp) {
                sso_msg("[sso] Failed to allocate memory.");
                return;
            }

            to->is_ptr = 1;
            to->ptr = tmp;
        }
    } else {
        if (to->is_ptr) {
            kfree(to->ptr);
        }

        to->is_ptr = 0;
    }

    memcpy(sso_get_data(to), sso_get_data(from), sso_get_size(from) + 1);
    sso_set_size(to, sso_get_size(from));
    sso_set_capacity(to, sso_get_capacity(from));
}