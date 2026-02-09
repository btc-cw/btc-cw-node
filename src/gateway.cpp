#include "gateway.hpp"

#include <cstdio>
#include <cstring>

#include <curl/curl.h>

namespace btccw::node {

// ---------------------------------------------------------------------------
// curl write callback
// ---------------------------------------------------------------------------
static std::size_t write_cb(char* ptr, std::size_t size, std::size_t nmemb,
                            void* userdata) {
    auto* resp = static_cast<std::string*>(userdata);
    resp->append(ptr, size * nmemb);
    return size * nmemb;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

Gateway::Gateway() = default;

Gateway::~Gateway() { close(); }

bool Gateway::open(const GatewayConfig& cfg) {
    cfg_ = cfg;

    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        std::fprintf(stderr, "[gateway] curl_global_init failed\n");
        return false;
    }
    initialized_ = true;
    return true;
}

void Gateway::close() {
    if (initialized_) {
        curl_global_cleanup();
        initialized_ = false;
    }
}

// ---------------------------------------------------------------------------
// Broadcast dispatcher
// ---------------------------------------------------------------------------

std::string Gateway::broadcast(std::string_view raw_tx_hex) {
    if (!initialized_) return {};

    switch (cfg_.backend) {
        case BroadcastBackend::MempoolSpace:
            return broadcast_mempool(raw_tx_hex);
        case BroadcastBackend::BitcoinRPC:
            return broadcast_rpc(raw_tx_hex);
    }
    return {};
}

// ---------------------------------------------------------------------------
// mempool.space  (POST raw hex as text/plain)
// ---------------------------------------------------------------------------

std::string Gateway::broadcast_mempool(std::string_view raw_tx_hex) {
    CURL* curl = curl_easy_init();
    if (!curl) return {};

    std::string response;
    std::string body(raw_tx_hex);

    curl_easy_setopt(curl, CURLOPT_URL, cfg_.mempool_url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: text/plain");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || http_code != 200) {
        std::fprintf(stderr, "[gateway] mempool broadcast failed (HTTP %ld): %s\n",
                     http_code, response.c_str());
        return {};
    }

    return response; // txid
}

// ---------------------------------------------------------------------------
// Bitcoin Core JSON-RPC  (sendrawtransaction)
// ---------------------------------------------------------------------------

std::string Gateway::broadcast_rpc(std::string_view raw_tx_hex) {
    CURL* curl = curl_easy_init();
    if (!curl) return {};

    std::string response;

    // Build JSON-RPC payload.
    std::string payload =
        "{\"jsonrpc\":\"1.0\",\"id\":\"btccw\","
        "\"method\":\"sendrawtransaction\",\"params\":[\"";
    payload.append(raw_tx_hex);
    payload += "\"]}";

    std::string url = "http://" + cfg_.rpc_host + ":" +
                      std::to_string(cfg_.rpc_port);

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    std::string userpwd = cfg_.rpc_user + ":" + cfg_.rpc_pass;
    curl_easy_setopt(curl, CURLOPT_USERPWD, userpwd.c_str());

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        std::fprintf(stderr, "[gateway] RPC broadcast failed: %s\n",
                     curl_easy_strerror(res));
        return {};
    }

    // Minimal parse: look for "result":"<txid>"
    auto pos = response.find("\"result\":\"");
    if (pos == std::string::npos) {
        std::fprintf(stderr, "[gateway] RPC error: %s\n", response.c_str());
        return {};
    }
    pos += 10; // skip past "result":"
    auto end = response.find('"', pos);
    return response.substr(pos, end - pos);
}

} // namespace btccw::node
