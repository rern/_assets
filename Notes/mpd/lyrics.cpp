#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

static bool findTag(const uint8_t* data, size_t size, const char* tag, size_t len)
{
    if (size < len) return false;

    const uint8_t first = (uint8_t)tag[0];

    for (size_t i = 0; i <= size - len; ++i)
    {
        if (data[i] != first) continue;
        if (!memcmp(data + i, tag, len)) return true;
    }
    return false;
}

static bool readBlock(std::ifstream& f, std::vector<uint8_t>& buf, std::streamoff offset, size_t bytes)
{
    f.seekg(0, std::ios::end);
    std::streamoff size = f.tellg();

    if (offset > size) return false;

    f.seekg(offset, std::ios::beg);

    buf.resize(bytes);
    f.read((char*)buf.data(), bytes);

    buf.resize(f.gcount());
    return !buf.empty();
}

bool hasLyrics(const std::string& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    std::vector<uint8_t> head;
    if (!readBlock(f, head, 0, 64 * 1024)) return false;

    const uint8_t* h = head.data();
    size_t hs = head.size();

    // MP3 (ID3)
    if (findTag(h, hs, "USLT", 4)) return true;
    if (findTag(h, hs, "SYLT", 4)) return true;

    // FLAC / OGG
    if (findTag(h, hs, "LYRICS=", 7)) return true;
    if (findTag(h, hs, "UNSYNCEDLYRICS", 16)) return true;

    // MP4/M4A
    const char lyr[] = { char(0xA9), 'l', 'y', 'r' };
    if (findTag(h, hs, lyr, 4)) return true;

    // DSF / tail-based formats
    bool needTail = findTag(h, hs, "ftyp", 4) || findTag(h, hs, "DSD ", 4);

    if (!needTail) return false;

    std::vector<uint8_t> tail;

    f.seekg(0, std::ios::end);
    std::streamoff end = f.tellg();
    std::streamoff start = std::max<std::streamoff>(0, end - 64 * 1024);

    if (!readBlock(f, tail, start, 64 * 1024)) return false;

    const uint8_t* t = tail.data();
    size_t ts = tail.size();

    if (findTag(t, ts, "USLT", 4)) return true;
    if (findTag(t, ts, "SYLT", 4)) return true;

    return false;
}

int main(int argc, char** argv)
{
    if (argc < 2) return 0;

    std::cout << (hasLyrics(argv[1]) ? "yes" : "no") << "\n";
}