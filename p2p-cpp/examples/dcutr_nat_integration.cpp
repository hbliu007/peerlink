#include "p2p/protocol/dcutr.hpp"
#include "p2p/nat/puncher.hpp"
#include <iostream>
#include <memory>

using namespace p2p::protocol;
using namespace p2p::nat;

/**
 * Example: DCUtR + NAT Traversal Integration
 *
 * This example demonstrates how to use DCUtR protocol with NAT traversal
 * to establish a direct P2P connection through a relay.
 */

void example_initiator() {
    std::cout << "=== Initiator Side ===" << std::endl;

    // Step 1: Create DCUtR client
    DCUtRClient dcutr_client;

    // Step 2: Prepare local addresses
    std::vector<Address> local_addrs = {
        {192, 168, 1, 100},  // Local address
        {203, 0, 113, 50}    // Public address (from STUN)
    };

    // Step 3: Initiate upgrade
    std::string peer_id = "peer-responder-123";
    auto session = dcutr_client.InitiateUpgrade(peer_id, local_addrs);

    std::cout << "Session state: "
              << static_cast<int>(session->GetState()) << std::endl;

    // Step 4: Send CONNECT message (via relay)
    ConnectMessage connect_msg = session->GetConnectMessage();
    std::cout << "Sending CONNECT with " << connect_msg.addrs.size()
              << " addresses" << std::endl;

    // Step 5: Receive SYNC message (via relay)
    // Simulated SYNC response
    SyncMessage sync_msg;
    sync_msg.addrs = {{198, 51, 100, 75}};  // Responder's address
    sync_msg.echo_timestamp_ns = connect_msg.timestamp_ns;
    sync_msg.timestamp_ns = connect_msg.timestamp_ns + 50000000LL;  // +50ms

    session->OnSyncReceived(sync_msg);

    // Step 6: Get punch schedule
    auto schedule_opt = session->GetPunchSchedule();
    if (!schedule_opt.has_value()) {
        std::cerr << "Failed to get punch schedule" << std::endl;
        return;
    }

    PunchSchedule schedule = schedule_opt.value();
    std::cout << "Punch schedule: RTT=" << schedule.rtt_ns / 1000000
              << "ms, targets=" << schedule.target_addrs.size() << std::endl;

    // Step 7: Execute coordinated punch
    NATTraversalCoordinator coordinator;

    // Simulate relay connection
    auto relay_conn = std::make_shared<Connection>("relay");

    coordinator.ExecuteWithRelayFallback(schedule, relay_conn,
        [](const PunchResult& result) {
            if (result.success) {
                std::cout << "Connection established via "
                          << result.transport_type << std::endl;
            } else {
                std::cout << "Connection failed: " << result.error << std::endl;
            }
        });

    std::cout << "=== Initiator Complete ===" << std::endl;
}

void example_responder() {
    std::cout << "\n=== Responder Side ===" << std::endl;

    // Step 1: Create DCUtR client
    DCUtRClient dcutr_client;

    // Step 2: Receive CONNECT message (via relay)
    ConnectMessage connect_msg;
    connect_msg.addrs = {{203, 0, 113, 50}};
    connect_msg.timestamp_ns = 1000000000LL;

    std::cout << "Received CONNECT with " << connect_msg.addrs.size()
              << " addresses" << std::endl;

    // Step 3: Respond to upgrade
    std::string peer_id = "peer-initiator-456";
    std::vector<Address> local_addrs = {{192, 168, 1, 100}};  // Responder's local addresses
    auto session = dcutr_client.RespondToUpgrade(peer_id, local_addrs, connect_msg);

    // Step 4: Send SYNC message (via relay)
    SyncMessage sync_msg = session->GetSyncMessage();
    std::cout << "Sending SYNC with " << sync_msg.addrs.size()
              << " addresses" << std::endl;

    // Step 5: Get punch schedule
    auto schedule_opt = session->GetPunchSchedule();
    if (!schedule_opt.has_value()) {
        std::cerr << "Failed to get punch schedule" << std::endl;
        return;
    }

    PunchSchedule schedule = schedule_opt.value();
    std::cout << "Punch schedule: RTT=" << schedule.rtt_ns / 1000000
              << "ms, targets=" << schedule.target_addrs.size() << std::endl;

    // Step 6: Execute coordinated punch
    NATTraversalCoordinator coordinator;

    auto relay_conn = std::make_shared<Connection>("relay");

    coordinator.ExecuteWithRelayFallback(schedule, relay_conn,
        [](const PunchResult& result) {
            if (result.success) {
                std::cout << "Connection established via "
                          << result.transport_type << std::endl;
            } else {
                std::cout << "Connection failed: " << result.error << std::endl;
            }
        });

    std::cout << "=== Responder Complete ===" << std::endl;
}

int main() {
    std::cout << "DCUtR + NAT Traversal Integration Example\n"
              << "=========================================\n" << std::endl;

    // Run initiator example
    example_initiator();

    // Run responder example
    example_responder();

    std::cout << "\nExample complete!" << std::endl;
    return 0;
}
