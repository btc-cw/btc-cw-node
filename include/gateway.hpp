#ifndef BTCCW_NODE_GATEWAY_HPP
#define BTCCW_NODE_GATEWAY_HPP

#include <string>
#include <string_view>

namespace btccw::node {

/// Supported broadcast backends.
enum class BroadcastBackend {
    MempoolSpace,    // POST to mempool.space/api/tx
    BitcoinRPC,      // JSON-RPC to a local Bitcoin Core node
};

/// Configuration for the network gateway.
struct GatewayConfig {
    BroadcastBackend backend = BroadcastBackend::MempoolSpace;

    // mempool.space settings
    std::string mempool_url = "https://mempool.space/api/tx";

    // Bitcoin Core RPC settings
    std::string rpc_host = "127.0.0.1";
    int         rpc_port = 8332;
    std::string rpc_user;
    std::string rpc_pass;
};

/// HTTP/RPC gateway for broadcasting raw transactions to the Bitcoin network.
class Gateway {
public:
    Gateway();
    ~Gateway();

    /// Initialise libcurl and store config.
    bool open(const GatewayConfig& cfg);

    /// Clean up.
    void close();

    /// Broadcast a raw hex transaction.
    /// Returns the txid on success, or an empty string on failure.
    std::string broadcast(std::string_view raw_tx_hex);

private:
    GatewayConfig cfg_;
    bool          initialized_ = false;

    std::string broadcast_mempool(std::string_view raw_tx_hex);
    std::string broadcast_rpc(std::string_view raw_tx_hex);
};

} // namespace btccw::node

#endif // BTCCW_NODE_GATEWAY_HPP
