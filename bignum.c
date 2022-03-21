#include "bignum.h"

#define bn_msg(fmt, ...) printk(KERN_ALERT fmt, ##__VA_ARGS__)

static void bn_reverse(bignum *bn)
{
    char *str = sso_get_data((sso_s *) bn);
    int end_idx = bn_size(bn) - 1;  // skip NULL byte
    int begin_idx = 0;

    while (begin_idx < end_idx) {
        char tmp;

        tmp = str[begin_idx];
        str[begin_idx] = str[end_idx];
        str[end_idx] = tmp;

        begin_idx++;
        end_idx--;
    }
}

static void bn_mode_to_cal(bignum *bn)
{
    if (bn->mode == BN_MOD_OUTPUTABLE) {
        bn_reverse(bn);
        bn->mode = BN_MOD_CALCULATABLE;
    }
}

static void bn_mode_to_out(bignum *bn)
{
    if (bn->mode == BN_MOD_CALCULATABLE) {
        bn_reverse(bn);
        bn->mode = BN_MOD_OUTPUTABLE;
    }
}

static size_t bn_extend_capacity(bignum *bn,
                                 int i,
                                 size_t *cap_left,
                                 char **str_result)
{
    sso_set_size((sso_s *) bn, i);

    *cap_left = sso_extend_capacity((sso_s *) bn) - i;
    if (!(*cap_left)) {
        bn_msg("[bignum] sso_extend_capacity failed");
        bn_release(bn);
        bn_init(bn, 0);
        return 0;
    }

    *str_result = sso_get_data((sso_s *) bn);

    return *cap_left;
}

static size_t bn_extend_capacity2(bignum *bn, size_t *cap, char **str_result)
{
    sso_set_size((sso_s *) bn, *cap);

    *cap = sso_extend_capacity((sso_s *) bn);
    if (!(*cap)) {
        bn_msg("[bignum] sso_extend_capacity failed");
        bn_release(bn);
        bn_init(bn, 0);
        return 0;
    }

    *str_result = sso_get_data((sso_s *) bn);

    return *cap;
}

static int bn_cmp(bignum *op1, bignum *op2)
{
    char *str_op1, *str_op2;
    int64_t sz_op1, sz_op2;

    bn_mode_to_cal(op1);
    bn_mode_to_cal(op2);

    str_op1 = sso_get_data((sso_s *) op1);
    str_op2 = sso_get_data((sso_s *) op2);
    sz_op1 = bn_size(op1);
    sz_op2 = bn_size(op2);

    if (sz_op1 > sz_op2) {
        return 1;
    }

    if (sz_op1 < sz_op2) {
        return -1;
    }

    while (--sz_op1 >= 0) {
        if (str_op1[sz_op1] > str_op2[sz_op1]) {
            return 1;
        }

        if (str_op1[sz_op1] < str_op2[sz_op1]) {
            return -1;
        }
    }

    return 0;
}

void bn_init(bignum *bn, int num)
{
    int size = 0;

    do {
        bn->sso.filler[size++] = num % 10 + '0';
        num /= 10;
    } while (num);

    bn->sso.filler[size] = 0;

    bn->mode = BN_MOD_CALCULATABLE;

    sso_clear_flag((sso_s *) bn);
    sso_set_size((sso_s *) bn, size);
}

void bn_release(bignum *bn)
{
    sso_release((sso_s *) bn);
}

size_t bn_size(bignum *bn)
{
    return sso_get_size((sso_s *) bn);
}

char *bn_str(bignum *bn)
{
    bn_mode_to_out(bn);

    return sso_get_data((sso_s *) bn);
}

void bn_mul(bignum *op1, bignum *op2, bignum *result)
{
    bignum tmp;
    char *str_op1, *str_op2, *str_result;
    size_t sz_op1, sz_op2, cap, i, j, inited;

    bn_mode_to_cal(op1);
    bn_mode_to_cal(op2);
    bn_mode_to_cal(result);

    bn_init(&tmp, 0);

    str_op1 = sso_get_data((sso_s *) op1);
    str_op2 = sso_get_data((sso_s *) op2);
    str_result = sso_get_data((sso_s *) &tmp);
    sz_op1 = bn_size(op1);
    sz_op2 = bn_size(op2);
    cap = sso_get_capacity((sso_s *) &tmp);

    inited = 0;

    for (i = 0; i < sz_op2; ++i) {
        int num2;

        num2 = str_op2[i] - '0';

        for (j = 0; j < sz_op1; ++j) {
            int num;

            num = (str_op1[j] - '0') * num2;

            if (i + j >= cap) {
                if (!bn_extend_capacity2(&tmp, &cap, &str_result)) {
                    return;
                }
            }

            if (i + j == 0 || i + j > inited) {
                inited = i + j;
            } else {
                num += str_result[i + j];
            }

            str_result[i + j] = num % 10;

            if (i + j + 1 >= cap) {
                if (!bn_extend_capacity2(&tmp, &cap, &str_result)) {
                    return;
                }
            }

            if (i + j + 1 > inited) {
                str_result[i + j + 1] = num / 10;
                inited = i + j + 1;
            } else {
                str_result[i + j + 1] += num / 10;
            }
        }
    }

    for (i = 0; i <= inited; ++i) {
        str_result[i] += '0';
    }

    if (str_result[inited] != '0') {
        inited += 1;
    }

    str_result[inited] = 0;

    sso_set_size((sso_s *) &tmp, inited);

    bn_assign(result, &tmp);

    bn_release(&tmp);
}

void bn_add(bignum *op1, bignum *op2, bignum *result)
{
    char *str_op1, *str_op2, *str_result;
    size_t sz_op1, sz_op2, cap_left, i;
    int carry;

    bn_mode_to_cal(op1);
    bn_mode_to_cal(op2);
    bn_mode_to_cal(result);

    if (bn_size(op1) < bn_size(op2)) {
        // Swap pointers
        bignum *tmp = op1;
        op1 = op2;
        op2 = tmp;
    }

    str_op1 = sso_get_data((sso_s *) op1);
    str_op2 = sso_get_data((sso_s *) op2);
    str_result = sso_get_data((sso_s *) result);
    sz_op1 = bn_size(op1);
    sz_op2 = bn_size(op2);
    cap_left = sso_get_capacity((sso_s *) result);

    carry = 0;

    for (i = 0; i < sz_op2; ++i) {
        int num;

        if (!cap_left) {
            if (!bn_extend_capacity(result, i, &cap_left, &str_result)) {
                return;
            }
        }

        num = str_op1[i] - '0' + str_op2[i] - '0' + carry;
        str_result[i] = num % 10 + '0';
        carry = num / 10;
        cap_left -= 1;
    }

    for (; i < sz_op1; ++i) {
        int num;

        if (!cap_left) {
            if (!bn_extend_capacity(result, i, &cap_left, &str_result)) {
                return;
            }
        }

        num = str_op1[i] - '0' + carry;
        str_result[i] = num % 10 + '0';
        carry = num / 10;
        cap_left -= 1;
    }

    if (carry) {
        if (cap_left == (size_t) 0) {
            if (!bn_extend_capacity(result, i, &cap_left, &str_result)) {
                return;
            }
        }

        str_result[i++] = '1';
        cap_left -= 1;
    }

    str_result[i] = 0;

    sso_set_size((sso_s *) result, i);
}

void bn_sub(bignum *op1, bignum *op2, bignum *result)
{
    char *str_op1, *str_op2, *str_result;
    size_t sz_op1, sz_op2, cap_left, i;
    int borrow;

    if (bn_cmp(op1, op2) < 0) {
        // TODO: Support negative big number
        return;
    }

    if (!bn_cmp(op1, op2)) {
        bn_release(result);
        bn_init(result, 0);
        return;
    }

    bn_mode_to_cal(op1);
    bn_mode_to_cal(op2);
    bn_mode_to_cal(result);

    str_op1 = sso_get_data((sso_s *) op1);
    str_op2 = sso_get_data((sso_s *) op2);
    str_result = sso_get_data((sso_s *) result);
    sz_op1 = bn_size(op1);
    sz_op2 = bn_size(op2);
    cap_left = sso_get_capacity((sso_s *) result);

    borrow = 0;

    for (i = 0; i < sz_op2; ++i) {
        int num;

        if (!cap_left) {
            if (!bn_extend_capacity(result, i, &cap_left, &str_result)) {
                return;
            }
        }

        num = str_op1[i] - str_op2[i] - borrow;

        if (num < 0) {
            num += 10;
            borrow = 1;
        } else {
            borrow = 0;
        }

        str_result[i] = num + '0';
        cap_left -= 1;
    }

    for (; i < sz_op1; ++i) {
        int num;

        if (!cap_left) {
            if (!bn_extend_capacity(result, i, &cap_left, &str_result)) {
                return;
            }
        }

        num = str_op1[i] - '0' - borrow;

        if (num < 0) {
            num += 10;
            borrow = 1;
        } else {
            borrow = 0;
        }

        str_result[i] = num + '0';
        cap_left -= 1;
    }


    if (str_result[i - 1] == '0') {
        i -= 1;
    }

    str_result[i] = 0;

    sso_set_size((sso_s *) result, i);
}

void bn_assign(bignum *to, bignum *from)
{
    bn_mode_to_cal(to);
    bn_mode_to_cal(from);

    sso_assign((sso_s *) to, (sso_s *) from);
}