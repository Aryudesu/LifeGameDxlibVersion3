#pragma once

#include <map>
#include <string>

struct PatternMetadata {
    bool favorite = false;
    std::string category;
};

class PatternMetadataStore {
public:
    bool load(std::string& errorMessage);

    bool isFavorite(const std::string& key) const;
    const std::string& category(const std::string& key) const;

    bool setFavorite(const std::string& key, bool favorite, std::string& errorMessage);
    bool setCategory(const std::string& key, const std::string& category, std::string& errorMessage);
    bool renameKey(const std::string& oldKey, const std::string& newKey, std::string& errorMessage);
    bool removeKey(const std::string& key, std::string& errorMessage);

private:
    bool save(std::string& errorMessage) const;
    void pruneEmpty(const std::string& key);

    std::map<std::string, PatternMetadata> entries_;
};
