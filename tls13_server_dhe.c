#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

#include "openssl/crypto.h"
#include "openssl/ssl.h"

#include "test_common.h"

#define SERVER_CERT_FILE "./certs/ECC_Prime256_Certs/serv_cert.pem"
#define SERVER_KEY_FILE "./certs/ECC_Prime256_Certs/serv_key.der"

int g_kexch_groups[] = {
    NID_ffdhe2048,
    NID_ffdhe3072,
    NID_ffdhe4096,
    NID_ffdhe6144,
    NID_ffdhe8192
};

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 7788

int do_tcp_accept(const char *server_ip, uint16_t port)
{
    struct sockaddr_in addr;
    struct sockaddr_in peeraddr;
    socklen_t peerlen = sizeof(peeraddr);
    int lfd, cfd;
    int ret;
    int optval = 1;

    lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lfd < 0) {
        printf("Socket creation failed\n");
        return -1;
    }

    ret = setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &optval, (socklen_t)sizeof(optval));
    if (ret) {
        printf("setsockopt SO_RESUSEADDR failed\n");
        goto err_handler;
    }

    addr.sin_family = AF_INET;
    if (inet_aton(server_ip, &addr.sin_addr) == 0) {
        printf("inet_aton failed\n");
        goto err_handler;
    }
    addr.sin_port = htons(port);

    ret = bind(lfd, (struct sockaddr *)&addr, sizeof(addr));
    if (ret) {
        printf("bind failed\n");
        goto err_handler;
    }

    ret = listen(lfd, 5);
    if (ret) {
        printf("listen failed\n");
        goto err_handler;
    }

    printf("Waiting for TCP connection from client...\n");
    cfd = accept(lfd, (struct sockaddr *)&peeraddr, &peerlen);
    if (cfd < 0) {
        printf("accept failed, errno=%d\n", errno);
        goto err_handler;
    }

    printf("TCP connection accepted fd=%d\n", cfd);
    close(lfd);
    return cfd;
err_handler:
    close(lfd);
    return -1;
}

SSL_CTX *create_context()
{
    SSL_CTX *ctx;

    ctx = SSL_CTX_new(TLS_server_method());
    if (!ctx) {
        printf("SSL ctx new failed\n");
        return NULL;
    }

    printf("SSL context created\n");

    if (SSL_CTX_use_certificate_file(ctx, SERVER_CERT_FILE, SSL_FILETYPE_PEM) != 1) {
        printf("Load Server cert %s failed\n", SERVER_CERT_FILE);
        goto err_handler;
    }

    printf("Loaded server cert %s on context\n", SERVER_CERT_FILE);

    if (SSL_CTX_use_PrivateKey_file(ctx, SERVER_KEY_FILE, SSL_FILETYPE_ASN1) != 1) {
        printf("Load Server key %s failed\n", SERVER_KEY_FILE);
        goto err_handler;
    }

    printf("Loaded server key %s on context\n", SERVER_KEY_FILE);

    printf("SSL context configurations completed\n");

    return ctx;
err_handler:
    SSL_CTX_free(ctx);
    return NULL;
}

SSL *create_ssl_object(SSL_CTX *ctx)
{
    SSL *ssl;
    int fd;

    fd = do_tcp_accept(SERVER_IP, SERVER_PORT);
    if (fd < 0) {
        printf("TCP connection establishment failed\n");
        return NULL;
    }

    ssl = SSL_new(ctx);
    if (!ssl) {
        printf("SSL object creation failed\n");
        return NULL;
    }

    SSL_set_fd(ssl, fd);

    if (SSL_set1_groups(ssl, g_kexch_groups, sizeof(g_kexch_groups)/sizeof(g_kexch_groups[0])) != 1) {
        printf("Set Groups failed");
        goto err_handler;
    }

    printf("SSL object creation finished\n");

    return ssl;
err_handler:
    SSL_free(ssl);
    return NULL;
}

int do_data_transfer(SSL *ssl)
{
    char buf[MAX_BUF_SIZE] = {0};
    int ret;
    ret = SSL_read(ssl, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        printf("SSL_read failed ret=%d\n", ret);
        return -1;
    }
    printf("SSL_read[%d] %s\n", ret, buf);

    ret = SSL_write(ssl, MSG_FOR_CLNT, sizeof(MSG_FOR_CLNT));
    if (ret <= 0) {
        printf("SSL_write failed ret=%d\n", ret);
        return -1;
    }
    printf("SSL_write[%d] sent %s\n", ret, MSG_FOR_CLNT);
    return 0;
}

int tls13_server()
{
    SSL_CTX *ctx;
    SSL *ssl = NULL;
    int fd;
    int ret;

    ctx = create_context();
    if (!ctx) {
        return -1;
    }

    ssl = create_ssl_object(ctx);
    if (!ssl) {
        goto err_handler;
    }

    fd = SSL_get_fd(ssl);

    ret = SSL_accept(ssl); 
    if (ret != 1) {
        printf("SSL accept failed%d\n", ret);
        goto err_handler;
    }

    printf("SSL accept succeeded\n");

    if (do_data_transfer(ssl)) {
        printf("Data transfer over TLS failed\n");
        goto err_handler;
    }
    printf("Data transfer over TLS succeeded\n");
    SSL_free(ssl);
    SSL_CTX_free(ctx);
    close(fd);

    return 0;
err_handler:
    if (ssl) {
        SSL_free(ssl);
    }
    SSL_CTX_free(ctx);
    close(fd);
    return -1;
}

int main()
{
    printf("OpenSSL version: %s, %s\n", OpenSSL_version(OPENSSL_VERSION), OpenSSL_version(OPENSSL_BUILT_ON));
    if (tls13_server()) {
        printf("TLS12 server connection failed\n");
    }
}

