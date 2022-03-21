#ifndef _BIGNUM_H
#define _BIGNUM_H
// TODO: Support negative big number

#include "sso.h"

#define BN_MOD_CALCULATABLE 0
#define BN_MOD_OUTPUTABLE 1

typedef struct {
    sso_s sso;  // Must be first member of this struct
    /*
     * mode:
     *  0: Calculatable
     *  1: Outputable
     */
    int mode;
} bignum;

void bn_init(bignum *bn, int num);
void bn_release(bignum *bn);

size_t bn_size(bignum *bn);
char *bn_str(bignum *bn);

void bn_mul(bignum *op1, bignum *op2, bignum *result);
void bn_add(bignum *op1, bignum *op2, bignum *result);
void bn_sub(bignum *op1, bignum *op2, bignum *result);
void bn_assign(bignum *to, bignum *from);

#endif /* _BIGNUM_H */