#pragma once

#include <stdint.h>
#include <stddef.h>
#include "ios.h"

#define DEBUG_ATC_MATCH
#define DEBUG_ATC_CONTEXT

#define DEBUG

#ifdef __MBED__
// for experimental millis() shim
#include <Thread.h>
#include "us_ticker_api.h"
#elif !defined(ARDUINO)
#include <time.h>
#endif

#include "DebugContext.h"

namespace experimental
{
// all streams here are assumed binary

template <class TIStream>
class BlockingInputStream
{
public:
    BlockingInputStream(const TIStream& stream)  {}

    int getc();

    void read(const uint8_t* destination, size_t len);
};


template <class T>
struct ValueHolder
{
    const T& getValue() const;
    void setValue(const T& value);
};

struct EmptyValue;

template <>
struct ValueHolder<EmptyValue>
{

};

// portable stream wrapper for ALL scenarios
template <class TOStream>
class BlockingOutputStream
{
    // FIX: make this private once I figure out how to make my "friend" operator work right
public:
    TOStream& stream;

public:
    BlockingOutputStream(TOStream& stream) : stream(stream) {}

    void putc(uint8_t ch);

    void write(const uint8_t* source, size_t len);

    /*
    template <typename T>
    friend void operator << (BlockingOutputStream<TOStream>& stream, const T& value);
    */

    void test()
    {
        char buf[] = "Hello World";

        write((const uint8_t*)buf, 11);
    }
};


template <const char* subsystem = nullptr>
class ErrorTracker
{
    const char* category;
    const char* description;

public:
    void set(const char* category, const char* description)
    {
#ifdef DEBUG
        if(subsystem != nullptr)
            fstd::cerr << subsystem << ' ';

        fstd::cerr << "error: " << category << ": " << description << fstd::endl;
#endif
        this->category = category;
        this->description = description;
    }

    ErrorTracker& operator ()(const char* description)
    {
        set("general", description);
        return *this;
    }

    ErrorTracker& operator ()(const char* category, const char* description)
    {
        set(category, description);
        return *this;
    }

    const char* get_category() { return category; }
    const char* get_description() { return description; }
};

template <class TParserTraits>
class ParserWrapper;

struct tokenizer_traits
{
    //static constexpr char* subsystem_name = "Tokenizer";
    //static constexpr char* subsystem_name() { return "Tokenizer"; }
    static constexpr char subsystem_name[] = "Tokenizer";
};


template <class TTraits = tokenizer_traits>
class Tokenizer
{
    friend class ParserWrapper<TTraits>;

    const char* delimiters;

    /**
     * See if one character exists within a string of characters
     *
     * @param c
     * @param match
     * @return
     */
    static bool is_match(char c, const char* match)
    {
        char delim;

        while((delim = *match++))
            if(delim == c) return true;

#ifdef DEBUG_ATC_MATCH
        //fstd::clog << "Didn't match on char# " << (int) c << fstd::endl;
#endif

        return false;
    }

public:
    typedef TTraits traits_t;

protected:
#ifdef DEBUG
    ErrorTracker<traits_t::subsystem_name> error;
#endif

    DebugContext<traits_t::subsystem_name> debug_context;

public:
    /*
    Tokenizer(const char* delimiters) : delimiters(delimiters)
    {

    } */

    void set_delimiter(const char* delimiters)
    {
        this->delimiters = delimiters;
    }

    bool is_delimiter(char c) const
    {
        return is_match(c, delimiters);
    }

    /**
     * @brief Matches a string against input
     *
     * This is a blocking function (via peek)
     *
     * @param match - string to compare against input
     * @return true if successful
     */
#ifndef DEBUG
    static
#endif
        bool token_match(fstd::istream& cin, const char* match)
    {
        char ch;
#ifdef DEBUG
        const char* original_match = match;
#endif

#ifdef DEBUG_ATC_MATCH
        debug_context.identify(fstd::clog);
        fstd::clog << "Match raw '" << match << "' = ";
#endif

        while((ch = *match++))
        {
            int _ch = cin.peek();

            if(ch != _ch)
            {
#ifdef DEBUG_ATC_MATCH
                fstd::clog << "false   preset='" << ch << "',incoming='" << _ch << '\'' << fstd::endl;
#endif
#ifdef DEBUG
                error("match", original_match);
#endif
                return false;
            }
            cin.ignore();
        }

#ifdef DEBUG_ATC_MATCH
        fstd::clog << "true" << fstd::endl;
#endif
        return true;
    }

    size_t tokenize(::fstd::istream& cin, char* input, size_t max) const;
};

template <class TTraits = tokenizer_traits>
class Parser : public Tokenizer<TTraits>
{
public:
    template <typename T>
    bool parse(fstd::istream& cin, T& inputValue)
#ifndef DEBUG
            const
#endif
    {
        // TODO: disallow constants from coming in here
        //static_assert(T, "Cannot input into a static pointer");

        constexpr uint8_t maxlen = ::experimental::maxStringLength<T>();
        char buffer[maxlen + 1];

        size_t n = this->tokenize(cin, buffer, maxlen);
#ifdef DEBUG
        const char* err = validateString<T>(buffer);
        if(err)
        {
            this->error("validation", err);
            return false;
        }

        if(n == 0) return false;
#endif
        inputValue = fromString<T>(buffer);

#ifdef DEBUG_ATC_INPUT
        fstd::clog << "Input raw = " << buffer << " / cooked = ";
        fstd::clog << inputValue << fstd::endl;
#endif

        return true;
    }


    /**
     * Matches character input against a strongly typed value
     *  converting incoming to-match into a string
     *  and then comparing the string
     *
     * @tparam T
     * @param cin
     * @param match
     * @return
     */
    template <typename T>
#ifndef DEBUG
    static
#endif
        bool parse_match_fast(fstd::istream& cin, T match)
    {
        constexpr uint8_t size = experimental::maxStringLength<T>();
        char buf[size + 1];

        toString(buf, match);

        return token_match(cin, buf);
    }


    template <typename T>
    bool parse_match(fstd::istream& cin, T match)
#ifndef DEBUG
        const
#endif
    {
        T input;

#ifdef DEBUG_ATC_MATCH
        this->debug_context.identify(fstd::clog);
#endif

        if(!parse(cin, input)) return false;

#ifdef DEBUG
        if(input != match)
            fstd::clog << "parse_match failure: '" << input << "' != expected '" << match << '\'' << fstd::endl;
#endif
        return input == match;
    }
};


template <class TChar, class TTraits = fstd::char_traits<TChar>>
class basic_istream_ref
{
protected:
    typedef fstd::basic_istream<TChar> istream_t;
    typedef basic_istream_ref<TChar> bir_t;
    typedef typename TTraits::int_type int_type;

    istream_t& cin;

public:
    basic_istream_ref(istream_t& cin) : cin(cin) {}

    template <typename T>
    bir_t& operator>>(T& value)
    {
        cin >> value;
        return *this;
    }

    bir_t& read(TChar* s, fstd::streamsize n)
    {
        cin.read(s, n);
        return *this;
    }

    int_type peek() { return cin.peek(); }

    int_type get() { return cin.get(); }
};


template <class TChar, class TTraits = fstd::char_traits<TChar>>
class basic_ostream_ref
{
protected:
    typedef fstd::basic_ostream <TChar> ostream_t;
    typedef basic_ostream_ref<TChar> bor_t;
    typedef typename TTraits::int_type int_type;

    ostream_t& cout;

public:
    basic_ostream_ref(ostream_t& cout) : cout(cout) {}

    void write(const char* s, fstd::streamsize len)
    {
#ifdef DEBUG_ATC_OUTPUT
        fstd::clog.write(s, len);
#endif

        cout.write(s, len);
    }

    void put(char ch)
    {
#ifdef DEBUG_ATC_OUTPUT
        fstd::clog.put(ch);
#endif

        cout.put(ch);
    }


    template <typename T>
    bor_t& operator<<(T outputValue)
    {
#ifdef DEBUG_ATC_OUTPUT
        fstd::clog << outputValue;
#endif
        cout << outputValue;
        return *this;
    }
};

// Ease off this one for now until we try Parser/Tokenizer in more real world scenarios
template <class TParserTraits = tokenizer_traits>
class ParserWrapper :
        public basic_istream_ref<char>
{
#ifdef FEATURE_DISCRETE_PARSER_FORMATTER
    typedef uint8_t input_processing_flags;

    static constexpr input_processing_flags eat_delimiter_bit = 0x01;
    // FIX: this is an OUTPUT processing flag
    static constexpr input_processing_flags auto_delimit_bit = 0x02;

    input_processing_flags flags;
#endif

protected:
    Parser<TParserTraits> parser;

#ifdef FEATURE_PARSER_ERRORTRACKER
    void reset_error() { parser.error(nullptr, nullptr); }
    const char* get_error() { return parser.error.get_category(); }
    void set_error(const char* category, const char* description)
    {
        parser.error(category, description);
    }
#endif

public:
    ParserWrapper(fstd::istream& cin) : basic_istream_ref<char>(cin) {}

#ifdef FEATURE_PARSER_ERRORTRACKER
    bool is_in_error() { return parser.error.get_description(); }
#endif

    // FIX: temporary/debug only.  interested parties should eventually
    // use an accessor call or directly reference parser
    DebugContext<TParserTraits::subsystem_name>& debug_context = parser.debug_context;

#ifdef FEATURE_DISCRETE_PARSER_FORMATTER
    void set_eat_delimiter()
    { flags |= eat_delimiter_bit; }

    bool eat_delimiter()
    { return flags & eat_delimiter_bit; }

    void eat_delimiters(const char* delimiters)
    {
        parser.set_delimiter(delimiters);
        set_eat_delimiter();
    }
#endif

    template <typename T>
    bool parse(T& inputValue)
    {
        bool result = parser.parse(cin, inputValue);

#ifdef FEATURE_DISCRETE_PARSER_FORMATTER
        if(eat_delimiter())
        {
            int ch;

            while((ch = cin.peek()) != -1 && parser.is_delimiter(ch))
            {
                cin.ignore();
            }
        }
#endif
        return result;
    }

    void set_delimiter(const char* delimiters)
    {
        parser.set_delimiter(delimiters);
    }

    bool is_delimiter(char c)
    {
        return parser.is_delimiter(c);
    }

    bool is_match(char c, const char* match)
    {
        return parser.is_match(c, match);
    }
};

}

#include "Tokenizer.hpp"

namespace FactUtilEmbedded { namespace std {

// all lifted from cppreference.com.  Hopefully we can use existing ones... but until we prove we can,
// use these instead
template <bool, typename T = void>
struct enable_if
{};

template <typename T>
struct enable_if<true, T> {
    typedef T type;
};


template<class T, T v>
struct integral_constant {
    static constexpr T value = v;
    typedef T value_type;
    typedef integral_constant type; // using injected-class-name
    constexpr operator value_type() const noexcept { return value; }
    constexpr value_type operator()() const noexcept { return value; } //since c++14
};


typedef integral_constant<bool, true> true_type;
typedef integral_constant<bool, false> false_type;

// primary template
template<class>
struct is_function : false_type { };

// specialization for regular functions
template<class Ret, class... Args>
struct is_function<Ret(Args...)> : true_type {};

// specialization for variadic functions such as std::printf
template<class Ret, class... Args>
struct is_function<Ret(Args...,...)> : true_type {};

// specialization for function types that have cv-qualifiers
template<class Ret, class... Args>
struct is_function<Ret(Args...)const> : std::true_type {};
template<class Ret, class... Args>
struct is_function<Ret(Args...)volatile> : std::true_type {};
template<class Ret, class... Args>
struct is_function<Ret(Args...)const volatile> : std::true_type {};
template<class Ret, class... Args>
struct is_function<Ret(Args...,...)const> : std::true_type {};
template<class Ret, class... Args>
struct is_function<Ret(Args...,...)volatile> : std::true_type {};
template<class Ret, class... Args>
struct is_function<Ret(Args...,...)const volatile> : std::true_type {};
}}