#include "browser/url_utils.h"

#include <cctype>
#include <iomanip>
#include <sstream>

namespace nebula::browser {
namespace {

constexpr char kSearchUrl[] = "https://www.google.com/search?q=";

std::string Trim(std::string value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
        value.erase(value.begin());
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }
    return value;
}

bool StartsWithScheme(const std::string& value) {
    return value.starts_with("http://") ||
           value.starts_with("https://") ||
           value.starts_with("file:") ||
           value.starts_with("data:") ||
           value.starts_with("blob:") ||
           value.starts_with("chrome:");
}

bool LooksLikeHostName(const std::string& value) {
    return value.find('.') != std::string::npos &&
           value.find_first_of(" \t\r\n") == std::string::npos;
}

std::string UrlEncodeSearch(const std::string& value) {
    std::ostringstream encoded;
    encoded << std::hex << std::uppercase;

    for (unsigned char ch : value) {
        if (std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~') {
            encoded << static_cast<char>(ch);
        } else if (ch == ' ') {
            encoded << '+';
        } else {
            encoded << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(ch);
        }
    }

    return encoded.str();
}

}  // namespace

std::string NormalizeNavigationInput(const std::string& input) {
    const std::string value = Trim(input);
    if (value.empty()) {
        return {};
    }

    if (StartsWithScheme(value)) {
        return value;
    }

    if (LooksLikeHostName(value)) {
        return "https://" + value;
    }

    return std::string(kSearchUrl) + UrlEncodeSearch(value);
}

std::string JsonEscape(const std::string& value) {
    std::ostringstream escaped;
    for (unsigned char ch : value) {
        switch (ch) {
            case '\\':
                escaped << "\\\\";
                break;
            case '"':
                escaped << "\\\"";
                break;
            case '\b':
                escaped << "\\b";
                break;
            case '\f':
                escaped << "\\f";
                break;
            case '\n':
                escaped << "\\n";
                break;
            case '\r':
                escaped << "\\r";
                break;
            case '\t':
                escaped << "\\t";
                break;
            default:
                if (ch < 0x20) {
                    escaped << "\\u" << std::hex << std::uppercase << std::setw(4)
                            << std::setfill('0') << static_cast<int>(ch);
                } else {
                    escaped << static_cast<char>(ch);
                }
                break;
        }
    }
    return escaped.str();
}

}  // namespace nebula::browser
