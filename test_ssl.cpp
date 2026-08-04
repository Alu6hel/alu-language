#include <openssl/evp.h>
int main() {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_MD_CTX_free(ctx);
    return 0;
}
