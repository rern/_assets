#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

struct MetadataResult
{
    bool hasLyrics   = false;
    bool hasAlbumArt = false;
};

static bool findTag(
    const uint8_t* data,
    size_t size,
    const char* tag,
    size_t len)
{
    if (size < len)
        return false;

    const uint8_t first =
        (uint8_t)tag[0];

    for (size_t i = 0; i <= size - len; ++i)
    {
        //
        // fast first-byte reject
        //

        if (data[i] != first)
            continue;

        //
        // compare remaining bytes
        //

        if (!memcmp(data + i, tag, len))
            return true;
    }

    return false;
}

static bool readBlock(
    std::ifstream& f,
    std::vector<uint8_t>& buf,
    std::streamoff offset,
    size_t bytes)
{
    f.seekg(0, std::ios::end);

    std::streamoff fileSize =
        f.tellg();

    if (offset > fileSize)
        return false;

    f.seekg(offset, std::ios::beg);

    buf.resize(bytes);

    f.read((char*)buf.data(), bytes);

    buf.resize(f.gcount());

    return !buf.empty();
}

MetadataResult scanMetadata(
    const std::string& path)
{
    MetadataResult r;

    std::ifstream f(path, std::ios::binary);

    if (!f)
        return r;

    //
    // =========
    // HEADER SCAN
    // =========
    //
    // Small fast read first
    //

    std::vector<uint8_t> head;

    if (!readBlock(f, head, 0, 64 * 1024))
        return r;

    const uint8_t* h =
        head.data();

    size_t hs =
        head.size();

    //
    // ===== MP3 =====
    //

    if (findTag(h, hs, "USLT", 4) ||
        findTag(h, hs, "SYLT", 4))
    {
        r.hasLyrics = true;
    }

    if (findTag(h, hs, "APIC", 4))
    {
        r.hasAlbumArt = true;
    }

    //
    // ===== FLAC / OGG =====
    //

    if (findTag(h, hs, "LYRICS=", 7) ||
        findTag(h, hs,
            "UNSYNCEDLYRICS", 16))
    {
        r.hasLyrics = true;
    }

    if (findTag(h, hs,
        "METADATA_BLOCK_PICTURE", 22))
    {
        r.hasAlbumArt = true;
    }

    //
    // ===== MP4/M4A =====
    //
    // metadata may be near end
    //

    bool needsTailScan =
        findTag(h, hs, "ftyp", 4);

    //
    // ===== DSF =====
    //
    // ID3 often appended at tail
    //

    if (findTag(h, hs, "DSD ", 4))
    {
        needsTailScan = true;
    }

    //
    // EARLY EXIT
    //

    if (r.hasLyrics &&
        r.hasAlbumArt)
    {
        return r;
    }

    //
    // =========
    // TAIL SCAN
    // =========
    //

    if (!needsTailScan)
        return r;

    f.seekg(0, std::ios::end);

    std::streamoff fileSize =
        f.tellg();

    std::streamoff tailOffset =
        std::max<std::streamoff>(
            0,
            fileSize - (64 * 1024));

    std::vector<uint8_t> tail;

    if (!readBlock(
            f,
            tail,
            tailOffset,
            64 * 1024))
    {
        return r;
    }

    const uint8_t* t =
        tail.data();

    size_t ts =
        tail.size();

    //
    // MP4 lyrics atom
    //

    const char lyr[] =
    {
        char(0xA9),
        'l',
        'y',
        'r'
    };

    if (!r.hasLyrics &&
        findTag(t, ts, lyr, 4))
    {
        r.hasLyrics = true;
    }

    //
    // MP4 cover art
    //

    if (!r.hasAlbumArt &&
        findTag(t, ts, "covr", 4))
    {
        r.hasAlbumArt = true;
    }

    //
    // DSF ID3 fallback
    //

    if (!r.hasLyrics)
    {
        if (findTag(t, ts, "USLT", 4) ||
            findTag(t, ts, "SYLT", 4))
        {
            r.hasLyrics = true;
        }
    }

    if (!r.hasAlbumArt)
    {
        if (findTag(t, ts, "APIC", 4))
        {
            r.hasAlbumArt = true;
        }
    }

    return r;
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cout
            << "usage: file\n";

        return 0;
    }

    MetadataResult r =
        scanMetadata(argv[1]);

    std::cout
        << "lyrics   : "
        << (r.hasLyrics
            ? "yes"
            : "no")
        << "\n";

    std::cout
        << "album art: "
        << (r.hasAlbumArt
            ? "yes"
            : "no")
        << "\n";
}