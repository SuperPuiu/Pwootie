#include <sys/stat.h>
#include <Shared.h>
#include <errno.h>

#include <unistd.h>

char NFTW_BinPath[PATH_MAX] = {0};

/* Used internally by nftw. */
static int32_t Search(const char *PathName, const struct stat *StatBuffer, int32_t Type, struct FTW *ftwb) {
		unused(ftwb);

		uint32_t PathLen = strlen(PathName);
		uint32_t SrcIndex = PathLen;

		while (PathName[SrcIndex] != '/')
				SrcIndex--;
		SrcIndex++;

		if ((strcmp(PathName + SrcIndex, "wineserver") == 0) && Type == FTW_F && StatBuffer->st_mode & S_IXUSR) {
				memcpy(NFTW_BinPath, PathName, PathLen - 6);
				NFTW_BinPath[PathLen - 6] = 0;
				return 1;
		}

		return 0;
}

/* GetPrefixPath() returns the built prefix path. Allows for ExtraBytes to be allocated by passing a number argument.
	* @return always a char array. */
char *GetPrefixPath(uint32_t ExtraBytes) {
		char *HomeEnv = getenv("HOME");

		if (!HomeEnv)
				Error("No HOME environment variable found. Aborting.", ERR_STANDARD);

		uint32_t HomeLength = strlen(HomeEnv), InstallDirLength = strlen(INSTALL_DIR);
		uint32_t PrefixLength = strlen("prefix");

		char *Location = malloc((HomeLength + InstallDirLength + PrefixLength + ExtraBytes + 4) * sizeof(char));

		if (unlikely(!Location))
				Error("[FATAL]: Unable to allocate Location buffer during GetPrefixPath call.", ERR_MEMORY);

		memcpy(Location, HomeEnv, HomeLength);
		memcpy(Location + HomeLength + 1, INSTALL_DIR, InstallDirLength);
		memcpy(Location + HomeLength + InstallDirLength + 2, "prefix", PrefixLength);
		Location[HomeLength] = Location[HomeLength + InstallDirLength + 1] = Location[HomeLength + InstallDirLength + PrefixLength + 2] =  '/';
		Location[HomeLength + InstallDirLength + PrefixLength + 3] = '\0';

		return Location;
}

/* GetUserRegistry opens the user.reg file within the prefix in read/appeand mode and returns it, if possible.
	* @return FILE* on success and NULL on failure. */
FILE* GetUserRegistry() {
		const char *RegistryFileName = "user.reg";

		uint32_t RegFileLen = strlen(RegistryFileName);
		FILE *UserRegistry = NULL;

		char *Prefix = GetPrefixPath(RegFileLen + 1);

		memcpy(Prefix + strlen(Prefix), RegistryFileName, RegFileLen + 1);
		UserRegistry = fopen(Prefix, "r+");

		if (unlikely(!UserRegistry)) {
				Error("[ERROR]: Unable to open registry file.", ERR_STANDARD | ERR_NOEXIT);
				goto error;
		}

		free(Prefix);
		return UserRegistry;
error:
		free(Prefix);
		return NULL;
}

/* AddNewUser() adds a new account to the registry, allowing the user to log into it.
	* The registry is HKEY_CURRENT_USER/Software/Roblox/RobloxStudio/LoggedInUserStore/https:/www.roblox.com/users.
	* Format: {"userId1":{"username1":"name","profilePicUrl":"url"}}; {"userId2":{"username":"name","profilePicUrl":"url"}}; ...
	* The format userId should be the account's userid, the name can be anything and the url must be **ANY** valid url to an image.
	*
	* When first adding the account to the registry key and the user switches to said account, they'll be prompted to use browser login.
	* After that in theory it **should** work. User should close both studio windows after logging into the new account.
	*
	* @return 0 on success and -1 on failure.*/
int8_t AddNewUser(char *restrict UserId, char *restrict Name, char *restrict URL) {
		const char *NEW_USER = "{\\\"%s\\\":{\\\"username\\\":\\\"%s\\\",\\\"profilePicUrl\\\":\\\"%s\\\"}};";

		char *StrtolEndChar = NULL;
		strtol(UserId, &StrtolEndChar, 10);

		if (errno != 0 || UserId == NULL || *StrtolEndChar) {
				Error("[ERROR]: UserId is not a valid number.", ERR_STANDARD | ERR_NOEXIT);
				return -1;
		}

		URL = URL ? URL : "";

		FILE *UserRegistry = GetUserRegistry();
		const char *KeyLocation = "[Software\\\\Roblox\\\\RobloxStudio\\\\LoggedInUsersStore\\\\https:\\\\www.roblox.com]";

		uint32_t UserIdLen = strlen(UserId), NameLen = strlen(Name), URLLen = strlen(URL);
		uint32_t FormatLen = strlen(NEW_USER), NewBufferLen = UserIdLen + NameLen + URLLen + FormatLen - 5;
		uint32_t OldKeyLen = 0, RegFileContentSize = 0;

		char *Buffer = malloc(NewBufferLen);
		char *KeyContent = NULL, *RegFileContent = NULL, *KeyStart = NULL;

		if (unlikely(!Buffer))
				Error("[ERROR]: Unable to allocate Buffer during AddNewUser call.", ERR_MEMORY);

		sprintf(Buffer, NEW_USER, UserId, Name, URL);

		if (unlikely(!UserRegistry))
				goto error;

		RegFileContent = ReadFileToBuffer(UserRegistry, &RegFileContentSize);
		RegFileContent[RegFileContentSize] = '\0';
		KeyStart = strstr(RegFileContent, KeyLocation);

		if (unlikely(!KeyStart)) {
				Error("[ERROR]: Unable to find the KeyStart in registry.", ERR_STANDARD | ERR_NOEXIT);
				goto error;
		}

		/* Hope we don't hit the wrong key.. */
		KeyStart = strstr(KeyStart, "users");
		if (unlikely(!KeyStart)) {
				Error("[ERROR]: Unable to find users sub-key.", ERR_STANDARD | ERR_NOEXIT);
				goto error;
		}

		KeyStart += 7;

		do { OldKeyLen++; } while (KeyStart[OldKeyLen] != '\n');

		KeyContent = malloc((OldKeyLen + NewBufferLen + 1) * sizeof(char));

		if (unlikely(!KeyContent))
				Error("[ERROR]: Unable to allocate KeyContent during AddNewUser call.", ERR_MEMORY);

		memcpy(KeyContent, KeyStart, OldKeyLen);

		memcpy(KeyContent + OldKeyLen - 1, Buffer, NewBufferLen - 1);
		KeyContent[OldKeyLen + NewBufferLen - 2] = '"';
		KeyContent[OldKeyLen + NewBufferLen - 1] = '\0';

		memmove(KeyStart + OldKeyLen + NewBufferLen - 1, KeyStart + OldKeyLen + 1, strlen(KeyStart) - OldKeyLen);
		memcpy(KeyStart, KeyContent, OldKeyLen + NewBufferLen - 1);

		fseek(UserRegistry, 0, SEEK_SET);
		fwrite(RegFileContent, sizeof(char), RegFileContentSize + (NewBufferLen - OldKeyLen), UserRegistry);

		fclose(UserRegistry);
		free(KeyContent);
		free(Buffer);
		free(RegFileContent);
		return 0;
error:
		if (UserRegistry)
				fclose(UserRegistry);

		free(Buffer);

		if (RegFileContent)
				free(RegFileContent);

		if (KeyContent)
				free(KeyContent);

		return -1;
}

char *GetDefaultWineBinary(uint32_t ExtraBytes) {
		char *PathEnv = getenv("PATH");
		char *WineBinPath = NULL;
		char SearchPath[PATH_MAX];

		uint32_t PathEnvLen = strlen(PathEnv);
		uint32_t SearchPathIndex = 0;

		if (unlikely(!PathEnv)) {
				Error("[ERROR]: Unable to getenv PATH", ERR_STANDARD | ERR_NOEXIT);
				return NULL;
		}

		for (uint32_t SrcIndex = 0; SrcIndex < PathEnvLen; SrcIndex++) {
				if (likely(PathEnv[SrcIndex] != ':')) {
						SearchPath[SearchPathIndex] = PathEnv[SrcIndex];
						SearchPathIndex++;
				} else {
						/* Include the \0 string terminator. */
						memcpy(SearchPath + SearchPathIndex, "wine", 5);

						if (access(SearchPath, F_OK) != 0) {
								SearchPathIndex = 0;
								continue;
						}

						WineBinPath = malloc((SearchPathIndex + ExtraBytes + 5) * sizeof(char));

						if (unlikely(!WineBinPath))
								Error("[FATAL]: Unable to allocate WineBinPath during GetDefaultWineBinary call.", ERR_MEMORY);

						memcpy(WineBinPath, SearchPath, SearchPathIndex + 5);
						return WineBinPath;
				}
		}

		Error("[ERROR]: No wine binary found in $PATH environment variable.", ERR_STANDARD | ERR_NOEXIT);
		return NULL;
}

/* GetStudioSettings() is tasked with fetching the state of a global studio setting.
	* @return char array with the status of the setting, or NULL on failure. */
char *GetStudioSetting(char *Name) {
		FILE *Settings = NULL;
		uint32_t MSFLen, FullPathLen;
		uint32_t FileDataLen;
		uint32_t NameLen = strlen(Name);
		uint32_t StateLen = 0, StateSize = 6;

		const char *MainSettingsFile = "/drive_c/users/steamuser/AppData/Local/Roblox/GlobalSettings_13.xml";
		MSFLen = strlen(MainSettingsFile);

		char *FileData = NULL;
		char *SettingState = NULL;
		char *FullPath = GetPrefixPath(MSFLen);

		FullPathLen = strlen(FullPath);
		memcpy(FullPath + FullPathLen, MainSettingsFile, MSFLen + 1);

		Settings = fopen(FullPath, "r");

		if (unlikely(!Settings)) {
				Error("[ERROR]: Unable to open %s during GetStudioSetting call.", ERR_STANDARD | ERR_NOEXIT, FullPath);
				goto error;
		}

		fseek(Settings, 0, SEEK_END);
		FileDataLen = ftell(Settings);
		fseek(Settings, 0, SEEK_SET);

		FileData = malloc((FileDataLen + 1) * sizeof(char));

		if (unlikely(!FileData))
				Error("[ERROR]: Unable to allocate FileData during GetStudioSetting call.", ERR_MEMORY);

		fread(FileData, sizeof(char), FileDataLen, Settings);
		FileData[FileDataLen] = '\0';

		char *KeyStart = strstr(FileData, Name);

		if (unlikely(!KeyStart)) {
				Error("[ERROR]: Unable to find %s.", ERR_STANDARD | ERR_NOEXIT, Name);
				goto error;
		}

		KeyStart += NameLen;

		if (*KeyStart != '"') {
				Error("[ERROR]: KeyStart appears to not point to the end of a string.", ERR_STANDARD | ERR_NOEXIT);
				goto error;
		}

		/* Skip the end quote and arrow. */
		KeyStart += 2;

		/* I've chosen 6 to be the default size as that's the required size to store "false\0" */
		SettingState = malloc(6 * sizeof(char));

		while (*KeyStart != '<') {
				if (StateLen == StateSize) {
						StateSize *= 1.5;

						char *NewPtr = realloc(SettingState, StateSize);

						if (!NewPtr)
								Error("[ERROR]: Unable to realloc SettingsState.", ERR_MEMORY);

						SettingState = NewPtr;
				}

				SettingState[StateLen] = *KeyStart;
				StateLen++;
				KeyStart++;
		}

		SettingState[StateLen] = '\0';

		fclose(Settings);
		free(FileData);
		free(FullPath);
		return SettingState;
error:
		if (Settings)
				fclose(Settings);

		if (FileData)
				free(FileData);

		free(FullPath);
		return NULL;
}

/* SetupWine() is tasked with downloading and extracting a default WINE installation.
	* @return -1 on failure and 0 on success. */
int8_t SetupWine(uint8_t CheckExistence) {
		const char *WINE_INSTALL_DIR = "wine";
		char *DownloadLink = "https://github.com/vinegarhq/wine-builds/releases/download/10.16/vinegarhq-wine-10.16.tar.xz";
		char *UpdateWine = PwootieReadEntry("update_wine", 0);
		char *ForcedLink = PwootieReadEntry("wine_link", 0);

		printf("[INFO]: Preparing to download WINE.\n");

		if (ForcedLink)
				DownloadLink = ForcedLink;

		if (UpdateWine) {
				if (strcmp(UpdateWine, "false") == 0 && CheckExistence == 1) {
						printf("[INFO]: Option update_wine appears to be false. Aborting installation.\n");
						free(UpdateWine);
						return 0;
				}

				free(UpdateWine);
		}

		/* Unfortunately for me, the file is a tar.xz, which means that I'd either have to
			* introduce ANOTHER dependency to Pwootie only for this whole case, or call system().
			* Second choice sounds like the best choice to me. */
		uint32_t WineNameLength = 128;
		uint32_t HomeLength = strlen(getenv("HOME")), InstallDirLength = strlen(INSTALL_DIR);
		uint32_t WineDirLength = strlen(WINE_INSTALL_DIR);

		ResponseStruct *Response = NULL;

		/* 3 for two additional slashes and one additional magic byte.
			* The extra 256 are for building the path to the wine_binary. */
		uint32_t Total = HomeLength + InstallDirLength + WineDirLength + WineNameLength + 4 + 256;

		char *Path = malloc(Total * sizeof(char)), *PathCopy = malloc(Total * sizeof(char));
		char *Command = malloc(Total * 2 + 3 + 4);

		memcpy(Path, getenv("HOME"), HomeLength);
		memcpy(Path + HomeLength + 1, INSTALL_DIR, InstallDirLength);
		memcpy(Path + HomeLength + InstallDirLength + 2, WINE_INSTALL_DIR, WineDirLength);
		Path[HomeLength] = Path[HomeLength + InstallDirLength + 1] = Path[HomeLength + InstallDirLength + WineDirLength + 2] = '/';
		Path[HomeLength + InstallDirLength + WineDirLength + 3] = '\0';

		/* First create the directory. */
		if (mkdir(Path, 0755) && errno != EEXIST) {
				Error("[ERROR]: Failed to create the proton installation folder.", ERR_STANDARD | ERR_NOEXIT);
				goto error;
		} else if (errno == EEXIST && CheckExistence == 1) {
				printf("[INFO]: WINE is already installed.\n");
				free(Path);
				free(Command);
				return 0;
		}

		printf("Downloading %s.\n", DownloadLink);
		Response = CurlDownloadNoFile(DownloadLink, Path);

		if (unlikely(Response->Response != CURLE_OK || Response->FileName[0] == '\0')) {
				Error("[ERROR]: Failed to download the tar file. (cURL error: %s)", ERR_STANDARD | ERR_NOEXIT, curl_easy_strerror(Response->Response));
				goto error;
		}

		memcpy(PathCopy, Path, Total);

		if (WineNameLength < Response->FileNameSize) {
				uint32_t SizeDifference = WineNameLength - Response->FileNameSize;

				Total += SizeDifference;
				PathCopy = realloc(PathCopy, sizeof(char) * Total);

				if (unlikely(!PathCopy))
						Error("[FATAL]: Unable to realloc PathCopy during SetupWine call.", ERR_MEMORY);

				Path = realloc(Path, sizeof(char) * Total);

				if (unlikely(!Path))
						Error("[FATAL]: Unable to realloc Path during SetupWine call.", ERR_MEMORY);
		}

		WineNameLength = Response->FileNameSize;
		memcpy(Path + HomeLength + InstallDirLength + WineDirLength + 3, Response->FileName, WineNameLength);
		Path[HomeLength + InstallDirLength + WineNameLength + WineDirLength + 3] = '\0';

		fclose(Response->FileStream);
		Response->FileStream = NULL;

		/* im not going to memcpy this. */
		printf("Unzipping new version..\n");
		sprintf(Command, "tar -xf %s -C %s > /dev/null 2>&1", Path, PathCopy);

		if (unlikely(system(Command) != 0)) {
				Error("[ERROR]: tar -xf failed.", ERR_STANDARD | ERR_NOEXIT);
				goto error;
		}

		printf("Unzipped new version.\n");

		/* Remove zip file. */
		remove(Path);

		/* Write wine_binary entry. If wine64 binary can't be found, then throw an error.
			* strlen(".tar.xz") should get optimized by the compiler hopefully. */
		printf("Fetching new wine path.\n");
		Path[(HomeLength + InstallDirLength + WineNameLength + WineDirLength + 3) - strlen(".tar.xz")] = 0;
		nftw(Path, Search, 10, FTW_DEPTH | FTW_MOUNT | FTW_PHYS);

		if (unlikely(NFTW_BinPath[0] == 0)) {
				Error("[ERROR]: Unable to find wine64 binary. Invalid installation path (%s).", ERR_STANDARD | ERR_NOEXIT, Path);
				goto error;
		}

		printf("New wine_binary path: %s\n", NFTW_BinPath);
		PwootieWriteEntry("wine_binary", NFTW_BinPath);

		/* Cleanup. */
		free(Command);
		free(Path);
		free(PathCopy);

		if (ForcedLink)
				free(ForcedLink);

		if (Response->FreeName)
				free(Response->FileName);
		free(Response);

		printf("[INFO]: WINE installation finished!\n");

		return 0;
error:
		free(Path);
		free(PathCopy);
		free(Command);

		if (ForcedLink)
				free(ForcedLink);

		if (Response) {
				if (Response->FileStream)
						fclose(Response->FileStream);

				if (Response->FreeName)
						free(Response->FileName);
				free(Response);
		}

		return -1;
}

/* SetupPrefix() is tasked with setting up the prefix.
	* @return 0 on success, otherwise return -1 on failure. */
int8_t SetupPrefix() {
		printf("[INFO]: Setting up prefix.\n");

		char *Location = NULL;

		const char *D3DX11_43 = "winetricks d3dx11_43 > /dev/null 2>&1";
		const char *DXVK = "winetricks dxvk > /dev/null 2>&1";

		char *DEBUG_ENTRY = PwootieReadEntry("debug", 0);
		char *WINE_BINARY = PwootieReadEntry("wine_binary", 0);
		uint8_t FreePath = 1;

		if (unlikely(!WINE_BINARY)) {
				WINE_BINARY = "";
				FreePath = 0;
		}

		if (DEBUG_ENTRY) {
				if (strcmp(DEBUG_ENTRY, "true") == 0) {
						D3DX11_43 = "winetricks d3dx11_43";
						DXVK = "winetricks dxvk";
				}

				free(DEBUG_ENTRY);
		}

		setenv("WINE", WINE_BINARY, 1);

		if (unlikely(system("winetricks --version > /dev/null 2>&1") != 0)) {
				Error("[FATAL]: Unable to run winetricks --version. Is winetricks installed?", ERR_STANDARD | ERR_NOEXIT);
				goto error;
		}

		/* Is all this string building needed? */
		Location = GetPrefixPath(0);

		if (mkdir(Location, 0755) && errno != EEXIST) {
				Error("[FATAL]: Unable to create folder during SetupPrefix call.", ERR_STANDARD | ERR_NOEXIT);
				Error(Location, ERR_STANDARD | ERR_NOEXIT);
				goto error;
		}

		errno = 0;

		/* https://www.man7.org/linux/man-pages/man3/setenv.3.html */
		setenv("WINEPREFIX", Location, 1);

		/* Install required dlls for studio to launch. Check status values. */
		printf("Installing d3dx11_43..\n");
		if (unlikely(system(D3DX11_43) != 0))
				Error("[FATAL]: winetricks failed to install d3dx11_43. Please try to manually install the component.", ERR_STANDARD | ERR_NOEXIT);

		printf("Installing dxvk..\n");
		if (unlikely(system(DXVK) != 0))
				Error("[FATAL]: winetricks failed to install dxvk. Please try to manually install the component.", ERR_STANDARD | ERR_NOEXIT);

		if (FreePath)
				free(WINE_BINARY);

		free(Location);
		printf("[INFO]: Prefix setup finished!\n");
		return 0;
error:
		if (FreePath)
				free(WINE_BINARY);

		free(Location);
		return -1;
}

void RunWineCfg() {
		uint32_t WineExecLen;

		char *Prefix = GetPrefixPath(0);
		char *WineExec = PwootieReadEntry("wine_binary", 0);
		char *Command;

		if (unlikely(!WineExec)) {
				WineExec = GetDefaultWineBinary(0);

				if (!WineExec)
						return;

				PwootieWriteEntry("wine_binary", WineExec);
		}

		WineExecLen = strlen(WineExec);
		Command = malloc(sizeof(char) * (WineExecLen + 1 + strlen("cfg")));

		if (!Command)
				Error("[FATAL]: Unable to allocate Command during RunWineCfg call.", ERR_MEMORY);

		memcpy(Command, WineExec, WineExecLen);

		for (uint32_t SrcIndex = WineExecLen; SrcIndex > 0; SrcIndex--) {
				if (Command[SrcIndex] != 'e')
						continue;

				memcpy(Command + SrcIndex + 1, "cfg", 4);
				break;
		}

		setenv("WINEPREFIX", Prefix, 1);

		if (unlikely(ExecProgram(Command, 0, 0, Command, NULL) != 0)) {
				printf("[ERROR]: ExecProgram(%s) returned non zero value. Running default winecfg.\n", Command);
				if (unlikely(system("winecfg") != 0)) /* Fallback. */
						printf("[ERROR]: Unable to run fallback winecfg. Aborting.\n");
		}

		free(Command);
		free(Prefix);
		free(WineExec);
}

void Run(char *restrict Argument, char *restrict Version) {
		printf("[INFO]: Attempting to launch studio..\n");
		const char *EXECUTABLE = "RobloxStudioBeta.exe";

		if (Argument == NULL)
				Argument = "";

		char *ArgDuplicate = strdup(Argument);

		char *WINE_EXEC = PwootieReadEntry("wine_binary", 0);
		char *DEBUG_ENTRY = PwootieReadEntry("debug", 0);
		uint8_t Silence = 1;

		/* If the wine_binary entry doesn't exist, just create a dummy one and hope wine exists. */
		if (unlikely(!WINE_EXEC)) {
				WINE_EXEC = GetDefaultWineBinary(0);

				if (unlikely(!WINE_EXEC))
						return;

				PwootieWriteEntry("wine_binary", WINE_EXEC);
		}

		if (DEBUG_ENTRY) {
				if (strcmp(DEBUG_ENTRY, "true") == 0)
						Silence = 0;

				free(DEBUG_ENTRY);
		}

		uint32_t VersionLen = strlen(Version), HomeLength = strlen(getenv("HOME"));
		uint32_t InstallLen = strlen(INSTALL_DIR), ExecutableLen = strlen(EXECUTABLE);

		char *Location = GetPrefixPath(0);
		char *Executable = malloc((HomeLength + InstallLen + VersionLen + ExecutableLen + 4) * sizeof(char));

		if (unlikely(!Executable))
				Error("[FATAL]: Unable to allocate Executable buffer during Run call.", ERR_MEMORY);

		memcpy(Executable, getenv("HOME"), HomeLength);
		memcpy(Executable + HomeLength + 1, INSTALL_DIR, InstallLen);
		memcpy(Executable + HomeLength + InstallLen + 2, Version, VersionLen);
		memcpy(Executable + HomeLength + InstallLen + VersionLen + 3, EXECUTABLE, ExecutableLen + 1);
		Executable[HomeLength] = Executable[HomeLength + InstallLen + 1] = Executable[HomeLength + InstallLen + VersionLen + 2] = '/';

		setenv("WINEPREFIX", Location, 1);
		ExecProgram(WINE_EXEC, Silence, 1, Executable, ArgDuplicate, NULL);

		free(Executable);
		free(Location);
		free(WINE_EXEC);
}
