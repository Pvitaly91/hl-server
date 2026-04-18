#include "JsonLite.h"

#include <cwctype>
#include <sstream>
#include <stdexcept>

namespace hlcfg {

JsonValue::JsonValue() = default;

JsonValue::JsonValue(bool value)
    : type_(Type::Bool), boolValue_(value) {}

JsonValue::JsonValue(double value)
    : type_(Type::Number), numberValue_(value) {}

JsonValue::JsonValue(const std::wstring& value)
    : type_(Type::String), stringValue_(value) {}

JsonValue::JsonValue(std::wstring&& value)
    : type_(Type::String), stringValue_(std::move(value)) {}

JsonValue JsonValue::MakeObject() {
    JsonValue value;
    value.type_ = Type::Object;
    return value;
}

JsonValue::Type JsonValue::GetType() const {
    return type_;
}

bool JsonValue::IsNull() const {
    return type_ == Type::Null;
}

bool JsonValue::IsBool() const {
    return type_ == Type::Bool;
}

bool JsonValue::IsNumber() const {
    return type_ == Type::Number;
}

bool JsonValue::IsString() const {
    return type_ == Type::String;
}

bool JsonValue::IsObject() const {
    return type_ == Type::Object;
}

bool JsonValue::AsBool() const {
    return boolValue_;
}

double JsonValue::AsNumber() const {
    return numberValue_;
}

const std::wstring& JsonValue::AsString() const {
    return stringValue_;
}

const JsonValue::Object& JsonValue::AsObject() const {
    return objectValue_;
}

JsonValue::Object& JsonValue::AsObject() {
    return objectValue_;
}

namespace {

class Parser {
public:
    explicit Parser(const std::wstring& text)
        : text_(text) {}

    bool Parse(JsonValue& value, std::wstring& errorMessage) {
        SkipWhitespace();
        if (!ParseValue(value, errorMessage)) {
            return false;
        }

        SkipWhitespace();
        if (!IsAtEnd()) {
            errorMessage = L"Unexpected trailing JSON content.";
            return false;
        }

        return true;
    }

private:
    bool ParseValue(JsonValue& value, std::wstring& errorMessage) {
        if (IsAtEnd()) {
            errorMessage = L"Unexpected end of JSON input.";
            return false;
        }

        const wchar_t ch = text_[position_];
        if (ch == L'{') {
            return ParseObject(value, errorMessage);
        }

        if (ch == L'"') {
            std::wstring parsed;
            if (!ParseString(parsed, errorMessage)) {
                return false;
            }

            value = JsonValue(std::move(parsed));
            return true;
        }

        if (ch == L't') {
            return ParseKeyword(L"true", JsonValue(true), value, errorMessage);
        }

        if (ch == L'f') {
            return ParseKeyword(L"false", JsonValue(false), value, errorMessage);
        }

        if (ch == L'n') {
            return ParseKeyword(L"null", JsonValue(), value, errorMessage);
        }

        if (ch == L'-' || std::iswdigit(ch)) {
            return ParseNumber(value, errorMessage);
        }

        errorMessage = L"Unsupported JSON token.";
        return false;
    }

    bool ParseObject(JsonValue& value, std::wstring& errorMessage) {
        Consume();
        value = JsonValue::MakeObject();
        SkipWhitespace();

        if (Match(L'}')) {
            return true;
        }

        while (!IsAtEnd()) {
            std::wstring key;
            if (!ParseString(key, errorMessage)) {
                return false;
            }

            SkipWhitespace();
            if (!Match(L':')) {
                errorMessage = L"Expected ':' after a JSON object key.";
                return false;
            }

            SkipWhitespace();
            JsonValue child;
            if (!ParseValue(child, errorMessage)) {
                return false;
            }

            value.AsObject().insert_or_assign(key, std::move(child));
            SkipWhitespace();

            if (Match(L'}')) {
                return true;
            }

            if (!Match(L',')) {
                errorMessage = L"Expected ',' between JSON object members.";
                return false;
            }

            SkipWhitespace();
        }

        errorMessage = L"Unexpected end of JSON object.";
        return false;
    }

    bool ParseString(std::wstring& value, std::wstring& errorMessage) {
        if (!Match(L'"')) {
            errorMessage = L"Expected a JSON string.";
            return false;
        }

        std::wstring result;
        while (!IsAtEnd()) {
            const wchar_t ch = Consume();
            if (ch == L'"') {
                value = std::move(result);
                return true;
            }

            if (ch == L'\\') {
                if (IsAtEnd()) {
                    errorMessage = L"Incomplete JSON string escape.";
                    return false;
                }

                const wchar_t escaped = Consume();
                switch (escaped) {
                case L'"':
                case L'\\':
                case L'/':
                    result.push_back(escaped);
                    break;
                case L'b':
                    result.push_back(L'\b');
                    break;
                case L'f':
                    result.push_back(L'\f');
                    break;
                case L'n':
                    result.push_back(L'\n');
                    break;
                case L'r':
                    result.push_back(L'\r');
                    break;
                case L't':
                    result.push_back(L'\t');
                    break;
                case L'u': {
                    wchar_t unicodeChar = 0;
                    if (!ParseUnicodeEscape(unicodeChar, errorMessage)) {
                        return false;
                    }

                    result.push_back(unicodeChar);
                    break;
                }
                default:
                    errorMessage = L"Unsupported JSON escape sequence.";
                    return false;
                }

                continue;
            }

            if (ch < 0x20) {
                errorMessage = L"Control characters must be escaped in JSON strings.";
                return false;
            }

            result.push_back(ch);
        }

        errorMessage = L"Unexpected end of JSON string.";
        return false;
    }

    bool ParseUnicodeEscape(wchar_t& value, std::wstring& errorMessage) {
        if (position_ + 4 > text_.size()) {
            errorMessage = L"Incomplete JSON unicode escape.";
            return false;
        }

        unsigned int result = 0;
        for (int index = 0; index < 4; ++index) {
            const wchar_t ch = Consume();
            result <<= 4;
            if (ch >= L'0' && ch <= L'9') {
                result += static_cast<unsigned int>(ch - L'0');
            } else if (ch >= L'a' && ch <= L'f') {
                result += static_cast<unsigned int>(ch - L'a' + 10);
            } else if (ch >= L'A' && ch <= L'F') {
                result += static_cast<unsigned int>(ch - L'A' + 10);
            } else {
                errorMessage = L"Invalid JSON unicode escape.";
                return false;
            }
        }

        value = static_cast<wchar_t>(result);
        return true;
    }

    bool ParseNumber(JsonValue& value, std::wstring& errorMessage) {
        const std::size_t start = position_;

        if (Peek() == L'-') {
            Consume();
        }

        if (!ConsumeDigits()) {
            errorMessage = L"Invalid JSON number.";
            return false;
        }

        if (Match(L'.') && !ConsumeDigits()) {
            errorMessage = L"Invalid JSON number fraction.";
            return false;
        }

        if (Peek() == L'e' || Peek() == L'E') {
            Consume();
            if (Peek() == L'+' || Peek() == L'-') {
                Consume();
            }

            if (!ConsumeDigits()) {
                errorMessage = L"Invalid JSON number exponent.";
                return false;
            }
        }

        try {
            const double parsedNumber = std::stod(text_.substr(start, position_ - start));
            value = JsonValue(parsedNumber);
            return true;
        } catch (const std::exception&) {
            errorMessage = L"Failed to parse JSON number.";
            return false;
        }
    }

    bool ParseKeyword(const wchar_t* keyword, const JsonValue& keywordValue, JsonValue& value, std::wstring& errorMessage) {
        const std::wstring keywordText(keyword);
        if (text_.compare(position_, keywordText.size(), keywordText) != 0) {
            errorMessage = L"Invalid JSON keyword.";
            return false;
        }

        position_ += keywordText.size();
        value = keywordValue;
        return true;
    }

    bool ConsumeDigits() {
        const std::size_t start = position_;
        while (!IsAtEnd() && std::iswdigit(text_[position_])) {
            ++position_;
        }

        return position_ > start;
    }

    wchar_t Peek() const {
        return IsAtEnd() ? 0 : text_[position_];
    }

    wchar_t Consume() {
        return text_[position_++];
    }

    bool Match(wchar_t expected) {
        if (Peek() != expected) {
            return false;
        }

        ++position_;
        return true;
    }

    bool IsAtEnd() const {
        return position_ >= text_.size();
    }

    void SkipWhitespace() {
        while (!IsAtEnd() && std::iswspace(text_[position_])) {
            ++position_;
        }
    }

    const std::wstring& text_;
    std::size_t position_ = 0;
};

std::wstring EscapeString(const std::wstring& value) {
    std::wstring escaped;
    escaped.reserve(value.size() + 8);

    for (const wchar_t ch : value) {
        switch (ch) {
        case L'"':
            escaped += L"\\\"";
            break;
        case L'\\':
            escaped += L"\\\\";
            break;
        case L'\b':
            escaped += L"\\b";
            break;
        case L'\f':
            escaped += L"\\f";
            break;
        case L'\n':
            escaped += L"\\n";
            break;
        case L'\r':
            escaped += L"\\r";
            break;
        case L'\t':
            escaped += L"\\t";
            break;
        default:
            if (ch < 0x20) {
                std::wostringstream stream;
                stream << L"\\u";
                stream.setf(std::ios::hex, std::ios::basefield);
                stream.width(4);
                stream.fill(L'0');
                stream << static_cast<unsigned int>(ch);
                escaped += stream.str();
            } else {
                escaped.push_back(ch);
            }
            break;
        }
    }

    return escaped;
}

void SerializeValue(const JsonValue& value, std::wstring& output, int indentLevel) {
    switch (value.GetType()) {
    case JsonValue::Type::Null:
        output += L"null";
        return;
    case JsonValue::Type::Bool:
        output += value.AsBool() ? L"true" : L"false";
        return;
    case JsonValue::Type::Number: {
        std::wostringstream stream;
        stream.precision(15);
        stream << value.AsNumber();
        output += stream.str();
        return;
    }
    case JsonValue::Type::String:
        output.push_back(L'"');
        output += EscapeString(value.AsString());
        output.push_back(L'"');
        return;
    case JsonValue::Type::Object: {
        output += L"{";
        if (value.AsObject().empty()) {
            output += L"}";
            return;
        }

        bool first = true;
        for (const auto& [key, child] : value.AsObject()) {
            if (!first) {
                output += L",";
            }

            output += L"\n";
            output.append(static_cast<std::size_t>((indentLevel + 1) * 2), L' ');
            output.push_back(L'"');
            output += EscapeString(key);
            output += L"\": ";
            SerializeValue(child, output, indentLevel + 1);
            first = false;
        }

        output += L"\n";
        output.append(static_cast<std::size_t>(indentLevel * 2), L' ');
        output += L"}";
        return;
    }
    }
}

}  // namespace

bool ParseJsonText(const std::wstring& text, JsonValue& value, std::wstring& errorMessage) {
    Parser parser(text);
    return parser.Parse(value, errorMessage);
}

std::wstring SerializeJsonText(const JsonValue& value) {
    std::wstring output;
    SerializeValue(value, output, 0);
    output += L"\n";
    return output;
}

}  // namespace hlcfg
