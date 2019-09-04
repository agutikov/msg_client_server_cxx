
#include <cstddef>
#include <string>
#include <vector>

#include <boost/fusion/include/define_struct.hpp>

#include "proto.hpp"
using namespace piott::proto;

// Programming Interview Offline Test Task
namespace piott {

// Simple Message eXchange protocol
namespace smx_proto {
    

enum class packet_type_t : uint32_t
{
    // server sends to client immediately after new connection accepted
    GREETER = 0x1,

    // client >> server - request msg data and/or state
    // server >> client - response msg data and/or state
    MSG_STATE = 0x2,

    // server >> client, list of IDs of available messages
    MSG_ID_LIST = 0x3,

    // client >> server - post
    // server >> client - get responce
    MSG_DATA = 0x4,
};

// limit of packet data length
using packet_length_t = uint32_t;

using proto_magic_t = std::integral_constant<uint32_t, 0xDEADFACE>;

enum class proto_version_t : uint32_t
{
    PROTO_V_1 = 1,
};

// client >> server
enum class msg_state_client_t : int32_t
{
    // get msg state request
    DEFAULT = 0,

    // claim mesasge and request message data
    WAIT_FOR_DATA = 1,
    
    // request message ownership, request to delete message on server side
    WAIT_FOR_OWNERSHIP = 2,
};

// server >> client
enum class msg_state_server_t : int32_t
{
    // message accepted by server, message is available for claiming
    DEFAULT = 0,
    
    // msg claimed on server side
    CLAIMED = -1,
    
    // msg deleted io server side
    DELETED = -2,
};

using client_id_t = uint64_t;

using msg_id_t = uint64_t;

using msg_data_t = std::vector<uint8_t>;

using msg_id_list_t = std::vector<msg_id_t>;


// struct defined with BOOST_FUSION_DEFINE_STRUCT has no default constructor from initializer list
struct packet_header
{
    proto_magic_t magic;
    proto_version_t version = proto_version_t::PROTO_V_1;
    packet_length_t length;
    packet_type_t packet_type;
};


}
}



BOOST_FUSION_ADAPT_STRUCT(
    piott::smx_proto::packet_header,
    (piott::smx_proto::proto_magic_t, magic)
    (piott::smx_proto::proto_version_t, version)
    (piott::smx_proto::packet_length_t, length) 
    (piott::smx_proto::packet_type_t, packet_type)
)

// client >> server
// contains only message state
BOOST_FUSION_DEFINE_STRUCT(
    (piott)(smx_proto), request_with_msg_state,
    (piott::smx_proto::msg_id_t, msg_id)
    (piott::smx_proto::msg_state_client_t, msg_state)
)

// client >> server
// contains message state and data
BOOST_FUSION_DEFINE_STRUCT(
    (piott)(smx_proto), request_with_msg_data,
    (piott::smx_proto::msg_id_t, msg_id)
    (piott::smx_proto::msg_state_client_t, msg_state)
    (piott::smx_proto::msg_data_t, msg_data)
)

// server >> client
BOOST_FUSION_DEFINE_STRUCT(
    (piott)(smx_proto), response_msg_state,
    (piott::smx_proto::msg_id_t, msg_id)
    (piott::smx_proto::msg_state_server_t, msg_state)
)

// server >> client
BOOST_FUSION_DEFINE_STRUCT(
    (piott)(smx_proto), response_msg_data,
    (piott::smx_proto::msg_id_t, msg_id)
    (piott::smx_proto::msg_state_server_t, msg_state)
    (piott::smx_proto::msg_data_t, msg_data)
)

// server >> client
BOOST_FUSION_DEFINE_STRUCT(
    (piott)(smx_proto), greeter,
    (piott::smx_proto::client_id_t, client_id)
)

// server >> client
BOOST_FUSION_DEFINE_STRUCT(
    (piott)(smx_proto), announce_msg_id_list,
    (piott::smx_proto::msg_id_list_t, msg_id_list)
)


