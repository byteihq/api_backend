#include "resource_handler.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <unordered_map>

namespace app {
IResource::Extension IResource::GetExtension() const noexcept {
    return extension_;
}
const IResource::Data& IResource::GetData() const noexcept { return data_; }

bool IResource::Load(const fs::path& path) noexcept {
    static_assert(std::is_same_v<byte, Reader::char_type>);

    Reader file(path, std::ios::binary);
    if (!file) return false;
    file.unsetf(std::ios::skipws);
    data_.reserve(fs::file_size(path));
    std::copy(std::istream_iterator<Reader::char_type>(file),
              std::istream_iterator<Reader::char_type>(),
              std::back_inserter(data_));

    return true;
}

void IResource::DefineExtension(const fs::path& path) noexcept {
    if (!path.has_extension()) return;

    auto ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](char c) { return std::tolower(c); });

    static const std::unordered_map<std::string, Extension> ext_map = {
        {".htm", Extension::HTM},   {".html", Extension::HTML},
        {".css", Extension::CSS},   {".txt", Extension::TXT},
        {".js", Extension::JS},     {".json", Extension::JSON},
        {".xml", Extension::XML},   {".png", Extension::PNG},
        {".jpg", Extension::JPG},   {".jpe", Extension::JPE},
        {".jpeg", Extension::JPEG}, {".gif", Extension::GIF},
        {".bmp", Extension::BMP},   {".ico", Extension::ICO},
        {".tiff", Extension::TIFF}, {".tif", Extension::TIF},
        {".svg", Extension::SVG},   {".svgz", Extension::SVGZ},
        {".mp3", Extension::MP3}};

    if (!ext_map.count(ext)) return;
    extension_ = ext_map.at(ext);
}

StaticResource::StaticResource(const fs::path& path) {
    if (!Load(path)) throw std::runtime_error("file not found");
    DefineExtension(path);
}

ResourceHandler::ResourceHandler(const fs::path& path)
    : base_{fs::absolute(fs::weakly_canonical(path))} {}

bool ResourceHandler::IsSubPath(fs::path path) const {
    path = base_ / path;
    path = fs::weakly_canonical(path);

    for (auto b = base_.begin(), p = path.begin(); b != base_.end(); ++b, ++p) {
        if (p == path.end() || *p != *b) {
            return false;
        }
    }
    return true;
}

fs::path ResourceHandler::GetAbsolutePath(fs::path path) const {
    if (!path.has_filename()) path /= "index.html";
    return fs::absolute(base_ / path);
}
}  // namespace app
