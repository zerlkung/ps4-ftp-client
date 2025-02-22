#include "fs.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <filesystem>
#include <stdio.h>
#include <unistd.h>

#include "util.h"
#include "lang.h"
#include "windows.h"

namespace FS
{
    int hasEndSlash(const char *path)
    {
        return path[strlen(path) - 1] == '/';
    }

    void MkDirs(const std::string &ppath, bool prev)
    {
        std::string path = ppath;
        if (!prev)
        {
            path.push_back('/');
        }
        auto ptr = path.begin();
        while (true)
        {
            ptr = std::find(ptr, path.end(), '/');
            if (ptr == path.end())
                break;

            char last = *ptr;
            *ptr = 0;
            int err = mkdir(path.c_str(), 0777);
            *ptr = last;
            ++ptr;
        }
    }

    void Rm(const std::string &file)
    {
        remove(file.c_str());
    }

    void RmDir(const std::string &path)
    {
        remove(path.c_str());
    }

    int64_t GetSize(const std::string &path)
    {
        struct stat file_stat = {0};
        int err = stat(path.c_str(), &file_stat);
        if (err < 0)
        {
            return -1;
        }
        return file_stat.st_size;
    }

    bool FileExists(const std::string &path)
    {
        struct stat file_stat = {0};
        return (stat(path.c_str(), std::addressof(file_stat)) == 0 && S_ISREG(file_stat.st_mode));
    }

    bool FolderExists(const std::string &path)
    {
        struct stat dir_stat = {0};
        return (stat(path.c_str(), &dir_stat) == 0);
    }

    void Rename(const std::string &from, const std::string &to)
    {
        int res = rename(from.c_str(), to.c_str());
    }

    FILE *Create(const std::string &path)
    {
        FILE *fd = fopen(path.c_str(), "w");

        return fd;
    }

    FILE *OpenRW(const std::string &path)
    {
        FILE *fd = fopen(path.c_str(), "w+");
        return fd;
    }

    FILE *OpenRead(const std::string &path)
    {
        FILE *fd = fopen(path.c_str(), "r");
        return fd;
    }

    FILE *Append(const std::string &path)
    {
        FILE *fd = fopen(path.c_str(), "a");
        return fd;
    }

    int64_t Seek(FILE *f, uint64_t offset)
    {
        auto const pos = fseek(f, offset, SEEK_SET);
        return pos;
    }

    int Read(FILE *f, void *buffer, uint32_t size)
    {
        const auto read = fread(buffer, size, 1, f);
        return read;
    }

    int Write(FILE *f, const void *buffer, uint32_t size)
    {
        int write = fwrite(buffer, size, 1, f);
        return write;
    }

    void Close(FILE *fd)
    {
        int err = fclose(fd);
    }

    std::vector<char> Load(const std::string &path)
    {
        FILE *fd = fopen(path.c_str(), "r");
        if (fd == nullptr)
            return std::vector<char>(0);

        const auto size = fseek(fd, 0, SEEK_END);
        fseek(fd, 0, SEEK_SET);

        std::vector<char> data(size);

        const auto read = fread(data.data(), data.size(), 1, fd);
        fclose(fd);
        if (read < 0)
            return std::vector<char>(0);

        data.resize(read);

        return data;
    }

    void Save(const std::string &path, const void *data, uint32_t size)
    {
        FILE *fd = fopen(path.c_str(), "w+");
        if (fd == nullptr)
            return;

        const char *data8 = static_cast<const char *>(data);
        while (size != 0)
        {
            int written = fwrite(data8, size, 1, fd);
            fclose(fd);
            if (written <= 0)
                return;
            data8 += written;
            size -= written;
        }
    }

    std::vector<FsEntry> ListDir(const std::string &ppath, int *err)
    {
        std::vector<FsEntry> out;
        FsEntry entry;
        std::string path = ppath;

        memset(&entry, 0, sizeof(FsEntry));
        sprintf(entry.directory, "%s", path.c_str());
        sprintf(entry.name, "..");
        sprintf(entry.display_size, lang_strings[STR_FOLDER]);
        sprintf(entry.path, "%s", path.c_str());
        entry.file_size = 0;
        entry.isDir = true;
        out.push_back(entry);

        DIR *fd = opendir(path.c_str());
        *err = 0;
        if (fd == NULL)
        {
            *err = 1;
            return out;
        }

        while (true)
        {
            struct dirent *dirent;
            FsEntry entry;
            dirent = readdir(fd);
            if (dirent == NULL)
            {
                closedir(fd);
                return out;
            }
            else
            {
                if (strcmp(dirent->d_name, ".") == 0 || strcmp(dirent->d_name, "..") == 0)
                {
                    continue;
                }

                snprintf(entry.directory, 512, "%s", path.c_str());
                snprintf(entry.name, 256, "%s", dirent->d_name);

                if (hasEndSlash(path.c_str()))
                {
                    sprintf(entry.path, "%s%s", path.c_str(), dirent->d_name);
                }
                else
                {
                    sprintf(entry.path, "%s/%s", path.c_str(), dirent->d_name);
                }
                struct stat file_stat = {0};
                stat(entry.path, &file_stat);
                struct tm tm = *localtime(&file_stat.st_mtim.tv_sec);
                entry.modified.day = tm.tm_mday;
                entry.modified.month = tm.tm_mon + 1;
                entry.modified.year = tm.tm_year + 1900;
                entry.modified.hours = tm.tm_hour;
                entry.modified.minutes = tm.tm_min;
                entry.modified.seconds = tm.tm_sec;
                entry.file_size = file_stat.st_size;

                if (dirent->d_type & DT_DIR)
                {
                    entry.isDir = true;
                    entry.file_size = 0;
                    sprintf(entry.display_size, lang_strings[STR_FOLDER]);
                }
                else
                {
                    if (entry.file_size < 1024)
                    {
                        sprintf(entry.display_size, "%lldB", entry.file_size);
                    }
                    else if (entry.file_size < 1024 * 1024)
                    {
                        sprintf(entry.display_size, "%.2fKB", entry.file_size * 1.0f / 1024);
                    }
                    else if (entry.file_size < 1024 * 1024 * 1024)
                    {
                        sprintf(entry.display_size, "%.2fMB", entry.file_size * 1.0f / (1024 * 1024));
                    }
                    else
                    {
                        sprintf(entry.display_size, "%.2fGB", entry.file_size * 1.0f / (1024 * 1024 * 1024));
                    }
                    entry.isDir = false;
                }
                out.push_back(entry);
            }
        }
        closedir(fd);

        return out;
    }

    std::vector<std::string> ListFiles(const std::string &path)
    {
        DIR *fd = opendir(path.c_str());
        if (fd == NULL)
            return std::vector<std::string>(0);

        std::vector<std::string> out;
        while (true)
        {
            struct dirent *dirent;
            dirent = readdir(fd);
            if (dirent == NULL)
            {
                closedir(fd);
                return out;
            }

            if (strcmp(dirent->d_name, ".") == 0 || strcmp(dirent->d_name, "..") == 0)
            {
                continue;
            }

            if (dirent->d_type & DT_DIR)
            {
                std::vector<std::string> files = FS::ListFiles(path + "/" + dirent->d_name);
                for (std::vector<std::string>::iterator it = files.begin(); it != files.end();)
                {
                    out.push_back(std::string(dirent->d_name) + "/" + *it);
                    ++it;
                }
            }
            else
            {
                out.push_back(dirent->d_name);
            }
        }
        closedir(fd);
        return out;
    }

    int RmRecursive(const std::string &path)
    {
        if (stop_activity)
            return 1;
        
        DIR *dfd = opendir(path.c_str());
        if (dfd != NULL)
        {
            struct dirent *dir = NULL;
            do
            {
                dir = readdir(dfd);
                if (dir == NULL || strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0)
                {
                    continue;
                }

                char new_path[512];
                snprintf(new_path, 512, "%s%s%s", path.c_str(), hasEndSlash(path.c_str()) ? "" : "/", dir->d_name);

                if (dir->d_type & DT_DIR)
                {
                    int ret = RmRecursive(new_path);
                    if (ret <= 0)
                    {
                        sprintf(status_message, "%s %s", lang_strings[STR_FAIL_DEL_DIR_MSG], new_path);
                        closedir(dfd);
                        return ret;
                    }
                }
                else
                {
                    snprintf(activity_message, 1024, "%s %s", lang_strings[STR_DELETING], new_path);
                    int ret = remove(new_path);
                    if (ret < 0)
                    {
                        sprintf(status_message, "%s %s", lang_strings[STR_FAIL_DEL_FILE_MSG], new_path);
                        closedir(dfd);
                        return ret;
                    }
                }
            } while (dir != NULL && !stop_activity);

            closedir(dfd);

            if (stop_activity)
                return 0;

            int ret = rmdir(path.c_str());
            if (ret < 0)
            {
                sprintf(status_message, "%s %s", lang_strings[STR_FAIL_DEL_DIR_MSG], path.c_str());
                return ret;
            }
            snprintf(activity_message, 1024, "%s %s", lang_strings[STR_DELETED], path.c_str());
        }
        else
        {
            int ret = remove(path.c_str());
            if (ret < 0)
            {
                sprintf(status_message, "%s %s", lang_strings[STR_FAIL_DEL_FILE_MSG], path.c_str());
                return ret;
            }
            snprintf(activity_message, 1024, "%s %s", lang_strings[STR_DELETED], path.c_str());
        }

        return 1;
    }

    int FsEntryComparator(const void *v1, const void *v2)
    {
        const FsEntry *p1 = (FsEntry *)v1;
        const FsEntry *p2 = (FsEntry *)v2;
        if (strcasecmp(p1->name, "..") == 0)
            return -1;
        if (strcasecmp(p2->name, "..") == 0)
            return 1;

        if (p1->isDir && !p2->isDir)
        {
            return -1;
        }
        else if (!p1->isDir && p2->isDir)
        {
            return 1;
        }

        return strcasecmp(p1->name, p2->name);
    }

    void Sort(std::vector<FsEntry> &list)
    {
        qsort(&list[0], list.size(), sizeof(FsEntry), FsEntryComparator);
    }

    std::string GetPath(const std::string &ppath1, const std::string &ppath2)
    {
        std::string path1 = ppath1;
        std::string path2 = ppath2;
        path2 = Util::Rtrim(Util::Trim(path2, " "), "/");
        return path1 + "/" + path2;
    }
}
