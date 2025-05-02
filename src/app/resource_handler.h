#pragma once

#include <filesystem>
#include <fstream>
#include <vector>

namespace app {
namespace fs = std::filesystem;

class IResource {
   public:
    enum class Extension : uint8_t {
        UNKNOWN = 0,
        HTM,
        HTML,
        CSS,
        TXT,
        JS,
        JSON,
        XML,
        PNG,
        JPG,
        JPE,
        JPEG,
        GIF,
        BMP,
        ICO,
        TIFF,
        TIF,
        SVG,
        SVGZ,
        MP3
    };

    using byte = char;
    using Data = std::vector<byte>;

   public:
    Extension GetExtension() const noexcept;
    const Data& GetData() const noexcept;

   protected:
    bool Load(const fs::path& path) noexcept;
    void DefineExtension(const fs::path& path) noexcept;

   protected:
    using Reader = std::ifstream;

    Data data_;
    Extension extension_{Extension::UNKNOWN};
};

class StaticResource final : public IResource {
   public:
    explicit StaticResource(const fs::path& path);
};

class ResourceHandler final {
   public:
    explicit ResourceHandler(const fs::path& path);

    bool IsSubPath(fs::path path) const;
    fs::path GetAbsolutePath(fs::path path) const;

   private:
    fs::path base_;
};
}  // namespace app
