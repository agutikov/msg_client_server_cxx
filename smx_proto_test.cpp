
#include "smx_proto.hpp"
#include <iostream>

using namespace piott::smx_proto;

int main()
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

    auto p = read<test_packet>(b1);

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

    boost::asio::mutable_buffer b3 = write<test_packet>(b2, h1);

    std::cout << b2 << std::endl;
    std::cout << b3 << std::endl;



    return 0;
}






