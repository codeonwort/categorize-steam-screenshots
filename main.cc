//
// Categorize uncompressed (.png) screenshots taken from Steam
//
// When you press F12, compressed (.jpg) screenshots are saved to your Steam installation path
// and each game has it's own screenshot folder. But there uncompressed companions are
// just saved in a single folder and they all get mixed.
// 
// This program compares .jpg and .png screenshot files
// to categorize .png files into their own subdirectories.
//
// NOTE: In fact, I realized I don't need .jpg files to categorize .png files
// after completing this version of program. I'm working on the next version...
//

#include <stdio.h>
#include <string>
#include <vector>
#include <regex>
#include <filesystem>
using namespace std;

#include <dirent.h> // popen, opendir, readdir, ...

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
    snprintf(pngTargetDirBuffer, sizeof(pngTargetDirBuffer), "%s/cat/%s/", pngBaseDir, appTitle.c_str());
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
    puts("Usage: <program_name> %1 %2\n"
         "Example:\n"
         "  %1 : /mnt/c/Program Files (x86)/Steam/userdata/<number1>/<number2>/remote/\n"
         "  %2 : /mnt/f/SteamPngScreenshots/\n"
         "To identity <number1> and <number2>, open your steam client and go to "
         "Steam -> View -> Screenshots -> Screenshot Uploader then click 'Show on Disk'.");
}

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

