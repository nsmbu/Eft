#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

using u8 = unsigned char;
using u32 = unsigned int;

inline bool ReadFile(const char* filename, u8** out_data, u32* out_size)
{
    std::FILE* f = std::fopen(filename, "rb");
    if (!f)
        return false;

    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::rewind(f);

    if (size < 0)
    {
        std::fclose(f);
        return false;
    }

    auto* buf = static_cast<u8*>(std::malloc(static_cast<u32>(size)));
    if (!buf)
    {
        std::fclose(f);
        return false;
    }

    if (std::fread(buf, 1, static_cast<u32>(size), f) != static_cast<std::size_t>(size))
    {
        std::free(buf);
        std::fclose(f);
        return false;
    }

    std::fclose(f);
    *out_data = buf;
    *out_size = static_cast<u32>(size);
    return true;
}

inline bool WriteFile(const char* filename, u8* data, u32 size)
{
    std::FILE* f = std::fopen(filename, "wb");
    if (!f)
        return false;

    const bool ok = std::fwrite(data, 1, size, f) == size;
    std::fclose(f);
    return ok;
}

inline void FreeFile(const void* data)
{
    std::free(const_cast<void*>(data));
}

inline bool ReadFile(const std::string& filename, std::string* out_str)
{
    std::ifstream ifs(filename, std::ios::binary | std::ios::ate);
    if (!ifs)
        return false;

    const auto size = ifs.tellg();
    ifs.seekg(0);

    out_str->resize(static_cast<std::size_t>(size));
    return static_cast<bool>(ifs.read(out_str->data(), size));
}

inline bool WriteFile(const std::string& filename, const std::string& str)
{
    std::ofstream ofs(filename, std::ios::binary);
    if (!ofs)
        return false;

    return static_cast<bool>(ofs.write(str.data(), static_cast<std::streamsize>(str.size())));
}

inline bool FileExists(const char* path)
{
    return std::filesystem::exists(path);
}

inline void DeleteFile(const char* path)
{
    std::filesystem::remove(path);
}
