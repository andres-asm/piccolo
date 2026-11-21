#include "measure_common.h"

#include <cstdio>
#include <vector>

#include "archive.h"

uint16_t measure_script(uint32_t frame)
{
    uint32_t t = frame % 90;
    if (frame < 20 * 60)
    {
        if (t < 6)
        {
            return 1u << 3;   // START
        }
        if (t >= 45 && t < 51)
        {
            return 1u << 8;   // A
        }
        return 0;
    }
    uint16_t buttons = static_cast<uint16_t>((1u << 7) | (1u << 1));   // RIGHT, Y
    if (frame % 30 < 10)
    {
        buttons = static_cast<uint16_t>(buttons | (1u << 0));   // B
    }
    return buttons;
}

bool measure_content(const char* game, const std::string* scratch, std::string* content)
{
    *content = game;
    if (archive_format(game) == ArchiveFormat::none)
    {
        return true;
    }
    std::vector<ArchiveEntry> entries;
    if (!archive_list(game, &entries))
    {
        return false;
    }
    size_t first = 0;
    while (first < entries.size() && entries[first].is_directory)
    {
        first++;
    }
    if (first >= entries.size())
    {
        std::printf("nothing to load in %s\n", game);
        return false;
    }
    const std::string* name = &entries[first].name;
    size_t slash = name->rfind('/');
    *content = *scratch + "/" + (slash == std::string::npos ? *name : name->substr(slash + 1));
    return archive_extract(game, name->c_str(), content->c_str());
}
