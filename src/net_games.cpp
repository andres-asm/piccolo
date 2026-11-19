#include "net_games.h"

#include <cstdio>
#include <cstring>

#include "content_archive.h"
#include "host_log.h"
#include "net_link.h"

void net_games_init(NetGames* games)
{
    games->reply.resize(NET_LINK_MAX_PAYLOAD);
}

void net_games_set_folders(NetGames* games, const std::vector<std::string>* game_folders)
{
    if (*game_folders == games->folders_from && !games->folders_from.empty())
    {
        return;
    }
    games->folders_from = *game_folders;
    net_content_set_folders(&games->folders, game_folders);
}

static void make_cores(NetGames* games, const CoreCatalog* catalog)
{
    games->cores.clear();
    for (int32_t i = 0; i < catalog->count; i++)
    {
        const CoreCatalogEntry* entry = &catalog->entries[i];
        if (!entry->info.usable)
        {
            continue;   // as LOAD GAME: only cores that load
        }
        NetContentCore core;
        core.flags = entry->info.supports_no_game ? NET_CONTENT_CORE_NO_GAME : 0;
        core.file = entry->file_name;
        core.name = entry->info.name;
        core.version = entry->info.version;
        core.extensions = entry->info.extensions;
        games->cores.push_back(core);
    }
}

size_t net_games_answer(NetGames* games, const CoreCatalog* catalog, const uint8_t* data, size_t size)
{
    uint32_t id = 0;
    uint32_t start = 0;
    if (net_content_read_cores_request(data, size, &id, &start))
    {
        if (start == 0 || games->cores.empty())
        {
            make_cores(games, catalog);
        }
        return net_content_write_cores_result(games->reply.data(), games->reply.size(), id, NetContentStatus::ok,
                                              &games->cores, start);
    }
    return net_content_answer_list(&games->folders, data, size, games->reply.data(), games->reply.size());
}

static const CoreCatalogEntry* find_core(const CoreCatalog* catalog, const std::string* file)
{
    for (int32_t i = 0; i < catalog->count; i++)
    {
        if (catalog->entries[i].info.usable && *file == catalog->entries[i].file_name)
        {
            return &catalog->entries[i];
        }
    }
    return nullptr;
}

bool net_games_load(const NetGames* games, const CoreCatalog* catalog, const uint8_t* data, size_t size, uint32_t* id,
                    NetContentStatus* status, FilePickerChoice* choice)
{
    std::string core_file;
    std::string path;
    if (!net_content_read_load_request(data, size, id, &core_file, &path))
    {
        return false;
    }
    *choice = FilePickerChoice{};
    const CoreCatalogEntry* core = find_core(catalog, &core_file);
    if (core == nullptr || !core_catalog_path(catalog, core, choice->core_path, sizeof(choice->core_path)))
    {
        *status = NetContentStatus::not_found;
        return true;
    }
    if (path.empty())
    {
        *status = core->info.supports_no_game ? NetContentStatus::ok : NetContentStatus::refused;
        return true;
    }

    std::string real_path;
    NetContentKind kind = NetContentKind::folder;
    *status = net_content_resolve(&games->folders, path.c_str(), &real_path, &kind);
    if (*status != NetContentStatus::ok)
    {
        return true;
    }
    if (kind == NetContentKind::folder)
    {
        *status = NetContentStatus::refused;
        return true;
    }
    if (kind == NetContentKind::archive && !content_archive_core_takes_whole(&core->info, real_path.c_str()))
    {
        // LOAD on an archive in LOAD GAME: the one file inside this core loads.
        std::vector<ArchiveEntry> entries;
        std::string single;
        if (!archive_list(real_path.c_str(), &entries))
        {
            *status = NetContentStatus::failed;
            return true;
        }
        if (content_archive_loadable(&core->info, &entries, &single) != 1)
        {
            host_log(LogLevel::info, "Network: %s holds no single game for %s: browse it instead", path.c_str(),
                     core->info.name);
            *status = NetContentStatus::refused;
            return true;
        }
        real_path += "#" + single;
    }
    int written = std::snprintf(choice->content_path, sizeof(choice->content_path), "%s", real_path.c_str());
    if (written < 0 || static_cast<size_t>(written) >= sizeof(choice->content_path))
    {
        *status = NetContentStatus::refused;
    }
    return true;
}
