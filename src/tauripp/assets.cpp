#include "tauripp/assets.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <fstream>
#include <system_error>

namespace tauripp {

namespace {

    constexpr char kContentSecurityPolicy[] =
        "default-src 'self'; script-src 'self' 'unsafe-inline'; connect-src 'self' ipc:;";

    bool is_hex(char character) {
        return (character >= '0' && character <= '9') ||
               (character >= 'a' && character <= 'f') ||
               (character >= 'A' && character <= 'F');
    }

    unsigned char hex_value(char character) {
        if (character >= '0' && character <= '9') {
            return static_cast<unsigned char>(character - '0');
        }
        if (character >= 'a' && character <= 'f') {
            return static_cast<unsigned char>(character - 'a' + 10);
        }
        return static_cast<unsigned char>(character - 'A' + 10);
    }

    bool percent_decode(std::string_view value, std::string& decoded) {
        decoded.clear();
        decoded.reserve(value.size());
        for (std::size_t index = 0; index < value.size(); ++index) {
            if (value[index] != '%') {
                decoded.push_back(value[index]);
                continue;
            }
            if (index + 2 >= value.size() || !is_hex(value[index + 1]) || !is_hex(value[index + 2])) {
                return false;
            }
            const char decoded_character =
                static_cast<char>((hex_value(value[index + 1]) << 4) | hex_value(value[index + 2]));
            if (decoded_character == '\0') {
                return false;
            }
            decoded.push_back(decoded_character);
            index += 2;
        }
        return true;
    }

    bool parse_range(std::string_view header, std::size_t total, std::size_t& start, std::size_t& end) {
        if (header.empty()) {
            start = 0;
            end = total == 0 ? 0 : total - 1;
            return true;
        }
        if (total == 0 || header.substr(0, 6) != "bytes=") {
            return false;
        }

        const std::string_view range = header.substr(6);
        const std::size_t separator = range.find('-');
        if (separator == std::string_view::npos || range.find(',', separator + 1) != std::string_view::npos) {
            return false;
        }

        const std::string_view first = range.substr(0, separator);
        const std::string_view second = range.substr(separator + 1);
        if (first.empty()) {
            std::size_t suffix = 0;
            const auto parsed = std::from_chars(second.data(), second.data() + second.size(), suffix);
            if (parsed.ec != std::errc {} || parsed.ptr != second.data() + second.size() || suffix == 0) {
                return false;
            }
            start = suffix >= total ? 0 : total - suffix;
            end = total - 1;
            return true;
        }

        const auto parsed_start = std::from_chars(first.data(), first.data() + first.size(), start);
        if (parsed_start.ec != std::errc {} || parsed_start.ptr != first.data() + first.size() || start >= total) {
            return false;
        }
        if (second.empty()) {
            end = total - 1;
            return true;
        }
        const auto parsed_end = std::from_chars(second.data(), second.data() + second.size(), end);
        if (parsed_end.ec != std::errc {} || parsed_end.ptr != second.data() + second.size() || end < start) {
            return false;
        }
        end = std::min(end, total - 1);
        return true;
    }

} // namespace

AssetStore::AssetStore(std::filesystem::path root) {
    std::error_code error;
    root_ = std::filesystem::weakly_canonical(root, error);
    if (error) {
        root_ = std::filesystem::absolute(root, error).lexically_normal();
    }
}

bool AssetStore::resolve(std::string_view request_path, std::filesystem::path& path) const {
    const std::size_t query_start = request_path.find_first_of("?#");
    const std::string_view path_part = request_path.substr(0, query_start);
    std::string decoded;
    if (!percent_decode(path_part, decoded)) {
        return false;
    }
    while (!decoded.empty() && decoded.front() == '/') {
        decoded.erase(decoded.begin());
    }
    if (decoded.empty()) {
        decoded = "index.html";
    }

    std::filesystem::path relative;
    std::size_t segment_start = 0;
    while (segment_start <= decoded.size()) {
        const std::size_t separator = decoded.find('/', segment_start);
        const std::size_t segment_length =
            separator == std::string::npos ? decoded.size() - segment_start : separator - segment_start;
        const std::string_view segment(decoded.data() + segment_start, segment_length);
        if (segment == ".." ||
            segment.find('\\') != std::string_view::npos ||
            segment.find(':') != std::string_view::npos) {
            return false;
        }
        if (!segment.empty() && segment != ".") {
            relative /= std::string(segment);
        }
        if (separator == std::string::npos) {
            break;
        }
        segment_start = separator + 1;
    }

    std::error_code error;
    const std::filesystem::path candidate = std::filesystem::weakly_canonical(root_ / relative, error);
    if (error) {
        return false;
    }
    const std::filesystem::path relative_to_root = std::filesystem::relative(candidate, root_, error);
    const std::string relative_text = relative_to_root.string();
    if (error ||
        relative_to_root.empty() ||
        relative_to_root == ".." ||
        (relative_text.size() >= 3 &&
         relative_text.compare(0, 3, ".." + std::string(1, std::filesystem::path::preferred_separator)) == 0)) {
        return false;
    }
    path = candidate;
    return true;
}

std::string AssetStore::mime_type(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    for (char& character : extension) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    if (extension == ".html" || extension == ".htm") {
        return "text/html; charset=utf-8";
    }
    if (extension == ".css") {
        return "text/css; charset=utf-8";
    }
    if (extension == ".js" || extension == ".mjs") {
        return "text/javascript; charset=utf-8";
    }
    if (extension == ".json") {
        return "application/json; charset=utf-8";
    }
    if (extension == ".wasm") {
        return "application/wasm";
    }
    if (extension == ".png") {
        return "image/png";
    }
    if (extension == ".jpg" || extension == ".jpeg") {
        return "image/jpeg";
    }
    if (extension == ".svg") {
        return "image/svg+xml";
    }
    if (extension == ".ico") {
        return "image/x-icon";
    }
    if (extension == ".woff") {
        return "font/woff";
    }
    if (extension == ".woff2") {
        return "font/woff2";
    }
    return "application/octet-stream";
}

AssetResponse AssetStore::serve(std::string_view request_path, std::string_view range_header) const {
    AssetResponse response;
    response.content_security_policy = kContentSecurityPolicy;

    std::filesystem::path path;
    if (!resolve(request_path, path)) {
        response.status = 403;
        return response;
    }
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error) || error) {
        response.status = 404;
        return response;
    }

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        response.status = 404;
        return response;
    }
    file.seekg(0, std::ios::end);
    const std::streamoff length = file.tellg();
    if (length < 0) {
        response.status = 500;
        return response;
    }
    file.seekg(0, std::ios::beg);
    response.body.resize(static_cast<std::size_t>(length));
    if (!response.body.empty()) {
        file.read(reinterpret_cast<char*>(response.body.data()), length);
        if (!file) {
            response.status = 500;
            response.body.clear();
            return response;
        }
    }

    response.mime_type = mime_type(path);
    std::size_t start = 0;
    std::size_t end = 0;
    if (!parse_range(range_header, response.body.size(), start, end)) {
        if (!range_header.empty()) {
            response.status = 416;
            response.body.clear();
            return response;
        }
    } else if (!range_header.empty()) {
        const auto first = response.body.begin() + static_cast<std::ptrdiff_t>(start);
        const auto last = response.body.begin() + static_cast<std::ptrdiff_t>(end + 1);
        std::vector<unsigned char> ranged(first, last);
        response.body = std::move(ranged);
        response.content_range = "bytes " +
                                 std::to_string(start) +
                                 "-" +
                                 std::to_string(end) +
                                 "/" +
                                 std::to_string(static_cast<std::size_t>(length));
        response.status = 206;
        return response;
    }

    response.status = 200;
    return response;
}

} // namespace tauripp