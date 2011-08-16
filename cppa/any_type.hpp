#ifndef LIBCPPA_ANY_TYPE_HPP
#define LIBCPPA_ANY_TYPE_HPP

namespace cppa {

struct any_type
{
    constexpr any_type() { }
    constexpr inline operator any_type*() { return nullptr; }
};

inline bool operator==(const any_type&, const any_type&) { return true; }

template<typename T>
inline bool operator==(const T&, const any_type&) { return true; }

template<typename T>
inline bool operator==(const any_type&, const T&) { return true; }

inline bool operator!=(const any_type&, const any_type&) { return false; }

template<typename T>
inline bool operator!=(const T&, const any_type&) { return false; }

template<typename T>
inline bool operator!=(const any_type&, const T&) { return false; }

#ifdef __GNUC__
static constexpr any_type any_val __attribute__ ((unused));
#else
static constexpr any_type any_val;
#endif

} // namespace cppa

#endif // LIBCPPA_ANY_TYPE_HPP
