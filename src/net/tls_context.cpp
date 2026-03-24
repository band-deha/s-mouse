#include "tls_context.h"

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/pem.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <algorithm>
#include <random>
#include <sstream>
#include <iomanip>
#include <vector>

namespace smouse {

TlsContext::TlsContext() = default;

TlsContext::~TlsContext() {
    if (ctx_) {
        SSL_CTX_free(ctx_);
        ctx_ = nullptr;
    }
}

bool TlsContext::init_server() {
    ctx_ = SSL_CTX_new(TLS_server_method());
    if (!ctx_) return false;

    // Require TLS 1.3
    SSL_CTX_set_min_proto_version(ctx_, TLS1_3_VERSION);

    if (!generate_self_signed_cert()) {
        SSL_CTX_free(ctx_);
        ctx_ = nullptr;
        return false;
    }

    // Generate pairing PIN
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(100000, 999999);
    pairing_pin_ = std::to_string(dist(gen));

    return true;
}

bool TlsContext::init_client() {
    ctx_ = SSL_CTX_new(TLS_client_method());
    if (!ctx_) return false;

    // Require TLS 1.3
    SSL_CTX_set_min_proto_version(ctx_, TLS1_3_VERSION);

    // Don't verify server cert automatically (we do manual fingerprint verification)
    SSL_CTX_set_verify(ctx_, SSL_VERIFY_NONE, nullptr);

    return true;
}

std::string TlsContext::get_fingerprint() const {
    if (!ctx_) return "";

    SSL* ssl = SSL_new(ctx_);
    if (!ssl) return "";

    X509* cert = SSL_get_certificate(ssl);
    SSL_free(ssl);
    if (!cert) return "";

    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int md_len = 0;
    X509_digest(cert, EVP_sha256(), md, &md_len);

    std::ostringstream oss;
    for (unsigned int i = 0; i < md_len; i++) {
        if (i > 0) oss << ":";
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(md[i]);
    }

    return oss.str();
}

void TlsContext::add_trusted_peer(const std::string& fingerprint) {
    if (std::find(trusted_peers_.begin(), trusted_peers_.end(), fingerprint) == trusted_peers_.end()) {
        trusted_peers_.push_back(fingerprint);
    }
}

bool TlsContext::is_trusted_peer(const std::string& fingerprint) const {
    return std::find(trusted_peers_.begin(), trusted_peers_.end(), fingerprint) != trusted_peers_.end();
}

bool TlsContext::generate_self_signed_cert() {
    // Generate RSA key pair
    EVP_PKEY* pkey = EVP_PKEY_new();
    EVP_PKEY_CTX* pkey_ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
    EVP_PKEY_keygen_init(pkey_ctx);
    EVP_PKEY_CTX_set_rsa_keygen_bits(pkey_ctx, 2048);
    EVP_PKEY_keygen(pkey_ctx, &pkey);
    EVP_PKEY_CTX_free(pkey_ctx);

    if (!pkey) return false;

    // Create X509 certificate
    X509* x509 = X509_new();
    if (!x509) {
        EVP_PKEY_free(pkey);
        return false;
    }

    // Set serial number
    ASN1_INTEGER_set(X509_get_serialNumber(x509), 1);

    // Valid from now for 10 years
    X509_gmtime_adj(X509_get_notBefore(x509), 0);
    X509_gmtime_adj(X509_get_notAfter(x509), 10L * 365 * 24 * 3600);

    X509_set_pubkey(x509, pkey);

    // Set subject
    X509_NAME* name = X509_get_subject_name(x509);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
        reinterpret_cast<const unsigned char*>("s-mouse"), -1, -1, 0);
    X509_set_issuer_name(x509, name);

    // Self-sign
    X509_sign(x509, pkey, EVP_sha256());

    // Load into context
    SSL_CTX_use_certificate(ctx_, x509);
    SSL_CTX_use_PrivateKey(ctx_, pkey);

    X509_free(x509);
    EVP_PKEY_free(pkey);

    return SSL_CTX_check_private_key(ctx_) == 1;
}

} // namespace smouse
