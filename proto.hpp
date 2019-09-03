
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

#include <arpa/inet.h>

namespace piott {
namespace proto {

///////////////////////////////////////////////////////////////////////////////
// DESERIALIZATION
///////////////////////////////////////////////////////////////////////////////

template<class T>
T ntoh(T v);

template<>
uint32_t ntoh(uint32_t v)
{
    return ntohl(v);
}

template<>
uint16_t ntoh(uint16_t v)
{
    return ntohs(v);
}

struct reader
{
    mutable boost::asio::const_buffer buf_;

    explicit reader(boost::asio::const_buffer buf)
        : buf_(std::move(buf))
    { }

    template<class T>
    void operator()(T & val) const
    {
        val = ntoh(*boost::asio::buffer_cast<T const*>(buf_));
        buf_ = buf_ + sizeof(T);
    }

    template<class T>
    auto operator()(T & val) const -> typename std::enable_if<std::is_enum<T>::value>::type
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
        val = std::string(boost::asio::buffer_cast<char const*>(buf_), length);
        buf_ = buf_ + length;
    }
};

template<typename T>
std::pair<T, boost::asio::const_buffer> read(boost::asio::const_buffer b)
{
    reader r(std::move(b));
    T res;
    boost::fusion::for_each(res, r);
    return std::make_pair(res, r.buf_);
}


///////////////////////////////////////////////////////////////////////////////
// SERIALIZATION
///////////////////////////////////////////////////////////////////////////////

template<class T>
T hton(T v);

template<>
uint32_t hton(uint32_t v)
{
    return htonl(v);
}

template<>
uint16_t hton(uint16_t v)
{
    return htons(v);
}

struct writer
{
    mutable boost::asio::mutable_buffer buf_;

    explicit writer(boost::asio::mutable_buffer buf)
    : buf_(std::move(buf))
    { }

    template<class T>
    void operator()(T const& val) const
    {
        T tmp = hton(val);
        boost::asio::buffer_copy(buf_, boost::asio::buffer(&tmp, sizeof(T)));
        buf_ = buf_ + sizeof(T);
    }
    
    template<class T>
    auto operator()(T const& val) const -> typename std::enable_if<std::is_enum<T>::value>::type
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
        (*this)(vals.length());
        for(auto&& val : vals) {
            (*this)(val);
        }
    }

    void operator()(std::string const& val) const
    {
        (*this)(static_cast<uint32_t>(val.length()));
        boost::asio::buffer_copy(buf_, boost::asio::buffer(val));
        buf_ = buf_ + val.length();
    }
};

template<typename T>
boost::asio::mutable_buffer write(boost::asio::mutable_buffer b, T const& val)
{
    writer w(std::move(b));
    boost::fusion::for_each(val, w);
    return w.buf_;
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


