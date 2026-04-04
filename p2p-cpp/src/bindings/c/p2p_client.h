#ifndef P2P_CLIENT_H
#define P2P_CLIENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

// Opaque types
typedef struct p2p_client p2p_client_t;
typedef struct p2p_connection p2p_connection_t;
typedef struct p2p_config p2p_config_t;

// Status codes
typedef enum {
    P2P_OK = 0,
    P2P_ERROR_INVALID_ARGUMENT = 1,
    P2P_ERROR_CONNECTION_FAILED = 2,
    P2P_ERROR_TIMEOUT = 3,
    P2P_ERROR_NOT_FOUND = 4,
    P2P_ERROR_ALREADY_EXISTS = 5,
    P2P_ERROR_PERMISSION_DENIED = 6,
    P2P_ERROR_INTERNAL = 7
} p2p_status_t;

// Event types
typedef enum {
    P2P_EVENT_CONNECTION_OPENED = 0,
    P2P_EVENT_CONNECTION_CLOSED = 1,
    P2P_EVENT_CONNECTION_ERROR = 2,
    P2P_EVENT_DATA_RECEIVED = 3,
    P2P_EVENT_PEER_DISCOVERED = 4,
    P2P_EVENT_NAT_TYPE_DETECTED = 5
} p2p_event_type_t;

// Event callback
typedef void (*p2p_event_callback_t)(p2p_event_type_t event_type,
                                     const void* event_data,
                                     size_t event_data_size,
                                     void* user_data);

// Configuration
p2p_config_t* p2p_config_create(void);
void p2p_config_destroy(p2p_config_t* config);
void p2p_config_set_signaling_url(p2p_config_t* config, const char* url);
void p2p_config_set_stun_server(p2p_config_t* config, const char* server);
void p2p_config_set_max_connections(p2p_config_t* config, size_t max_connections);

// Client lifecycle
p2p_client_t* p2p_client_create(const p2p_config_t* config);
void p2p_client_destroy(p2p_client_t* client);
p2p_status_t p2p_client_start(p2p_client_t* client);
p2p_status_t p2p_client_stop(p2p_client_t* client);

// Event handling
void p2p_client_set_event_callback(p2p_client_t* client,
                                   p2p_event_callback_t callback,
                                   void* user_data);

// Connection management
p2p_status_t p2p_client_connect(p2p_client_t* client,
                                const char* peer_id,
                                p2p_connection_t** conn);
void p2p_connection_close(p2p_connection_t* conn);

// Data transfer
p2p_status_t p2p_connection_send(p2p_connection_t* conn,
                                 const uint8_t* data,
                                 size_t len);
p2p_status_t p2p_connection_recv(p2p_connection_t* conn,
                                 uint8_t* buffer,
                                 size_t buffer_size,
                                 size_t* received);

// Async data transfer
typedef void (*p2p_recv_callback_t)(p2p_status_t status,
                                    const uint8_t* data,
                                    size_t len,
                                    void* user_data);
void p2p_connection_recv_async(p2p_connection_t* conn,
                               p2p_recv_callback_t callback,
                               void* user_data);

// Connection info
const char* p2p_connection_get_peer_id(p2p_connection_t* conn);
const char* p2p_connection_get_local_addr(p2p_connection_t* conn);
const char* p2p_connection_get_remote_addr(p2p_connection_t* conn);

// Error handling
const char* p2p_status_message(p2p_status_t status);

#ifdef __cplusplus
}
#endif

#endif // P2P_CLIENT_H