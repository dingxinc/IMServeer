
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>
//#include <sys/stat.h>
#include "../base/logging.h"
#include "FileManager.h"

FileManager::FileManager()
{

}

FileManager::~FileManager()
{

}

bool FileManager::Init(const char* basepath)
{
    DIR* dp = opendir(basepath);
    if (dp == NULL)
    {
        LOG_INFO << "open base dir error, errno: " << errno << ", " << strerror(errno);

        if (mkdir(basepath, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == 0)
            return true;

        LOG_ERROR << "create base dir error, " << basepath << ", errno: " << errno << ", " << strerror(errno);

        return false;
    }

    struct dirent* dirp;
    //struct stat filestat;
    while ((dirp = readdir(dp)) != NULL)
    {
        if (strcmp(dirp->d_name, ".") == 0 || strcmp(dirp->d_name, "..") == 0)
            continue;


        //if (stat(dirp->d_name, &filestat) != 0)
        //{
        //    LOG_WARN << "stat filename: [" << dirp->d_name << "] error, errno: " << errno << ", " << strerror(errno);
        //    continue;
        //}

        m_listFiles.emplace_back(dirp->d_name);
        LOG_INFO << "filename: " << dirp->d_name;
    }

    closedir(dp);
    
    return true;
}

bool FileManager::IsFileExsit(const char* filename)
{
    std::lock_guard<std::mutex> guard(m_mtFile);
    //�Ȳ鿴����
    for (const auto& iter : m_listFiles)
    {
        if (iter == filename)
            return true;
    }

    //�ٲ鿴�ļ�ϵͳ
    FILE* fp = fopen(filename, "r");
    if (fp != NULL)
    {
        fclose(fp);
        m_listFiles.emplace_back(filename);
        return true;
    }

    return false;
}

void FileManager::addFile(const char* filename)
{
    std::lock_guard<std::mutex> guard(m_mtFile);
    m_listFiles.emplace_back(filename);
}
