#include "tauripp/assets.hpp"

#include <algorithm>
#include <cassert>
#include <string>

int main() {
    const tauripp::AssetStore assets("assets/hello");

    const auto page = assets.serve("/index.html");
    assert(page.status == 200);
    assert(page.mime_type == "text/html; charset=utf-8");
    assert(!page.body.empty());
    assert(page.content_security_policy.find("connect-src") != std::string::npos);

    const std::string page_text(page.body.begin(), page.body.end());
    assert(page_text.find("id=\"form\"") != std::string::npos);
    assert(page_text.find("id=\"message\"") != std::string::npos);

    const auto range = assets.serve("index.html", "bytes=0-9");
    assert(range.status == 206);
    assert(range.body.size() == 10);
    assert(range.content_range == "bytes 0-9/" + std::to_string(page.body.size()));

    assert(assets.serve("../index.html").status == 403);
    assert(assets.serve("/%2e%2e/index.html").status == 403);
    assert(assets.serve("missing.js").status == 404);
    return 0;
}