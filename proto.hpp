
#include <iostream>
#include <iomanip>
#include <cstddef>
#include <string>
#include <vector>
#include <exception>

#include <boost/asio/buffer.hpp>

#include <boost/fusion/include/define_struct.hpp>
#include <boost/fusion/include/for_each.hpp>
#include <boost/fusion/include/value_at.hpp>
#include <boost/fusion/include/at.hpp>

#include <boost/mpl/range_c.hpp>
#include <boost/mpl/for_each.hpp>
#include <boost/mpl/size.hpp>

#include <endian.h>

namespace piott {
namespace proto {

//TODO: Add checksum

/*
 * It is nessesary to use ntoh/hton for integral fields in packets
 * if we want code compiled for different architectures with different byteorder work together.
 * If we plan to run only on x86 or at least on same byteorder architecture each time
 * then hton/ntoh can be safely removed and it will give us simething like 0.000001% of performance :)
 * 
 * I use here big-endian because MEGA-COOL Heavy-Metal magic number 0xDEADFACE is then readable in packet dump!
 */ 
template<class T>
T ntoh(T v);

template<class T>
T hton(T v);

template<>
uint64_t ntoh(uint64_t v)
{
    return be64toh(v);
}

template<>
uint32_t ntoh(uint32_t v)
{
    return be32toh(v);
}

template<>
uint16_t ntoh(uint16_t v)
{
    return be16toh(v);
}

template<>
uint64_t hton(uint64_t v)
{
    return htobe64(v);
}

template<>
uint32_t hton(uint32_t v)
{
    return htobe32(v);
}

template<>
uint16_t hton(uint16_t v)
{
    return htobe16(v);
}

///////////////////////////////////////////////////////////////////////////////
// DESERIALIZATION
///////////////////////////////////////////////////////////////////////////////

struct unpack_item
{
    mutable boost::asio::const_buffer input_buffer;

    explicit unpack_item(boost::asio::const_buffer input_buffer)
        : input_buffer(std::move(input_buffer))
    { }

    template<class T>
    auto operator()(T & val) const ->
    typename std::enable_if<std::is_integral<T>::value>::type
    {
        val = ntoh(*boost::asio::buffer_cast<T const*>(input_buffer));
        input_buffer = input_buffer + sizeof(T);
    }

    template<class T>
    auto operator()(T & val) const ->
    typename std::enable_if<std::is_enum<T>::value>::type
    {
        typename std::underlying_type<T>::type v;
        (*this)(v);
        val = static_cast<T>(v);
    }
    
    template<class T, T v>
    void operator()(std::integral_constant<T, v>) const
    {
        typedef std::integral_constant<T, v> type;
        typename type::value_type val;
        (*this)(val);
        if (val != type::value)
            throw std::domain_error("Invalid protocol magic number");
    }

    template<class T>
    void operator()(std::vector<T> & vals) const
    {
        uint32_t length;
        (*this)(length);
        for (; length; --length) {
            T val;
            (*this)(val);
            vals.emplace_back(std::move(val));
        }
    }

    void operator()(std::string& val) const
    {
        uint32_t length = 0;
        (*this)(length);
        val = std::string(boost::asio::buffer_cast<char const*>(input_buffer), length);
        input_buffer = input_buffer + length;
    }
};

template<typename T>
std::pair<T, boost::asio::const_buffer> unpack_struct(boost::asio::const_buffer input_buffer)
{
    unpack_item unpack(std::move(input_buffer));
    T res;
    boost::fusion::for_each(res, unpack);
    return std::make_pair(res, unpack.input_buffer);
}


///////////////////////////////////////////////////////////////////////////////
// SERIALIZATION
///////////////////////////////////////////////////////////////////////////////

struct pack_item
{
    mutable boost::asio::mutable_buffer output_buffer;

    explicit pack_item(boost::asio::mutable_buffer output_buffer)
    : output_buffer(std::move(output_buffer))
    { }

    template<class T>
    auto operator()(T const& val) const ->
    typename std::enable_if<std::is_integral<T>::value>::type
    {
        T tmp = hton(val);
        boost::asio::buffer_copy(output_buffer, boost::asio::buffer(&tmp, sizeof(T)));
        output_buffer = output_buffer + sizeof(T);
    }
    
    template<class T>
    auto operator()(T const& val) const ->
    typename std::enable_if<std::is_enum<T>::value>::type
    {
        using utype = typename std::underlying_type<T>::type;
        (*this)(static_cast<utype>(val));
    }

    template<class T, T v>
    void operator()(std::integral_constant<T, v>) const
    {
        typedef std::integral_constant<T, v> type;
        (*this)(type::value);
    }

    template<class T>
    void operator()(std::vector<T> const& vals) const
    {
        (*this)(static_cast<uint32_t>(vals.size()));
        for(auto&& val : vals) {
            (*this)(val);
        }
    }

    void operator()(std::string const& val) const
    {
        (*this)(static_cast<uint32_t>(val.size()));
        boost::asio::buffer_copy(output_buffer, boost::asio::buffer(val));
        output_buffer = output_buffer + val.size();
    }
};

template<typename T>
boost::asio::mutable_buffer pack_struct(boost::asio::mutable_buffer output_buffer, T const& val)
{
    pack_item pack(std::move(output_buffer));
    boost::fusion::for_each(val, pack);
    return pack.output_buffer;
}

///////////////////////////////////////////////////////////////////////////////
// PRETTY PRINT
///////////////////////////////////////////////////////////////////////////////

namespace detail {
    namespace f_ext = boost::fusion::extension;
    namespace mpl = boost::mpl;

#if 0
    template<typename T>
    typename std::enable_if_t<
        std::conjunction_v<
            std::is_enum<T>,
            std::is_signed<typename std::underlying_type<T>::type>
        >,
        std::ostream&
    >
    operator << (std::ostream& os, const T& enum_val)
    {
        os << static_cast<typename std::underlying_type<T>::type>(enum_val);
        return os;
    }
#endif

    //TODO: Runtime output format selection: debug, JSON, C++ initializer list

    template<typename T>
    typename std::enable_if<std::is_enum<T>::value, std::ostream&>::type
    operator << (std::ostream& os, const T& enum_val)
    {
        if constexpr (std::is_unsigned<typename std::underlying_type<T>::type>::value) {
            os << "0x" << std::hex << std::uppercase;
        }
        os << static_cast<typename std::underlying_type<T>::type>(enum_val);
        if constexpr (std::is_unsigned<typename std::underlying_type<T>::type>::value) {
            os << std::dec << std::nouppercase;
        }
        return os;
    }

    template<typename T, T v>
    std::ostream& operator << (std::ostream& os, const std::integral_constant<T, v>& val)
    {
        os << "0x" << std::hex << std::uppercase;
        os << static_cast<T>(val);
        os << std::dec << std::nouppercase;
        return os;
    }

    template<class T>
    std::ostream& operator << (std::ostream& os, std::vector<T> const& vals)
    {
        for (auto it = std::begin(vals); it < vals.end(); it++) {
            if (it != std::begin(vals)) {
                os << ", ";
            }
            os << *it;
        }
        os << ";";
        return os;
    }

#if 0
    std::ostream& operator << (std::ostream& os, const proto_magic_t& magic_val)
    {
        os << "__SMX_MAGIC__"; 
        return os;
    }
#endif

    template<class T>
    struct visitor
    {
        std::ostream & os_;
        T const& msg_;

        visitor(std::ostream & os, T const& msg) :
            os_(os),
            msg_(msg)
        {}

        template<class N>
        void operator()(N idx)
        {
            os_ << (idx == 0 ? "{ " : ", ");
            os_ << f_ext::struct_member_name<T, N::value>::call();
            os_ << ": ";
            os_ << boost::fusion::at<N>(msg_);
            os_ << (idx == mpl::size<T>::value - 1 ? " }" : "");
        }
    };

    template<class T>
    struct pprinter
    {
        T const& msg_;

        pprinter(T const& msg) :
            msg_(msg)
        {}

        friend std::ostream& operator << (std::ostream& os, pprinter<T> const& v)
        {
            mpl::for_each<mpl::range_c<int, 0, mpl::size<T>::value>>(visitor<T>(os, v.msg_));
            return os;
        }
    };
}

template<class T>
detail::pprinter<T> pretty_print(T const& v)
{
    return detail::pprinter<T>(v);
}


}} // namespace


///////////////////////////////////////////////////////////////////////////////
// UTILS
///////////////////////////////////////////////////////////////////////////////

/*
 * Prettiest possible dump of memory.
 * Takes into account addresses and start offset.
 * Shows memory aligned to 16 bytes.
 */
struct hex_pprinter
{
    const void* ptr;
    size_t size;

    hex_pprinter(const void* ptr, size_t size) :
        ptr(ptr),
        size(size)
    {}

    friend std::ostream& operator << (std::ostream& os, hex_pprinter const& v)
    {
        size_t size = v.size;
        const char* buf = static_cast<const char*>(v.ptr);
        const char* end = buf + v.size;
        uint64_t offset = reinterpret_cast<uint64_t>(buf) % 16;

        os << std::hex << std::setfill('0');

        while (buf < end) {
            int nread =     size >= 16 ? 16 : size;
            if (offset > 0) {
                if (nread + offset > 16) {
                    nread = 16 - offset;
                }
            }

            // Show the address
            os << std::setw(16) << (reinterpret_cast<uint64_t>(buf) & ~0x0F);

            int offset_counter = offset;
            // Show the hex codes
            for (int i = 0; i < 16; i++) {
                
                if (i % 8 == 0) {
                    os << ' ';
                }

                if (offset_counter == 0 && i < (nread + offset)) {
                    os << ' ' << std::setw(2) << (unsigned int)(unsigned char)(buf[i - offset]);
                } else {
                    os << " __";
                }

                if (offset_counter > 0) {
                    offset_counter--;
                }
            }

            offset_counter = offset;
            // Show printable characters
            os << "  |";
            for (int i = 0; i < 16; i++) {
                
                if (offset_counter == 0 && i < (nread + offset)) {
                    os << (std::isprint(buf[i - offset]) ? buf[i - offset] : '.');
                } else {
                    os << ' ';
                }

                if (offset_counter > 0) {
                    offset_counter--;
                }
            }
            os << "|" << std::endl;

            buf += nread;
            size -= nread;
            offset = 0;
        }
        os << std::dec << std::setfill(' ') << std::setw(0);

        return os;
    }
};

std::ostream& operator << (std::ostream& os, boost::asio::const_buffer const& buffer)
{
    os << "--------------------  const_buffer " << (void*)(&buffer) << ", size = " << std::setw(4) << std::left << buffer.size()
       << " -----------------------" << std::endl << std::right << std::setw(0);
    os << hex_pprinter(buffer.data(), buffer.size());
    return os;
}

std::ostream& operator << (std::ostream& os, boost::asio::mutable_buffer const& buffer)
{
    os << "--------------------  mutable_buffer " << (void*)(&buffer) << ", size = " << std::setw(4) << std::left << buffer.size()
       << " ---------------------" << std::endl << std::right << std::setw(0);
    os << hex_pprinter(buffer.data(), buffer.size());
    return os;
}


