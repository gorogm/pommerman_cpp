/**
  Taken from https://stackoverflow.com/a/20170989
 */

#ifndef TESTING_UTILITIES_HPP
#define TESTING_UTILITIES_HPP

#include <iostream>
#include <iomanip>
#include <locale>

#include "bboard.hpp"

/**
 * @author https://stackoverflow.com/a/7276879
 */
template<class T>
std::string FormatWithCommas(T value)
{
    std::stringstream ss;
    ss.imbue(std::locale(""));
    ss << std::fixed << value;
    return ss.str();
}

template <class T>
constexpr
std::string_view
type_name()
{
    using namespace std;
#ifdef __clang__
    string_view p = __PRETTY_FUNCTION__;
    return string_view(p.data() + 34, p.size() - 34 - 1);
#elif defined(__GNUC__)
    string_view p = __PRETTY_FUNCTION__;
#  if __cplusplus < 201402
    return string_view(p.data() + 36, p.size() - 36 - 1);
#  else
    return string_view(p.data() + 49, p.find(';', 49) - 49);
#  endif
#elif defined(_MSC_VER)
    string_view p = __FUNCSIG__;
    return string_view(p.data() + 84, p.size() - 84 - 7);
#endif
}

#endif // TESTING_UTILITIES_HPP
