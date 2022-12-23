using System.Net;
using System.Net.Http;
using System.Text.RegularExpressions;

namespace csharp_ver
{
    // For C# practice, ported from original C++ program.
    //
    // When you press F12, Steam might save an uncompressed screenshot in .png format.
    // Unlike compressed screenshots, uncompressed ones are all saved in the same folder.
    // This program categorizes such .png files into their own subdirectories.

    public struct PngFileInfo
    {
        public int appId;
        public string fullpath; // full path
        public string filename; // including extension
    }

    internal class CategorizeSteamScreenshots
    {
        static string? HARD_CODED_PNG_FOLDER = null;
        static string DUMP_APP_TITLE = "dumpapptitle.txt";

        static async Task Main(string[] args)
        {
            Console.WriteLine("=== Categorize Steam Screenshots ===");

            string? pngDir = HARD_CODED_PNG_FOLDER;
            if (pngDir == null)
            {
                Console.WriteLine("Enter the path of screenshot folder");
                Console.Write("> ");
                pngDir = Console.ReadLine();
            }
            if (pngDir == null)
            {
                Console.WriteLine("Invalid input. The program will exit...");
                return;
            }

            List<PngFileInfo> pngInfoArray = new List<PngFileInfo>();
            CollectPngFiles(pngDir, ref pngInfoArray);
            Console.WriteLine(String.Format("{0} .png files are found", pngInfoArray.Count));

            var appTitleDictionary = new Dictionary<int/*appId*/, string/*appTitle*/>();
            ReadAppTitleDump(pngDir, ref appTitleDictionary);
            Console.WriteLine(String.Format("{0} titles were already cached", appTitleDictionary.Count));

            Console.WriteLine("Retrieving unknown app titles...");

            HashSet<int> alreadySearched = new HashSet<int>();
            foreach (PngFileInfo info in pngInfoArray)
            {
                if (appTitleDictionary.ContainsKey(info.appId) == false)
                {
                    if (alreadySearched.Contains(info.appId))
                    {
                        continue;
                    }

                    string? title = await GetAppTitleFromId(info.appId);
                    if (title != null)
                    {
                        appTitleDictionary[info.appId] = title;
                        Console.WriteLine(String.Format("{0} : {1}", info.appId, title));
                    }
                    else
                    {
                        Console.WriteLine(String.Format("Can't find the title for {0}", info.appId));
                    }
                    alreadySearched.Add(info.appId);
                }
            }

            WriteAppTitleDump(pngDir, appTitleDictionary);
            Console.WriteLine(String.Format("{0} titles are cached", appTitleDictionary.Count));

            Console.WriteLine("> Categorize...");

            int numMoved = 0;
            foreach (PngFileInfo info in pngInfoArray)
            {
                if (appTitleDictionary.ContainsKey(info.appId) == false)
                {
                    Console.WriteLine(String.Format("[Unknown AppId] Failed to process: {0}", info.filename));
                    continue;
                }

                string title = appTitleDictionary[info.appId];
                string sourcePath = info.fullpath;
                string targetDir = String.Format("{0}/{1}", pngDir, title);
                string targetPath = Path.GetFullPath(String.Format("{0}/{1}", targetDir, info.filename));
                if (sourcePath.ToLower() != targetPath.ToLower())
                {
                    Directory.CreateDirectory(targetDir);
                    File.Move(sourcePath, targetPath);
                    Console.WriteLine(String.Format("{0} -> {1}", sourcePath, targetPath));
                    ++numMoved;
                }
            }
            Console.WriteLine(String.Format("{0} files are moved", numMoved));
        }

        // https://learn.microsoft.com/en-us/dotnet/csharp/programming-guide/file-system/how-to-iterate-through-a-directory-tree
        static void CollectPngFiles(in string pngDir, ref List<PngFileInfo> pngInfoArray)
        {
            pngInfoArray.Clear();

            Stack<string> dirs = new Stack<string>();
            if (!System.IO.Directory.Exists(pngDir))
            {
                throw new ArgumentException();
            }
            dirs.Push(pngDir);

            while (dirs.Count > 0)
            {
                string currentDir = dirs.Pop();
                string[] subDirs;
                try
                {
                    subDirs = System.IO.Directory.GetDirectories(currentDir);
                }
                catch (UnauthorizedAccessException) { continue; }
                catch (System.IO.DirectoryNotFoundException) { continue; }

                string[]? files = null;
                try
                {
                    files = System.IO.Directory.GetFiles(currentDir);
                }
                catch (UnauthorizedAccessException) { continue; }
                catch (System.IO.DirectoryNotFoundException) { continue; }

                foreach (string file in files)
                {
                    try
                    {
                        System.IO.FileInfo info = new System.IO.FileInfo(file);
                        if (info.Extension == ".png")
                        {
                            string[] splits = info.Name.Split("_");
                            if (splits.Length <= 1)
                            {
                                continue;
                            }
                            string appIdStr = splits[0];
                            int appId;
                            if (int.TryParse(appIdStr, out appId))
                            {
                                PngFileInfo pngInfo;
                                pngInfo.appId = appId;
                                pngInfo.fullpath = info.FullName;
                                pngInfo.filename = info.Name;
                                pngInfoArray.Add(pngInfo);
                            }
                        }
                    }
                    catch (System.IO.FileNotFoundException) { continue; }
                }

                foreach (string subdir in subDirs)
                {
                    dirs.Push(subdir);
                }
            }
        }

        static string GetDumpPath(in string pngDir)
        {
            return Path.GetFullPath(String.Format("{0}/{1}", pngDir, DUMP_APP_TITLE));
        }

        static void ReadAppTitleDump(in string pngDir, ref Dictionary<int, string> appTitleDictionary)
        {
            string dumppath = GetDumpPath(pngDir);
            Console.WriteLine(String.Format("> Read app title dump: {0}", dumppath));

            try
            {
                string[] lines = System.IO.File.ReadAllLines(dumppath);
                foreach (string line in lines)
                {
                    int sepIx = line.IndexOf(',');
                    int appId = int.Parse(line.Substring(0, sepIx));
                    string title = line.Substring(sepIx + 1);
                    appTitleDictionary[appId] = title;
                }
            }
            catch (Exception e)
            {
                Console.WriteLine(String.Format("Failed to open: {0}", dumppath));
                Console.WriteLine(e.Message);
            }
        }

        // Clunky ad-hoc relying on http request and renaming hueristics.
        // #todo: Robust way to get the title string.
        static async Task<string?> GetAppTitleFromId(int appId)
        {
            HttpClient client = new HttpClient();
            string uri = String.Format("https://store.steampowered.com/app/{0}", appId);
            
            string response = await client.GetStringAsync(uri);
            if (response.Length == 0)
            {
                return null;
            }

            int x1 = response.IndexOf("<title>");
            int x2 = response.IndexOf(" on Steam</title>");
            string? ret = null;
            if (x1 != -1 && x2 != -1)
            {
                x1 += 7; // length of "<title>"
                ret = response.Substring(x1, x2 - x1);

                // Special case: If on discount, "Save N% on " is attached as a prefix on the title.
                Regex re = new Regex("^Save [0-9]+% on ");
                ret = re.Replace(ret, "");

                // Remove characters that are invalid for a filename.
                re = new Regex("[\\/:*?\"<>|]");
                ret = re.Replace(ret, "");
            }
            return ret;
        }

        static void WriteAppTitleDump(in string pngDir, in Dictionary<int, string> appTitleDictionary)
        {
            string dumppath = GetDumpPath(pngDir);
            Console.WriteLine(String.Format("> Dump app titles: {0}", dumppath));

            FileStream fs = File.Create(dumppath);
            StreamWriter writer = new StreamWriter(fs);
            foreach (KeyValuePair<int, string> pair in appTitleDictionary)
            {
                int appId = pair.Key;
                string appTitle = pair.Value;
                writer.WriteLine(String.Format("{0},{1}", appId, appTitle));
            }
            writer.Flush();
            fs.Close();
        }
    }
}
