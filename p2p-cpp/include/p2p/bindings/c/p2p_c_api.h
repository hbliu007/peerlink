#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

/**
 * @file p2p_c_api.h
 * @brief C API for P2P client library
 *
 * This C API provides a language-agnostic interface for the P2P client,
 * enabling bindings for Python, Java, Swift, JavaScript, and other languages.
 */

// Opaque handle types
typedef void* p2p_client_t;
typedef void* p2p_config_t;

// Error codes
typedef enum {
    P2P_OK = 0,
    P2P_ERROR_INVALID_PARAM = 1,
    P2P_ERROR_NOT_INITIALIZED = 2,
    P2P_ERROR_ALREADY_CONNECTED = 3,
    P2P_ERROR_CONNECTION_FAILED = 4,
    P2P_ERROR_TIMEOUT = 5,
    P2P_ERROR_NAT_DETECTION_FAILED = 6,
    P2P_ERROR_SEND_FAILED = 7,
    P2P_ERROR_UNKNOWN = 99
} p2p_error_t;

// Connection state
typedef enum {
    P2P_STATE_DISCONNECTED = 0,
    P2P_STATE_CONNECTING = 1,
    P2P_STATE_HANDSHAKE = 2,
    P2P_STATE_CONNECTED_P2P = 3,
    P2P_STATE_CONNECTED_RELAY = 4,
    P2P_STATE_FAILED = 5
} p2p_connection_state_t;

// Callback types
typedef void (*p2p_connected_callback_t)(void* user_data);
typedef void (*p2p_disconnected_callback_t)(void* user_data);
typedef void (*p2p_data_callback_t)(int channel_id, const uint8_t* data, size_t data_len, void* user_data);
typedef void (*p2p_error_callback_t)(p2p_error_t error, const char* message, void* user_data);
typedef void (*p2p_completion_callback_t)(p2p_error_t error, void* user_data);

/**
 * @brief Create P2P client configuration
 * @return Configuration handle
 */
p2p_config_t p2p_config_create(void);

/**
 * @brief Destroy P2P client configuration
 * @param config Configuration handle
 */
void p2p_config_destroy(p2p_config_t config);

/**
 * @brief Set signaling server
 * @param config Configuration handle
 * @param server Server hostname or IP
 * @param port Server port
 */
void p2p_config_set_signaling_server(p2p_config_t config, const char* server, uint16_t port);

/**
 * @brief Set STUN server
 * @param config Configuration handle
 * @param server Server hostname or IP
 * @param port Server port
 */
void p2p_config_set_stun_server(p2p_config_t config, const char* server, uint16_t port);

/**
 * @brief Set relay server
 * @param config Configuration handle
 * @param server Server hostname or IP
 * @param port Server port
 */
void p2p_config_set_relay_server(p2p_config_t config, const char* server, uint16_t port);

/**
 * @brief Set local UDP port
 * @param config Configuration handle
 * @param port Local port (0 for auto-assign)
 */
void p2p_config_set_local_port(p2p_config_t config, uint16_t port);

/**
 * @brief Create P2P client
 * @param did Device ID (null-terminated string)
 * @param config Configuration handle (can be NULL for defaults)
 * @return Client handle or NULL on failure
 */
p2p_client_t p2p_client_create(const char* did, p2p_config_t config);

/**
 * @brief Destroy P2P client
 * @param client Client handle
 */
void p2p_client_destroy(p2p_client_t client);

/**
 * @brief Initialize P2P client
 * @param client Client handle
 * @param callback Completion callback
 * @param user_data User data passed to callback
 * @return Error code
 */
p2p_error_t p2p_client_initialize(p2p_client_t client,
                                   p2p_completion_callback_t callback,
                                   void* user_data);

/**
 * @brief Connect to peer
 * @param client Client handle
 * @param peer_did Peer device ID
 * @param callback Completion callback
 * @param user_data User data passed to callback
 * @return Error code
 */
p2p_error_t p2p_client_connect(p2p_client_t client,
                                const char* peer_did,
                                p2p_completion_callback_t callback,
                                void* user_data);

/**
 * @brief Send data on channel
 * @param client Client handle
 * @param channel_id Channel ID
 * @param data Data buffer
 * @param data_len Data length
 * @param callback Completion callback
 * @param user_data User data passed to callback
 * @return Error code
 */
p2p_error_t p2p_client_send_data(p2p_client_t client,
                                  int channel_id,
                                  const uint8_t* data,
                                  size_t data_len,
                                  p2p_completion_callback_t callback,
                                  void* user_data);

/**
 * @brief Create data channel
 * @param client Client handle
 * @return Channel ID or -1 on failure
 */
int p2p_client_create_channel(p2p_client_t client);

/**
 * @brief Close data channel
 * @param client Client handle
 * @param channel_id Channel ID
 */
void p2p_client_close_channel(p2p_client_t client, int channel_id);

/**
 * @brief Close connection
 * @param client Client handle
 */
void p2p_client_close(p2p_client_t client);

/**
 * @brief Set connected callback
 * @param client Client handle
 * @param callback Callback function
 * @param user_data User data passed to callback
 */
void p2p_client_set_connected_callback(p2p_client_t client,
                                        p2p_connected_callback_t callback,
                                        void* user_data);

/**
 * @brief Set disconnected callback
 * @param client Client handle
 * @param callback Callback function
 * @param user_data User data passed to callback
 */
void p2p_client_set_disconnected_callback(p2p_client_t client,
                                           p2p_disconnected_callback_t callback,
                                           void* user_data);

/**
 * @brief Set data callback
 * @param client Client handle
 * @param callback Callback function
 * @param user_data User data passed to callback
 */
void p2p_client_set_data_callback(p2p_client_t client,
                                   p2p_data_callback_t callback,
                                   void* user_data);

/**
 * @brief Set error callback
 * @param client Client handle
 * @param callback Callback function
 * @param user_data User data passed to callback
 */
void p2p_client_set_error_callback(p2p_client_t client,
                                    p2p_error_callback_t callback,
                                    void* user_data);

/**
 * @brief Get connection state
 * @param client Client handle
 * @return Connection state
 */
p2p_connection_state_t p2p_client_get_state(p2p_client_t client);

/**
 * @brief Check if connected
 * @param client Client handle
 * @return 1 if connected, 0 otherwise
 */
int p2p_client_is_connected(p2p_client_t client);

/**
 * @brief Check if connection is P2P (not relay)
 * @param client Client handle
 * @return 1 if P2P, 0 otherwise
 */
int p2p_client_is_p2p(p2p_client_t client);

/**
 * @brief Get device ID
 * @param client Client handle
 * @return Device ID string (do not free)
 */
const char* p2p_client_get_did(p2p_client_t client);

/**
 * @brief Run event loop (blocking)
 * @param client Client handle
 * @return Error code
 */
p2p_error_t p2p_client_run(p2p_client_t client);

/**
 * @brief Stop event loop
 * @param client Client handle
 */
void p2p_client_stop(p2p_client_t client);

#ifdef __cplusplus
}
#endif
