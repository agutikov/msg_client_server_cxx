
#include "smx_proto.hpp"
#include <iostream>

#include <boost/asio.hpp>

using namespace piott::smx_proto;

void test_2()
{
    announce_msg_id_list id_list;
    id_list.msg_id_list = piott::smx_proto::msg_id_list_t(40, 0xABABABABCDCDCDCD);

    boost::asio::streambuf b;
    //b.reserve(1024);

    std::cout << "b.capacity = " << b.capacity() << std::endl << b.data() << std::endl;
    auto header_buffer = b.prepare(sizeof(packet_header));

    std::cout << "b.capacity = " << b.capacity() << std::endl << b.data() << std::endl;
    b.commit(header_buffer.size());

    std::cout << "b.capacity = " << b.capacity() << std::endl << b.data() << std::endl;

    auto payload_buffer = b.prepare(sizeof(uint32_t) + id_list.msg_id_list.size() * sizeof(*id_list.msg_id_list.data()));
    std::cout << "b.capacity = " << b.capacity() << std::endl << b.data() << std::endl;

    b.commit(payload_buffer.size());
    std::cout << "b.capacity = " << b.capacity() << std::endl << b.data() << std::endl;

    std::cout << "==================================" << std::endl;

    pack_struct(payload_buffer, id_list);

    packet_header header = {
        .length = static_cast<packet_length_t>(payload_buffer.size()), 
        .packet_type = packet_type_t::MSG_ID_LIST
    };

    pack_struct(header_buffer, header);

    std::cout << pretty_print(header) << std::endl;
    std::cout << header_buffer << std::endl;

    std::cout << pretty_print(id_list) << std::endl;
    std::cout << payload_buffer << std::endl;

    std::cout << "b.capacity = " << b.capacity() << std::endl << b.data() << std::endl;

    b.consume(b.data().size());
    std::cout << "b.capacity = " << b.capacity() << std::endl << b.data() << std::endl;
}


BOOST_FUSION_DEFINE_STRUCT(
    (piott)(smx_proto), test_packet,
    (uint32_t, magic)
    (uint32_t, version)
    (uint32_t, length) 
    (uint32_t, packet_type)
    (std::string, xxx)
)

void test_1()
{
    packet_header header = {.packet_type = packet_type_t::GREETER};
    std::cout << pretty_print(header) << std::endl;


    const unsigned char b1_data[] = {
        0xDE, 0xAD, 0xFA, 0xCE,
        0x00, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x10,
        0x00, 0x00, 0x00, 0x02,
        0x00, 0x00, 0x00, 0x00,
    };
    boost::asio::const_buffer b1(b1_data, sizeof(b1_data));

    auto p = unpack_struct<test_packet>(b1);

    std::cout << pretty_print(p.first) << std::endl;



    test_packet h1;
    h1.magic = 1;
    h1.version = 2;
    h1.length = 3;
    h1.packet_type = 4;
    h1.xxx = "ihglhg67r7*$*&^";

    uint8_t b2_data[45];
    std::memset(b2_data, 0xab, sizeof(b2_data));

    boost::asio::mutable_buffer b2(b2_data, sizeof(b2_data));

    boost::asio::mutable_buffer b3 = pack_struct<test_packet>(b2, h1);

    std::cout << b2 << std::endl;
    std::cout << b3 << std::endl;
}


int main()
{
    //test_1();
    test_2();





    return 0;
}






