// For conditions of distribution and use, see copyright notice in license.txt

#ifndef incl_Core_StringUtil_h
#define incl_Core_StringUtil_h

namespace Core
{
    static std::wstring ToWString(const std::string &str)
    {
        std::wstring w_str(str.length(), L' ');
        std::copy(str.begin(), str.end(), w_str.begin());
        return w_str;
    }

    //! Converts value to a string. May throw boost::bad_lexical_cast.
    template <class T>
    static std::string ToString(const T &val) { return boost::lexical_cast<std::string>(val); }

    //! Converts string to a primitive type, such as int or float. May throw boost::bad_lexical_cast.
    template <typename T>
    static T ParseString(const std::string &val) { return boost::lexical_cast<T>(val); }
}
 
#endif


