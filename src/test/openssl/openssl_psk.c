#include "openssl_psk.h"


const unsigned char g_tls13_aes128gcmsha256_id[] = { 0x13, 0x01 };
const unsigned char g_tls13_aes256gcmsha384_id[] = { 0x13, 0x02 };

int tls13_psk_use_session_cb(SSL *s, const EVP_MD *md,
                              const unsigned char **id, size_t *idlen,
                              SSL_SESSION **sess)
{
    TC_CONF *conf = SSL_get_ex_data(s, SSL_EX_DATA_TC_CONF);
    SSL_SESSION *usesess = NULL;
    const SSL_CIPHER *cipher = NULL;
    long key_len;
    unsigned char *key;
    
    DBG("Called PSK use sess cb\n");
    key = OPENSSL_hexstr2buf(conf->res.psk_key, &key_len);
    if (key == NULL) {
        ERR("hexstr2buf failed\n");
        return 0;
    }

    /* We default to SHA-256 */
    cipher = SSL_CIPHER_find(s, g_tls13_aes256gcmsha384_id);
    if (cipher == NULL) {
        ERR("Cipher fine failed\n");
        OPENSSL_free(key);
        return 0;
    }

    if (md != NULL && SSL_CIPHER_get_handshake_digest(cipher) != md) {
        /* PSK not usable, ignore it */
        return 1;
    }

    usesess = SSL_SESSION_new();
    if (usesess == NULL
            || !SSL_SESSION_set1_master_key(usesess, key, key_len)
            || !SSL_SESSION_set_cipher(usesess, cipher)
            || !SSL_SESSION_set_protocol_version(usesess, TLS1_3_VERSION)) {
        OPENSSL_free(key);
        goto err;
    }
    OPENSSL_free(key);

    if ((conf->res.early_data)
            && (SSL_SESSION_set_max_early_data(usesess, 4098) != 1)) {
        ERR("Use sess cb: Enabled early data\n");
        goto err;
    }

    *sess = usesess;
    *id = (unsigned char *)conf->res.psk_id;
    *idlen = strlen(conf->res.psk_id);

    return 1;

 err:
    SSL_SESSION_free(usesess);
    return 0;
}

int tls13_psk_find_session_cb(SSL *ssl, const unsigned char *id,
                               size_t id_len, SSL_SESSION **sess)
{
    TC_CONF *conf = SSL_get_ex_data(ssl, SSL_EX_DATA_TC_CONF);
    SSL_SESSION *tmpsess = NULL;
    unsigned char *key;
    long key_len;
    const SSL_CIPHER *cipher = NULL;

    DBG("Called PSK find sess cb\n");
    if ((id_len != strlen(conf->res.psk_id))
            || (memcmp(id, conf->res.psk_id, id_len) != 0)) {
        *sess = NULL;
        return 1;
    }

    key = OPENSSL_hexstr2buf(conf->res.psk_key, &key_len);
    if (key == NULL) {
        ERR("hexstr2buf conversion failed\n");
        return 0;
    }

    /* We default to SHA256 */
    cipher = SSL_CIPHER_find(ssl, g_tls13_aes256gcmsha384_id);
    if (cipher == NULL) {
        ERR("Find cipher failed\n");
        OPENSSL_free(key);
        return 0;
    }

    tmpsess = SSL_SESSION_new();
    if (tmpsess == NULL
            || !SSL_SESSION_set1_master_key(tmpsess, key, key_len)
            || !SSL_SESSION_set_cipher(tmpsess, cipher)
            || !SSL_SESSION_set_protocol_version(tmpsess, SSL_version(ssl))) {
        OPENSSL_free(key);
        return 0;
    }
    OPENSSL_free(key);
    *sess = tmpsess;

    return 1;
}

unsigned int tls_psk_client_cb(SSL *ssl, const char *hint,
                                       char *identity,
                                       unsigned int max_identity_len,
                                       unsigned char *psk,
                                       unsigned int max_psk_len)
{
    DBG("Called PSK client cb\n");
    TC_CONF *conf = SSL_get_ex_data(ssl, SSL_EX_DATA_TC_CONF);
    if ((strlen(conf->res.psk_id) + 1 > max_identity_len)
            || (strlen(conf->res.psk_key) > max_psk_len)) {
        ERR("PSK ID or Key buffer is not sufficient\n");
        goto err;
    }
    strcpy(identity, conf->res.psk_id);
    memcpy(psk, conf->res.psk_key, strlen(conf->res.psk_key));
    return strlen(conf->res.psk_key);
err:
    return 0;
}

unsigned int tls_psk_server_cb(SSL *ssl, const char *id,
                                            unsigned char *psk,
                                            unsigned int max_psk_len)
{
    DBG("Called PSK server cb\n");
    TC_CONF *conf = SSL_get_ex_data(ssl, SSL_EX_DATA_TC_CONF);
    if (strcmp(conf->res.psk_id, id) != 0) {
        ERR("Unknown Client's PSK ID\n");
        goto err;
    }
    if (strlen(conf->res.psk_key) > max_psk_len) {
        ERR("Insufficient buffer size to copy conf->res.psk_key\n");
        goto err;
    }
    memcpy(psk, conf->res.psk_key, strlen(conf->res.psk_key));
    return strlen(conf->res.psk_key);
err:
    return 0;
}

int ssl_ctx_psk_cb_config(TC_CONF *conf, SSL_CTX *ctx)
{
    if (conf->server == 1) {
        SSL_CTX_set_psk_server_callback(ctx, tls_psk_server_cb);
    } else {
        SSL_CTX_set_psk_client_callback(ctx, tls_psk_client_cb);
    }
    return TWT_SUCCESS;
}

int ssl_ctx_psk_sess_cb_config(TC_CONF *conf, SSL_CTX *ctx)
{
    if (conf->server == 1) {
        SSL_CTX_set_psk_find_session_callback(ctx, tls13_psk_find_session_cb);
        DBG("Registered TLS1.3 PSK find sess cb\n");
    } else {
        SSL_CTX_set_psk_use_session_callback(ctx, tls13_psk_use_session_cb);
        DBG("Registered TLS1.3 PSK use sess cb\n");
    }
    return TWT_SUCCESS;
}

int ssl_ctx_psk_config(TC_CONF *conf, SSL_CTX *ctx)
{
    switch (conf->res.psk) {
        case PSK_ID_AND_KEY:
            return ssl_ctx_psk_cb_config(conf, ctx);
        case PSK_ID_KEY_AND_CIPHERSUITE:
            return ssl_ctx_psk_sess_cb_config(conf, ctx);
        default:
            return TWT_FAILURE;
    }
}
