#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "openssl/crypto.h"
#include "openssl/ssl.h"

#include "test_common.h"
#include "test_openssl_common.h"
#include "test_openssl_arg.h"

int g_dhe_kexch_groups[] = {
    NID_ffdhe2048,
    NID_ffdhe3072,
    NID_ffdhe4096,
    NID_ffdhe6144,
    NID_ffdhe8192
};

int g_ec_kexch_groups[] = {
    NID_X9_62_prime256v1,   /* secp256r1 */
    NID_secp384r1,          /* secp384r1 */
    NID_secp521r1,          /* secp521r1 */
    NID_X25519,             /* x25519 */
    NID_X448                /* x448 */
};

int main(int argc, char *argv[])
{
    TC_CONF conf;

    if (parse_arg(argc, argv, &conf)) {
        printf("Parsing arg failed\n");
        return -1;
    }

    return do_test_openssl(&conf);
}