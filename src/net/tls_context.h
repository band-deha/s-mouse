#pragma once

#include <memory>
#include <string>
#include <vector>

// Forward declare OpenSSL types to avoid header pollution
typedef struct ssl_ctx_st SSL_CTX;
typedef struct ssl_st SSL;

namespace smouse {

// Manages TLS context (certificates, keys)
class TlsContext {
public:
    TlsContext();
    ~TlsContext();

    // Non-copyable
    TlsContext(const TlsContext&) = delete;
    TlsContext& operator=(const TlsContext&) = delete;

    // Initialize as server (generates self-signed cert)
    bool init_server();

    // Initialize as client
    bool init_client();

    // Get the generated PIN for pairing (6 digits)
    std::string get_pairing_pin() const { return pairing_pin_; }

    // Get certificate fingerprint (SHA-256)
    std::string get_fingerprint() const;

    // Store a trusted peer fingerprint
    void add_trusted_peer(const std::string& fingerprint);

    // Check if a peer fingerprint is trusted
    bool is_trusted_peer(const std::string& fingerprint) const;

    // Get the underlying SSL_CTX (for wrapping sockets)
    SSL_CTX* ctx() const { return ctx_; }

private:
    SSL_CTX* ctx_ = nullptr;
    std::string pairing_pin_;
    std::vector<std::string> trusted_peers_;

    bool generate_self_signed_cert();
};

} // namespace smouse
