#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace tauripp {

struct AssetResponse {
    int status = 404;
    std::string mime_type;
    std::string content_range;
    std::string content_security_policy;
    std::vector<unsigned char> body;
};

class AssetStore {
public:
    explicit AssetStore(std::filesystem::path root);

    AssetResponse serve(std::string_view request_path, std::string_view range_header = {}) const;

private:
    bool resolve(std::string_view request_path, std::filesystem::path& path) const;
    static std::string mime_type(const std::filesystem::path& path);

    std::filesystem::path root_;
};

} // namespace tauripp