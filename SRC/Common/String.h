#pragma once

namespace Util
{
    using convert_type = std::codecvt_utf8<wchar_t>;

    template <typename _Out, typename _In>
    struct string_convert_t {};

    template <>
    struct string_convert_t<char, char>
    {
        inline static std::string convert(const std::string& str)
        {
            return str;
        }

        inline static std::string_view convert(std::string_view str)
        {
            return str;
        }
    };

    template <>
    struct string_convert_t<wchar_t, wchar_t>
    {
        inline static std::wstring convert(const std::wstring& str)
        {
            return str;
        }

        inline static std::wstring_view convert(std::wstring_view str)
        {
            return str;
        }
    };

    template <typename _Out>
    struct string_convert_t<_Out, wchar_t>
    {
        inline static std::basic_string<_Out> convert(const std::wstring& str)
        {
            static std::wstring_convert<convert_type, wchar_t> converter;
            return std::move(converter.to_bytes(str));
        }

        inline static std::basic_string<_Out> convert(std::wstring_view str)
        {
            static std::wstring_convert<convert_type, wchar_t> converter;
            return std::move(converter.to_bytes(str.data()));
        }
    };

    template <typename _Out>
    struct string_convert_t<_Out, char>
    {
        inline static std::basic_string<_Out> convert(const std::string& str)
        {
            static std::wstring_convert<convert_type, wchar_t> converter;
            return std::move(converter.from_bytes(str));
        }

        inline static std::basic_string<_Out> convert(std::string_view str)
        {
            static std::wstring_convert<convert_type, wchar_t> converter;
            return std::move(converter.from_bytes(str.data()));
        }
    };

    template <typename _Out, typename _In>
    constexpr inline std::basic_string<_Out> convert(const std::basic_string<_In>& str)
    {
        return std::move(string_convert_t<_Out, _In>::convert(str));
    }

    template <typename _Out, typename _In>
    constexpr inline std::basic_string<_Out> convert(std::basic_string_view<_In> str)
    {
        return std::move(string_convert_t<_Out, _In>::convert(str));
    }
}