// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include "Util/Expr/Lexer.hpp"
#include "Util/StringUtils.hpp"
#include <array>
#include <charconv>
#include <cmath>
#include <limits>
#include <locale>
#include <sstream>
#include <string>

using namespace Pothos::Util::Expr;

namespace {

bool isDigit(const char ch)
{
    return ch >= '0' and ch <= '9';
}

bool isNameStart(const char ch)
{
    return (ch >= 'a' and ch <= 'z') or (ch >= 'A' and ch <= 'Z') or ch == '_';
}

bool isNameChar(const char ch)
{
    return isNameStart(ch) or isDigit(ch);
}

int hexValue(const char ch)
{
    if (isDigit(ch)) return ch - '0';
    if (ch >= 'a' and ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' and ch <= 'F') return ch - 'A' + 10;
    return -1;
}

class Lexer
{
public:
    Lexer(const std::string_view expr):
        _expr(expr)
    {
        return;
    }

    std::vector<Token> run(void)
    {
        std::vector<Token> toks;
        while (true)
        {
            while (_i < _expr.size() and Pothos::Util::Detail::isSpace(_expr[_i])) _i++;
            if (_i >= _expr.size()) break;
            const auto start = _i;
            const char ch = _expr[_i];
            Token tok{Tok::Op, {}, start, {}};
            if (isDigit(ch) or (ch == '.' and isDigit(peek(1)))) this->number(tok);
            else if (ch == '"' or ch == '\'') this->string(tok);
            else if (isNameStart(ch))
            {
                while (_i < _expr.size() and isNameChar(_expr[_i])) _i++;
                tok.kind = Tok::Name;
            }
            else this->op();
            tok.text = _expr.substr(start, _i-start);
            toks.push_back(std::move(tok));
        }
        toks.push_back(Token{Tok::End, _expr.substr(_expr.size()), _expr.size(), {}});
        return toks;
    }

private:
    char peek(const std::size_t ahead) const
    {
        return (_i+ahead < _expr.size())? _expr[_i+ahead] : '\0';
    }

    [[noreturn]] void fail(const std::string &message, const std::size_t pos) const
    {
        throw ExprError(message, pos);
    }

    void op(void)
    {
        static const std::array<std::string_view, 8> twoChar{"**", "//", "<<", ">>", "<=", ">=", "==", "!="};
        static const std::string_view oneChar("+-*/%&|^~<>()[]{},:");
        const auto rest = _expr.substr(_i);
        for (const auto &op : twoChar)
        {
            if (rest.starts_with(op))
            {
                _i += 2;
                return;
            }
        }
        if (oneChar.find(rest.front()) != std::string_view::npos)
        {
            _i++;
            return;
        }
        fail("unexpected character '" + std::string(1, rest.front()) + "'", _i);
    }

    //Digits with single underscores between them, returned without the underscores
    std::string digits(const std::size_t start, int (*digitValue)(char), const int base, const bool leadingUnderscore = false)
    {
        std::string out;
        bool lastUnderscore = leadingUnderscore;
        while (_i < _expr.size())
        {
            const char ch = _expr[_i];
            if (ch == '_')
            {
                if (lastUnderscore or (out.empty() and not leadingUnderscore)) fail("invalid number literal", start);
                lastUnderscore = true;
            }
            else if (digitValue(ch) >= 0 and digitValue(ch) < base)
            {
                out += ch;
                lastUnderscore = false;
            }
            else break;
            _i++;
        }
        if (lastUnderscore and _i > start and _expr[_i-1] == '_') fail("invalid number literal", start);
        return out;
    }

    void number(Token &tok)
    {
        const auto start = _i;
        const char prefix = static_cast<char>(std::tolower(static_cast<unsigned char>(peek(1))));
        if (_expr[_i] == '0' and (prefix == 'x' or prefix == 'o' or prefix == 'b'))
        {
            const int base = (prefix == 'x')? 16 : (prefix == 'o')? 8 : 2;
            _i += 2;
            const auto text = this->digits(start, hexValue, base, true);
            if (text.empty() or (_i < _expr.size() and isNameChar(_expr[_i]))) fail("invalid number literal", start);
            std::int64_t value = 0;
            const auto result = std::from_chars(text.data(), text.data()+text.size(), value, base);
            if (result.ec != std::errc()) fail("integer literal is too large", start);
            tok.kind = Tok::Int;
            tok.literal = Value(value);
            return;
        }

        const auto intPart = this->digits(start, hexValue, 10);
        std::string fracPart, expPart;
        bool isFloat = false;
        if (peek(0) == '.')
        {
            isFloat = true;
            _i++;
            fracPart = this->digits(start, hexValue, 10);
        }
        if (peek(0) == 'e' or peek(0) == 'E')
        {
            isFloat = true;
            _i++;
            if (peek(0) == '+' or peek(0) == '-') expPart += _expr[_i++];
            const auto expDigits = this->digits(start, hexValue, 10);
            if (expDigits.empty()) fail("invalid number literal", start);
            expPart += expDigits;
        }
        const bool isImag = peek(0) == 'j' or peek(0) == 'J';
        if (isImag) _i++;
        if (_i < _expr.size() and isNameChar(_expr[_i])) fail("invalid number literal", start);

        if (isImag or isFloat)
        {
            const double value = parseDecimalFloat(intPart, fracPart, expPart);
            tok.kind = isImag? Tok::Imag : Tok::Float;
            tok.literal = isImag? Value(std::complex<double>(0.0, value)) : Value(value);
            return;
        }

        if (intPart.size() > 1 and intPart.front() == '0' and intPart.find_first_not_of('0') != std::string::npos)
        {
            fail("leading zeros in a decimal literal are not allowed (use 0o for octal)", start);
        }
        std::int64_t value = 0;
        const auto result = std::from_chars(intPart.data(), intPart.data()+intPart.size(), value);
        tok.kind = Tok::Int;
        if (result.ec == std::errc()) tok.literal = Value(value);
        else if (intPart == "9223372036854775808") //-9223372036854775808 is printed for INT64_MIN (JSON)
        {
            tok.literal = Value(std::numeric_limits<std::int64_t>::min());
            tok.needsMinus = true;
        }
        else fail("integer literal is too large", start);
    }

    static void appendUtf8(std::string &out, const std::uint32_t cp)
    {
        if (cp < 0x80) out += char(cp);
        else if (cp < 0x800)
        {
            out += char(0xC0 | (cp >> 6));
            out += char(0x80 | (cp & 0x3F));
        }
        else if (cp < 0x10000)
        {
            out += char(0xE0 | (cp >> 12));
            out += char(0x80 | ((cp >> 6) & 0x3F));
            out += char(0x80 | (cp & 0x3F));
        }
        else
        {
            out += char(0xF0 | (cp >> 18));
            out += char(0x80 | ((cp >> 12) & 0x3F));
            out += char(0x80 | ((cp >> 6) & 0x3F));
            out += char(0x80 | (cp & 0x3F));
        }
    }

    //Read n hex digits after _i, or -1 when they are missing
    long hexDigits(const std::size_t offset, const int n) const
    {
        long value = 0;
        for (int k = 0; k < n; k++)
        {
            const int h = hexValue(peek(offset+k));
            if (h < 0) return -1;
            value = value*16 + h;
        }
        return value;
    }

    void string(Token &tok)
    {
        const auto start = _i;
        const char quote = _expr[_i++];
        std::string out;
        while (true)
        {
            if (_i >= _expr.size()) fail("unterminated string literal", start);
            const char ch = _expr[_i];
            if (ch == quote)
            {
                _i++;
                break;
            }
            if (ch != '\\')
            {
                out += ch;
                _i++;
                continue;
            }

            const auto esc = _i;
            if (esc+1 >= _expr.size()) fail("unterminated string literal", start);
            switch (_expr[esc+1])
            {
            case '"': out += '"'; break;
            case '\'': out += '\''; break;
            case '\\': out += '\\'; break;
            case '/': out += '/'; break;
            case 'n': out += '\n'; break;
            case 't': out += '\t'; break;
            case 'r': out += '\r'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case '0': out += '\0'; break;
            case 'x':
            {
                const auto value = this->hexDigits(2, 2);
                if (value < 0) fail("invalid \\x escape", esc);
                out += char(value);
                _i += 2;
                break;
            }
            case 'u':
            {
                auto cp = this->hexDigits(2, 4);
                if (cp < 0) fail("invalid \\u escape", esc);
                _i += 4;
                if (cp >= 0xDC00 and cp <= 0xDFFF) fail("unpaired surrogate in \\u escape", esc);
                if (cp >= 0xD800 and cp <= 0xDBFF)
                {
                    const long low = (peek(2) == '\\' and peek(3) == 'u')? this->hexDigits(4, 4) : -1;
                    if (low < 0xDC00 or low > 0xDFFF) fail("unpaired surrogate in \\u escape", esc);
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                    _i += 6;
                }
                appendUtf8(out, std::uint32_t(cp));
                break;
            }
            default: fail("invalid escape sequence '\\" + std::string(1, _expr[esc+1]) + "'", esc);
            }
            _i += 2;
        }
        tok.kind = Tok::String;
        tok.literal = Value(std::move(out));
    }

    const std::string_view _expr;
    std::size_t _i = 0;
};

} //namespace

double Pothos::Util::Expr::parseDecimalFloat(const std::string_view intPart, const std::string_view fracPart, const std::string_view expPart)
{
    std::string s(intPart.empty()? "0" : intPart);
    if (not fracPart.empty()) s += "." + std::string(fracPart);
    if (not expPart.empty()) s += "e" + std::string(expPart);

#if defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 201611L
    double value = 0.0;
    const auto result = std::from_chars(s.data(), s.data()+s.size(), value);
    if (result.ec != std::errc::result_out_of_range) return value;

    //too small underflows to zero, too large overflows to infinity (like Python)
    return expPart.starts_with('-')? 0.0 : std::numeric_limits<double>::infinity();
#else
    //no floating point from_chars (Apple libc++)
    return parseFloatStream(s);
#endif
}

double Pothos::Util::Expr::parseFloatStream(const std::string &text)
{
    std::istringstream is(text);
    is.imbue(std::locale::classic());
    double value = 0.0;
    is >> value;
    if (not is.fail()) return value;

    //out of range: decide from the stored value, not from the failbit alone --
    //libc++ also fails for subnormal results and keeps the correct value then
    if (std::isinf(value) or std::fabs(value) == std::numeric_limits<double>::max())
    {
        return std::copysign(std::numeric_limits<double>::infinity(), value);
    }
    return value;
}

std::vector<Token> Pothos::Util::Expr::tokenize(const std::string_view expr)
{
    return Lexer(expr).run();
}
