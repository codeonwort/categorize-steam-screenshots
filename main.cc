//
// Categorize uncompressed (.png) screenshots taken from Steam
//
// When you press F12, compressed (.jpg) screenshots are saved to your Steam installation path
// and each game has it's own screenshot folder. But there uncompressed companions are
// just saved in a single folder and they all get mixed.
// 
// ~This program compares .jpg and .png screenshot files
// to categorize .png files into their own subdirectories.~
// This program categorizes .png files into their own subdirectories.
//

#include <stdio.h>
#include <string>
#include <vector>
#include <regex>
#include <map>
#include <fstream>
#include <filesystem>
using namespace std;

#include <dirent.h> // popen, opendir, readdir, ...

const char* DUMP_APP_TITLE = "dumpapptitle.txt";

// #todo: Is there standard API for popen()?
bool exec(const char* cmd, string& outResult)
{
    FILE* pipe = popen(cmd, "r");
    if (pipe == nullptr)
    {
        return false;
    }

    outResult.clear();

    char buffer[1024];
    while (fgets(buffer, 1024, pipe) != nullptr)
    {
        outResult += buffer;
    }

    pclose(pipe);

    return true;
}

// Clunky ad-hoc relying on http request and renaming hueristics
// #todo: Robust way to get the title string
bool getTitleFromId(const char* appId, string& outResult)
{
    const char* CMD_FORMAT = "wget -qO- https://store.steampowered.com/app/%s | grep \"<title>\"";

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), CMD_FORMAT, appId);

    string cmdResult;
    if (exec(cmd, cmdResult))
    {
        size_t x1 = cmdResult.find("<title>");
        size_t x2 = cmdResult.find(" on Steam</title>");
        if (x1 != string::npos && x2 != string::npos)
        {
            x1 += 7; // length of <title>
            outResult = cmdResult.substr(x1, x2-x1).c_str();

            // Special case: If on discount, "Save N% on " is attached as a prefix on the title.
            {
                regex re = regex("^Save [0-9]+% on ");
                outResult = regex_replace(outResult, re, "");
            }

            // Remove characters that are invalid for a filename
            // #todo: Just remove it or replace with a whitespace?
            //     suggestion - if the invalid char is not followed by a whitespace, replace it with a whitespace.
            //     ex) XCOM: ENEMY UNKNOWN -> XCOM ENEMY UNKNOWN
            //     ex) Mega Man Zero/ZX Legacy Collection -> Mega Man Zero ZX Legacy Collection
            // Currently just removing all invalid chars...
            {
                regex re = regex("[\\/:*\?\"<>|]");
                outResult = regex_replace(outResult, re, "");
            }

            return true;
        }
        else
        {
            printf("No <title> tag? : appId=%s\n", appId);
        }
    }

    return false;
}

// #todo: Replace with std::filesystem
void collectAppIdList(const char* jpgDir, vector<string>& outIdList)
{
    outIdList.clear();

    DIR* jpgDirIter = opendir(jpgDir);
    if (jpgDirIter == nullptr)
    {
        puts("error: opendir failed");
    }
    while (jpgDirIter != nullptr)
    {
        dirent* entry = readdir(jpgDirIter);
        if (entry != nullptr)
        {
            if (entry->d_name[0] != '.')
            {
                string str = entry->d_name;
                outIdList.emplace_back(str);
            }
        }
        else
        {
            break;
        }
    }
    closedir(jpgDirIter);
}

void categorizePngFiles(const char* jpgBaseDir, const char* appId, const char* pngBaseDir)
{
    char jpgDir[1024];
    snprintf(jpgDir, sizeof(jpgDir), "%s/%s/screenshots/", jpgBaseDir, appId);

    string appTitle;
    if (getTitleFromId(appId, appTitle) == false)
    {
        printf("Failed to get the appTitle from appId: %s\n", appId);
        return;
    }

    printf("Processing appId=%s (%s)\n", appId, appTitle.c_str());

    char pngTargetDirBuffer[1024];
    snprintf(pngTargetDirBuffer, sizeof(pngTargetDirBuffer), "%s/%s/", pngBaseDir, appTitle.c_str());
    filesystem::create_directories(filesystem::path(pngTargetDirBuffer));

    for(auto& p : filesystem::directory_iterator(jpgDir))
    {
        if (p.is_regular_file() == false)
        {
            continue;
        }
        std::string ext = p.path().extension().string();
        if (ext != ".jpg")
        {
            continue;
        }
        std::string jpgName = p.path().stem().string();

        char pngSourceBuffer[1024];
        snprintf(pngSourceBuffer, sizeof(pngSourceBuffer), "%s/%s_%s.png", pngBaseDir, appId, jpgName.c_str());
        filesystem::path pngSource(pngSourceBuffer);

        if (filesystem::exists(pngSource) == false)
        {
            // #todo: Log if a companion png does not exist
            continue;
        }

        char pngTargetBuffer[1024];
        snprintf(pngTargetBuffer, sizeof(pngTargetBuffer), "%s/%s/%s_%s.png", pngBaseDir, appTitle.c_str(), appId, jpgName.c_str());
            
        filesystem::path pngTarget(pngTargetBuffer);
        //filesystem::copy_file(pngSource, pngTarget);
        // #todo: What if the target png is already there?
        filesystem::rename(pngSource, pngTarget);
    }
}

void printHelp()
{
    puts("Usage: <program_name> <steam_uncompressed_images_directory>");
#if 0
    puts("Usage: <program_name> %1 %2\n"
         "Example:\n"
         "  %1 : /mnt/c/Program Files (x86)/Steam/userdata/<number1>/<number2>/remote/\n"
         "  %2 : /mnt/f/SteamPngScreenshots/\n"
         "To identity <number1> and <number2>, open your steam client and go to "
         "Steam -> View -> Screenshots -> Screenshot Uploader then click 'Show on Disk'.");
#endif
}

bool isInteger(string s)
{
    return !s.empty() && s.find_first_not_of("0123456789") == string::npos;
}

struct PngFileInfo
{
    string appId;
    string filepath; // full path
    string filename; // including extension
};

void collectPngFilesRecursively(const char* pngDir, vector<PngFileInfo>& infoArray)
{
    infoArray.clear();

    filesystem::path baseDir(pngDir);
    for (auto& pngIter : filesystem::recursive_directory_iterator(baseDir))
    {
        if (pngIter.is_regular_file() == false)
        {
            continue;
        }
        if (pngIter.path().extension().string() != ".png")
        {
            continue;
        }

        string path = pngIter.path().string();
        string filename = pngIter.path().stem().string();
        string filenameFull = pngIter.path().filename().string();

        // If a filename consists of x_y.png where x is an integer,
        // then assume that x is the app id of a steam game.
        size_t x1 = filename.find_first_of('_');
        if (x1 == string::npos)
        {
            continue;
        }

        string appId = filename.substr(0, x1);
        if (isInteger(appId) == false)
        {
            continue;
        }

        //printf("[%s] %s\n", appId.c_str(), path.c_str());

        PngFileInfo info { appId, path, filenameFull };
        infoArray.push_back(info);
    }
}

string getDumpPath(const char* pngDir)
{
    char buffer[1024];
    snprintf(buffer, sizeof(buffer), "%s/%s", pngDir, DUMP_APP_TITLE);
    return string(buffer);
}

void readDumpAppTitle(const char* pngDir, map<string, string>& mapping)
{
    string dumppath = getDumpPath(pngDir);
    printf("> Read app title dump: %s\n", dumppath.c_str());

    ifstream fs;
    fs.open(dumppath);
    if (fs.is_open())
    {
        string line;
        while (getline(fs, line))
        {
            size_t delim = line.find(',');
            string appId = line.substr(0, delim);
            string appTitle = line.substr(delim + 1);
            mapping[appId] = appTitle;
        }
    }
    else
    {
        puts("Failed to read the dump");
    }
    fs.close();
}

void writeDumpAppTitle(const char* pngDir, const map<string, string>& mapping)
{
    string dumppath = getDumpPath(pngDir);
    printf("> Dump app titles: %s\n", dumppath.c_str());

    ofstream fs;
    fs.open(dumppath, fstream::out | fstream::trunc);
    if (fs.is_open())
    {
        for (auto& iter : mapping)
        {
            fs << iter.first << ',' << iter.second << std::endl;
        }
    }
    else
    {
        puts("Failed to create the dump");
    }
    fs.close();
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        printHelp();
        return 1;
    }

    const char* pngDir = argv[1];

    puts("> Collect steam uncompressed images...");

    vector<PngFileInfo> pngInfoArray;
    collectPngFilesRecursively(pngDir, pngInfoArray);
    printf("%d files are found\n", (int)pngInfoArray.size());

    map<string, string> idToTitleMap;

    // wget is too slow, let's dump the titles
    readDumpAppTitle(pngDir, idToTitleMap);

    puts("> Resolve app IDs into app titles...");

    for (size_t i = 0; i < pngInfoArray.size(); ++i)
    {
        const string& appId = pngInfoArray[i].appId;
        if (idToTitleMap.find(appId) == idToTitleMap.end())
        {
            string title;
            if (getTitleFromId(appId.c_str(), title))
            {
                idToTitleMap[appId] = title;
                printf("%s : %s\n", appId.c_str(), title.c_str());
            }
        }
    }

    // wget is too slow, let's dump the titles
    writeDumpAppTitle(pngDir, idToTitleMap);

    puts("> Categorize...");

    for (size_t i = 0; i < pngInfoArray.size(); ++i)
    {
        const string& appId = pngInfoArray[i].appId;
        if (idToTitleMap.find(appId) == idToTitleMap.end())
        {
            printf("[Unknown AppId] Failed to process: %s\n", pngInfoArray[i].filepath.c_str());
            continue;
        }
        const string& title = idToTitleMap[pngInfoArray[i].appId];
        const string& filename = pngInfoArray[i].filename;

        filesystem::path sourcePath(pngInfoArray[i].filepath);

        char targetBuffer[1024];
        snprintf(targetBuffer, sizeof(targetBuffer), "%s/%s/%s", pngDir, title.c_str(), filename.c_str());
        filesystem::path targetPath(targetBuffer);

        // Create subdirectories first
        char pngTargetDirBuffer[1024];
        snprintf(pngTargetDirBuffer, sizeof(pngTargetDirBuffer), "%s/%s/", pngDir, title.c_str());
        filesystem::create_directories(filesystem::path(pngTargetDirBuffer));

        // Then move
        if (sourcePath != targetPath)
        {
            filesystem::rename(sourcePath, targetPath);
        }
    }

    puts("> Done.");

    return 0;
}

#if 0 // Old impl. relying on jpg files
int main(int argc, char* argv[])
{
    if (argc < 3)
    {
        printHelp();
        return 1;
    }

    const char* jpgDir = argv[1];
    const char* pngDir = argv[2];

    vector<string> appIdList;
    collectAppIdList(jpgDir, appIdList);

    printf("appid count: %d\n", (int)appIdList.size());

    for (size_t i = 0; i < appIdList.size(); ++i)
    {
        const std::string& appId = appIdList[i];
        categorizePngFiles(jpgDir, appId.c_str(), pngDir);

        printf("Progress: (%d/%d)\n", (int)i + 1, (int)appIdList.size());
    }

    puts("DONE.");

    return 0;
}
#endif

