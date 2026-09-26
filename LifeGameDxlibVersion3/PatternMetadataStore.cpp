#include "PatternMetadataStore.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string_view>
#include <utility>
#include <windows.h>

namespace {
namespace fs = std::filesystem;

const fs::path MetadataPath = fs::path("patterns") / "metadata.json";

std::string ansiFromWide(const wchar_t* text) {
    const int size = WideCharToMultiByte(CP_ACP, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1) return {};

    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_ACP, 0, text, -1, result.data(), size, nullptr, nullptr);
    result.pop_back();
    return result;
}

std::wstring wideFromAnsi(const std::string& text) {
    if (text.empty()) return {};
    const int size = MultiByteToWideChar(CP_ACP, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (size <= 0) return {};

    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_ACP, 0, text.data(), static_cast<int>(text.size()), result.data(), size);
    return result;
}

std::string ansiFromWideString(const std::wstring& text) {
    if (text.empty()) return {};
    const int size = WideCharToMultiByte(
        CP_ACP, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};

    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(
        CP_ACP, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
    return result;
}

void appendJsonString(std::ostringstream& out, const std::string& text) {
    const std::wstring wide = wideFromAnsi(text);
    out << '"';
    for (const wchar_t ch : wide) {
        switch (ch) {
        case L'"': out << "\\\""; break;
        case L'\\': out << "\\\\"; break;
        case L'\b': out << "\\b"; break;
        case L'\f': out << "\\f"; break;
        case L'\n': out << "\\n"; break;
        case L'\r': out << "\\r"; break;
        case L'\t': out << "\\t"; break;
        default:
            if (ch >= 0x20 && ch <= 0x7e) {
                out << static_cast<char>(ch);
            } else {
                out << "\\u"
                    << std::uppercase << std::hex << std::setw(4) << std::setfill('0')
                    << static_cast<unsigned int>(ch)
                    << std::nouppercase << std::dec << std::setw(0) << std::setfill(' ');
            }
            break;
        }
    }
    out << '"';
}

class JsonReader {
public:
    explicit JsonReader(std::string_view text) : text_(text) {}

    bool parse(std::map<std::string, PatternMetadata>& entries) {
        skipSpace();
        if (!consume('{')) return false;

        bool sawVersion = false;
        bool sawPatterns = false;
        skipSpace();
        if (consume('}')) return true;

        while (true) {
            std::wstring key;
            if (!parseString(key) || !consumeAfterSpace(':')) return false;

            if (key == L"version") {
                int version = 0;
                if (!parseInt(version) || version != 1) return false;
                sawVersion = true;
            } else if (key == L"patterns") {
                if (!parsePatterns(entries)) return false;
                sawPatterns = true;
            } else if (!skipValue()) {
                return false;
            }

            skipSpace();
            if (consume('}')) break;
            if (!consume(',')) return false;
            skipSpace();
        }

        skipSpace();
        return pos_ == text_.size() && sawVersion && sawPatterns;
    }

private:
    void skipSpace() {
        while (pos_ < text_.size() &&
               std::isspace(static_cast<unsigned char>(text_[pos_]))) {
            ++pos_;
        }
    }

    bool consume(char expected) {
        if (pos_ >= text_.size() || text_[pos_] != expected) return false;
        ++pos_;
        return true;
    }

    bool consumeAfterSpace(char expected) {
        skipSpace();
        if (!consume(expected)) return false;
        skipSpace();
        return true;
    }

    static int hexValue(char ch) {
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'a' && ch <= 'f') return 10 + ch - 'a';
        if (ch >= 'A' && ch <= 'F') return 10 + ch - 'A';
        return -1;
    }

    bool parseString(std::wstring& value) {
        skipSpace();
        if (!consume('"')) return false;

        value.clear();
        while (pos_ < text_.size()) {
            const char ch = text_[pos_++];
            if (ch == '"') return true;
            if (ch != '\\') {
                if (static_cast<unsigned char>(ch) < 0x20) return false;
                value.push_back(static_cast<unsigned char>(ch));
                continue;
            }

            if (pos_ >= text_.size()) return false;
            const char escaped = text_[pos_++];
            switch (escaped) {
            case '"': value.push_back(L'"'); break;
            case '\\': value.push_back(L'\\'); break;
            case '/': value.push_back(L'/'); break;
            case 'b': value.push_back(L'\b'); break;
            case 'f': value.push_back(L'\f'); break;
            case 'n': value.push_back(L'\n'); break;
            case 'r': value.push_back(L'\r'); break;
            case 't': value.push_back(L'\t'); break;
            case 'u': {
                if (pos_ + 4 > text_.size()) return false;
                unsigned int code = 0;
                for (int i = 0; i < 4; ++i) {
                    const int digit = hexValue(text_[pos_++]);
                    if (digit < 0) return false;
                    code = (code << 4) | static_cast<unsigned int>(digit);
                }
                value.push_back(static_cast<wchar_t>(code));
                break;
            }
            default:
                return false;
            }
        }
        return false;
    }

    bool parseBool(bool& value) {
        skipSpace();
        if (text_.substr(pos_, 4) == "true") {
            pos_ += 4;
            value = true;
            return true;
        }
        if (text_.substr(pos_, 5) == "false") {
            pos_ += 5;
            value = false;
            return true;
        }
        return false;
    }

    bool parseInt(int& value) {
        skipSpace();
        if (pos_ >= text_.size()) return false;

        int result = 0;
        bool hasDigit = false;
        while (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9') {
            hasDigit = true;
            result = result * 10 + (text_[pos_] - '0');
            ++pos_;
        }
        if (!hasDigit) return false;
        value = result;
        return true;
    }

    bool parsePatterns(std::map<std::string, PatternMetadata>& entries) {
        skipSpace();
        if (!consume('{')) return false;
        skipSpace();
        if (consume('}')) return true;

        while (true) {
            std::wstring wideKey;
            if (!parseString(wideKey) || !consumeAfterSpace(':')) return false;

            PatternMetadata metadata;
            if (!parseMetadata(metadata)) return false;
            if (metadata.favorite || !metadata.category.empty()) {
                entries[ansiFromWideString(wideKey)] = std::move(metadata);
            }

            skipSpace();
            if (consume('}')) return true;
            if (!consume(',')) return false;
            skipSpace();
        }
    }

    bool parseMetadata(PatternMetadata& metadata) {
        skipSpace();
        if (!consume('{')) return false;
        skipSpace();
        if (consume('}')) return true;

        while (true) {
            std::wstring key;
            if (!parseString(key) || !consumeAfterSpace(':')) return false;

            if (key == L"favorite") {
                if (!parseBool(metadata.favorite)) return false;
            } else if (key == L"category") {
                std::wstring category;
                if (!parseString(category)) return false;
                metadata.category = ansiFromWideString(category);
            } else if (!skipValue()) {
                return false;
            }

            skipSpace();
            if (consume('}')) return true;
            if (!consume(',')) return false;
            skipSpace();
        }
    }

    bool skipValue() {
        skipSpace();
        if (pos_ >= text_.size()) return false;

        if (text_[pos_] == '"') {
            std::wstring ignored;
            return parseString(ignored);
        }
        if (text_[pos_] == '{') {
            ++pos_;
            skipSpace();
            if (consume('}')) return true;
            while (true) {
                std::wstring key;
                if (!parseString(key) || !consumeAfterSpace(':') || !skipValue()) return false;
                skipSpace();
                if (consume('}')) return true;
                if (!consume(',')) return false;
                skipSpace();
            }
        }
        if (text_[pos_] == '[') {
            ++pos_;
            skipSpace();
            if (consume(']')) return true;
            while (true) {
                if (!skipValue()) return false;
                skipSpace();
                if (consume(']')) return true;
                if (!consume(',')) return false;
                skipSpace();
            }
        }

        const std::size_t start = pos_;
        while (pos_ < text_.size()) {
            const char ch = text_[pos_];
            if (ch == ',' || ch == '}' || ch == ']' ||
                std::isspace(static_cast<unsigned char>(ch))) {
                break;
            }
            ++pos_;
        }
        return pos_ > start;
    }

    std::string_view text_;
    std::size_t pos_ = 0;
};

} // namespace

bool PatternMetadataStore::load(std::string& errorMessage) {
    errorMessage.clear();
    entries_.clear();

    std::error_code ec;
    fs::create_directories(MetadataPath.parent_path(), ec);
    if (ec) {
        errorMessage = ansiFromWide(
            L"patterns \u30d5\u30a9\u30eb\u30c0\u30fc\u3092\u4f5c\u6210\u3067\u304d\u307e\u305b\u3093\u3067\u3057\u305f\u3002");
        return false;
    }

    if (!fs::exists(MetadataPath, ec)) {
        if (ec) {
            errorMessage = ansiFromWide(
                L"patterns/metadata.json \u306e\u5b58\u5728\u78ba\u8a8d\u306b\u5931\u6557\u3057\u307e\u3057\u305f\u3002");
            return false;
        }
        return true;
    }

    std::ifstream input(MetadataPath, std::ios::binary);
    if (!input) {
        errorMessage = ansiFromWide(
            L"patterns/metadata.json \u3092\u958b\u3051\u307e\u305b\u3093\u3067\u3057\u305f\u3002");
        return false;
    }

    const std::string source((std::istreambuf_iterator<char>(input)),
                             std::istreambuf_iterator<char>());
    JsonReader reader(source);
    if (!reader.parse(entries_)) {
        entries_.clear();
        errorMessage = ansiFromWide(
            L"patterns/metadata.json \u306e\u5f62\u5f0f\u304c\u4e0d\u6b63\u3067\u3059\u3002");
        return false;
    }
    return true;
}

bool PatternMetadataStore::isFavorite(const std::string& key) const {
    const auto it = entries_.find(key);
    return it != entries_.end() && it->second.favorite;
}

const std::string& PatternMetadataStore::category(const std::string& key) const {
    static const std::string Empty;
    const auto it = entries_.find(key);
    return it == entries_.end() ? Empty : it->second.category;
}

bool PatternMetadataStore::setFavorite(
    const std::string& key, bool favorite, std::string& errorMessage) {
    const auto backup = entries_;
    entries_[key].favorite = favorite;
    pruneEmpty(key);
    if (save(errorMessage)) return true;
    entries_ = backup;
    return false;
}

bool PatternMetadataStore::setCategory(
    const std::string& key, const std::string& categoryValue, std::string& errorMessage) {
    const auto backup = entries_;
    entries_[key].category = categoryValue;
    pruneEmpty(key);
    if (save(errorMessage)) return true;
    entries_ = backup;
    return false;
}

bool PatternMetadataStore::renameKey(
    const std::string& oldKey, const std::string& newKey, std::string& errorMessage) {
    if (oldKey == newKey) {
        errorMessage.clear();
        return true;
    }

    const auto it = entries_.find(oldKey);
    if (it == entries_.end()) {
        errorMessage.clear();
        return true;
    }

    const auto backup = entries_;
    entries_[newKey] = it->second;
    entries_.erase(oldKey);
    if (save(errorMessage)) return true;
    entries_ = backup;
    return false;
}

bool PatternMetadataStore::removeKey(const std::string& key, std::string& errorMessage) {
    const auto it = entries_.find(key);
    if (it == entries_.end()) {
        errorMessage.clear();
        return true;
    }

    const auto backup = entries_;
    entries_.erase(it);
    if (save(errorMessage)) return true;
    entries_ = backup;
    return false;
}

bool PatternMetadataStore::save(std::string& errorMessage) const {
    errorMessage.clear();

    std::error_code ec;
    fs::create_directories(MetadataPath.parent_path(), ec);
    if (ec) {
        errorMessage = ansiFromWide(
            L"patterns \u30d5\u30a9\u30eb\u30c0\u30fc\u3092\u4f5c\u6210\u3067\u304d\u307e\u305b\u3093\u3067\u3057\u305f\u3002");
        return false;
    }

    std::ostringstream json;
    json << "{\n  \"version\": 1,\n  \"patterns\": {";
    if (!entries_.empty()) json << '\n';

    bool first = true;
    for (const auto& [key, metadata] : entries_) {
        if (!first) json << ",\n";
        first = false;

        json << "    ";
        appendJsonString(json, key);
        json << ": {\"favorite\": " << (metadata.favorite ? "true" : "false");
        if (!metadata.category.empty()) {
            json << ", \"category\": ";
            appendJsonString(json, metadata.category);
        }
        json << '}';
    }

    if (!entries_.empty()) json << '\n';
    json << "  }\n}\n";

    std::ofstream output(MetadataPath, std::ios::binary | std::ios::trunc);
    if (!output) {
        errorMessage = ansiFromWide(
            L"patterns/metadata.json \u3092\u4f5c\u6210\u3067\u304d\u307e\u305b\u3093\u3067\u3057\u305f\u3002");
        return false;
    }

    output << json.str();
    output.close();
    if (!output) {
        errorMessage = ansiFromWide(
            L"patterns/metadata.json \u306e\u66f8\u304d\u8fbc\u307f\u306b\u5931\u6557\u3057\u307e\u3057\u305f\u3002");
        return false;
    }
    return true;
}

void PatternMetadataStore::pruneEmpty(const std::string& key) {
    const auto it = entries_.find(key);
    if (it == entries_.end()) return;
    if (!it->second.favorite && it->second.category.empty()) entries_.erase(it);
}
