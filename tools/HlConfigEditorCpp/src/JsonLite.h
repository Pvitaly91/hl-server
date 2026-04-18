#pragma once

#include <map>
#include <string>

namespace hlcfg {

class JsonValue {
public:
    enum class Type {
        Null,
        Bool,
        Number,
        String,
        Object,
    };

    using Object = std::map<std::wstring, JsonValue>;

    JsonValue();
    JsonValue(bool value);
    JsonValue(double value);
    JsonValue(const std::wstring& value);
    JsonValue(std::wstring&& value);

    static JsonValue MakeObject();

    Type GetType() const;
    bool IsNull() const;
    bool IsBool() const;
    bool IsNumber() const;
    bool IsString() const;
    bool IsObject() const;

    bool AsBool() const;
    double AsNumber() const;
    const std::wstring& AsString() const;
    const Object& AsObject() const;
    Object& AsObject();

private:
    Type type_ = Type::Null;
    bool boolValue_ = false;
    double numberValue_ = 0.0;
    std::wstring stringValue_;
    Object objectValue_;
};

bool ParseJsonText(const std::wstring& text, JsonValue& value, std::wstring& errorMessage);
std::wstring SerializeJsonText(const JsonValue& value);

}  // namespace hlcfg
