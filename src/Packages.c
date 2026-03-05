#include <Shared.h>

#include <sys/stat.h>
#include <errno.h>
#include <zip.h>
#include <md5.h>

#include <fcntl.h>
#include <unistd.h>

#define OFFICIAL_INSTALLER  "RobloxStudioInstaller.exe"
#define APP_SETTINGS        "AppSettings.xml"
#define APP_SETTINGS_DATA   "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n<Settings>\r\n        <ContentFolder>content</ContentFolder>\r\n        <BaseUrl>http://www.roblox.com</BaseUrl>\r\n</Settings>\r\n"

/* This is ugly. */
static inline void ChecksumToString(uint8_t *restrict Checksum, char *restrict Buffer) {
		uint8_t BufPosition = 0;
		char l_Buf[3];

		for (uint8_t i = 0; i < 16; i++, BufPosition += 2) {
				sprintf(l_Buf, "%02x", Checksum[i]);
				Buffer[BufPosition] = l_Buf[0];
				Buffer[BufPosition + 1] = l_Buf[1];
		}

		Buffer[BufPosition] = '\0';
}

/* Prototypes are found in Installer.c */
char *FormatChecksums(FetchStruct *Fetched) {
		const uint8_t ChecksumSize = 32;
		uint32_t Offset = 0;

		/* We add Fetched->TotalPackages for the semicolons. */
		char *Checksums = malloc((Fetched->TotalPackages * ChecksumSize + Fetched->TotalPackages) * sizeof(char));

		if (unlikely(!Checksums))
				Error("[FATAL]: Unable to allocate Checksums buffer during FormatChecksums call.", ERR_MEMORY);

		for (uint32_t SrcIndex = 0; SrcIndex < Fetched->TotalPackages; SrcIndex++) {
				Package *Current = &Fetched->PackageList[SrcIndex];

				memcpy(Checksums + Offset, Current->Checksum, ChecksumSize);
				Checksums[Offset + ChecksumSize] = ';';

				Offset += ChecksumSize + 1;
		}

		Checksums[Offset] = '\0';

		return Checksums;
}

int8_t InstallPackages(FetchStruct *Fetched, char *restrict Version, char *restrict Checksums) {
		CURLcode  Response;
		FILE      *Installer;

		struct zip_stat *ZipStat = NULL;

		uint32_t SettingsLen = strlen(APP_SETTINGS_DATA), LengthVersion = strlen(Version);
		uint32_t InstallerLength = strlen(OFFICIAL_INSTALLER), TempDirLength = strlen(TEMP_PWOOTIE_FOLDER);
		uint32_t InstallDirLength = strlen(INSTALL_DIR), HomeLength = strlen(getenv("HOME"));
		uint32_t AppSettingsLen = strlen(APP_SETTINGS);

		/* The 'Official' string is built inside of this function only to reuse it later. Same for 'InstallDir'. */
		char **Instructions   = NULL;
		char *InstallDir      = NULL;
		char *Official        = malloc((InstallerLength + TempDirLength + 64 + 1) * sizeof(char));
		char *FullURL         = BuildString(4, CDN_URL, Version, "-", OFFICIAL_INSTALLER);

		unused(Checksums);

		if (unlikely(!Official))
				Error("[FATAL]: Unable to allocate Official during InstallPackages call.", ERR_MEMORY);

		/* Construct official downloaded path. */
		memcpy(Official, TEMP_PWOOTIE_FOLDER, TempDirLength);
		memcpy(Official + TempDirLength, OFFICIAL_INSTALLER, InstallerLength);
		Official[InstallerLength + TempDirLength] = '\0';

		/* Construct installation directory path. */
		InstallDir = GetVersionPath(Version, 253);
		InstallDir[HomeLength + InstallDirLength + LengthVersion + 2] = '/';
		InstallDir[HomeLength + InstallDirLength + LengthVersion + 3] = '\0';

		/* Download installer in TEMP_PWOOTIE_FOLDER folder. */
		Installer = fopen(Official, "w+");

		if (unlikely(!Installer)) {
				Error("[ERROR]: Unable to create RobloxStudioInstaller file during InstallPackages call. (Path: %s)", ERR_STANDARD | ERR_NOEXIT, Official);
				goto error;
		}

		Response = CurlDownload(Installer, FullURL);

		if (unlikely(Response != CURLE_OK)) {
				Error("[ERROR]: Failed to download RobloxStudioInstaller.exe. (cURL error: %s)\n", ERR_STANDARD | ERR_NOEXIT, curl_easy_strerror(Response));
				goto error;
		}

		/* For this step, we have two ways of proceeding:
			* 1. Hardcode a 2D array of chars with where the zip files should be extracted.
			* CONS: If ROBLOX decided to change locations or add new packages we'll have to update the hardcoded array.
			* PROS: Easier to implement.
			* 2. Get extraction locations directly from RobloxStudioInstaller.exe:
			* CONS: More difficult to implement.
			* PROS: We're fixing the problem we'd have with the first solution.
			*
			* I found second solution by looking at the source code for Vinegar. Big thanks to them.
			* I found first implementation by looking at the source code for Cork. Big thanks to them as well.
			* Because we're going with the second solution, you may find the implementation in Instructions.c. */

		Instructions = ExtractInstructions(Installer, Fetched);

		/* Initialize all directories we got after extracting the instructions. */
		for (uint8_t Index = 0; Index < Fetched->TotalPackages; Index++) {
				/* Skip empty instructions. */
				if (Instructions[Index][0] == '\0')
						continue;
				uint32_t InstructionLength = strlen(Instructions[Index]);

				/* Add the required path to construct. Add +1 to the length because we want to include the NULL terminator. */
				memcpy(InstallDir + HomeLength + InstallDirLength + LengthVersion + 3, Instructions[Index], InstructionLength + 1);

				/* Build the directory tree. */
				BuildDirectoryTree(InstallDir, 0);
		}

		/* Why can't just AppSettings.xml be packaged in a zip file? I don't know! */
		memcpy(InstallDir + HomeLength + InstallDirLength + LengthVersion + 3, APP_SETTINGS, AppSettingsLen);
		InstallDir[HomeLength + InstallDirLength + LengthVersion + AppSettingsLen + 3] = '\0';

		FILE *AppSettings = fopen(InstallDir, "w");
		if (unlikely(!AppSettings)) {
				Error("[ERROR]: Failed to create AppSettings durign InstallPackages call.", ERR_STANDARD | ERR_NOEXIT);
				goto error;
		}

		fwrite(APP_SETTINGS_DATA, sizeof(char), SettingsLen, AppSettings);
		fclose(AppSettings);

		/* Decompress all zips downloaded during DownloadPackages call. */
		for (uint8_t i = 0; i < Fetched->TotalPackages; i++) {
				Package         CurPackage = Fetched->PackageList[i];

				if (!CurPackage.Download)
						continue;

				uint32_t        ZipLength = strlen(CurPackage.Name), InstructionLength = strlen(Instructions[i]);
				int32_t         ErrorCode = 0;
				zip_t           *ZipPointer;
				zip_error_t     ZipError;
				zip_file_t      *FileDescriptor;

				char *Memory;
				FILE *NewFile;

				ZipStat = malloc(sizeof(struct zip_stat));

				if (unlikely(!ZipStat))
						Error("[FATAL]: Unable to allocate ZipStat during InstallPackages call.", ERR_MEMORY);

				/* Construct required paths. Official path becomes the path to which the zip file is located. */
				memcpy(InstallDir + HomeLength + InstallDirLength + LengthVersion + 3, Instructions[i], InstructionLength + 1);
				memcpy(Official + TempDirLength, CurPackage.Name, ZipLength);
				Official[TempDirLength + ZipLength] = '\0';

				ZipPointer = zip_open(Official, 0, &ErrorCode);

				if (unlikely(!ZipPointer)) {
						zip_error_init_with_code(&ZipError, ErrorCode);
						Error("[ERROR]: Unable to open zip file %s. (zip error: %s)", ERR_STANDARD | ERR_NOEXIT, Official, zip_error_strerror(&ZipError));
						goto error;
				}

				/* Read all entries within the zip. */
				int64_t TotalEntries = zip_get_num_entries(ZipPointer, 0);

				/* Loop over the entries. */
				for (int64_t Entry = 0; Entry < TotalEntries; Entry++) {
						const char *Name = zip_get_name(ZipPointer, Entry, 0);
						uint32_t NameLength = strlen(Name);
						uint8_t Directory;

						if (unlikely(!Name)) {
								Error("[ERROR]: Error occured while extracting %li entry from %s.", ERR_STANDARD | ERR_NOEXIT, Entry, Official);
								goto error;
						}

						Directory = Name[NameLength - 1] == 92; /* Is the entry a directory? */

						/* Grrr, windows paths.. */
						ConvertPath((char*)Name);

						/* If the entry is a directory (only directories end with /) then we'll build the directory tree. */
						if (Directory) {
								memcpy(InstallDir + HomeLength + InstallDirLength + LengthVersion + InstructionLength + 2, Name, NameLength + 1);

								BuildDirectoryTree(InstallDir, 0);
								continue;
						}

						/* Needed for entry size. */
						zip_stat_index(ZipPointer, Entry, 0, ZipStat);
						Memory = calloc(ZipStat->size, sizeof(char));

						if (unlikely(!Memory))
								Error("[FATAL]: Unable to allocate Memory variable during InstallPackages call.", ERR_MEMORY);

						/* Include string terminator. */
						memcpy(InstallDir + HomeLength + InstallDirLength + InstructionLength + LengthVersion + 3, Name, NameLength + 1);
						NewFile = fopen(InstallDir, "wb");

						if (unlikely(!NewFile)) {
								Error("[ERROR]: Unable to open file at path %s to write contents.", ERR_STANDARD | ERR_NOEXIT, InstallDir);
								goto error;
						}

						FileDescriptor = zip_fopen_index(ZipPointer, Entry, 0);

						/* Read entry content and then write to the file opened earlier. */
						zip_fread(FileDescriptor, Memory, ZipStat->size);
						fwrite(Memory, sizeof(char), ZipStat->size, NewFile);

						/* Cleanup. */
						zip_fclose(FileDescriptor);
						fclose(NewFile);
						free(Memory);
				}

				free(ZipStat);
				zip_close(ZipPointer);

				ZipStat = NULL;

				printf(" Installing package %i out of %i.\r", i + 1, Fetched->TotalPackages);
				fflush(stdout);
		}

		printf("\n");

		/* Free the instructions 2D array. */
		for (uint8_t i = 0; i < Fetched->TotalPackages; i++)
				free(Instructions[i]);
		free(Instructions);

		/* Remove temporary directory. We need to reset the path buffer. */
		Official[TempDirLength] = '\0';

		if (unlikely(nftw(Official, DeleteFile, 10, FTW_DEPTH | FTW_MOUNT | FTW_PHYS) < 0)) {
				Error("[ERROR]: Failed to remove the Pwootie temporary directory (%s).", ERR_STANDARD | ERR_NOEXIT, Official);
		}

		/* Free additional variables. */
		free(FullURL);
		free(InstallDir);
		free(Official);

		return 0;

error:
		free(FullURL);
		free(InstallDir);
		free(Official);

		if (ZipStat)
				free(ZipStat);

		if (Instructions) {
				for (uint8_t i = 0; i < Fetched->TotalPackages; i++)
						free(Instructions[i]);

				free(Instructions);
		}

		return -1;
}

int8_t DownloadPackages(FetchStruct *Fetched, char *restrict Version, char *restrict Checksums) {
		uint32_t LengthURL = strlen(CDN_URL), LengthVersion = strlen(Version), RootPartLength = strlen(TEMP_PWOOTIE_FOLDER);
		uint8_t Increment = 0;
		uint8_t PackagesToDownload = 0;

		char *FullURL = malloc((LengthURL + LengthVersion + Fetched->LongestName + 2) * sizeof(char));
		char *ZipFilePath = malloc((RootPartLength + Fetched->LongestName + 2) * sizeof(char));

		char *BufferPointers[32];
		char *LinkPointers[32];
		char *RequiredChecksums[32];

		int FileDescriptors[32];
		int64_t ZipSizes[32];

		unused(Checksums);

		/*if (unlikely(!BufferPointers))
				Error("[FATAL]: Failed to allocate BufferPointers during DownloadPackages call.", ERR_MEMORY);
		else if (unlikely(!LinkPointers))
				Error("[FATAL]: Failed to allocate LinkPointers during DownloadPackages call.", ERR_MEMORY);*/

		if (unlikely(!FullURL))
				Error("[FATAL]: Failed to allocate memory for FullURL during DownloadPackages call.", ERR_MEMORY);
		else if (unlikely(!ZipFilePath))
				Error("[FATAL]: Failed to allocate memory for ZipFilePath during DownloadPackages call.", ERR_MEMORY);

		memcpy(ZipFilePath, TEMP_PWOOTIE_FOLDER, RootPartLength);

		memcpy(FullURL, CDN_URL, LengthURL);
		memcpy(FullURL + LengthURL, Version, LengthVersion);
		FullURL[LengthURL + LengthVersion] = '-';

		/* Create a temp folder for downloads.
			* The installer function cleans it once installation finishes.*/
		if (unlikely(mkdir(TEMP_PWOOTIE_FOLDER, 0755) && errno != EEXIST)) {
				Error("[FATAL]: Failed to create new directory within /tmp/.", ERR_STANDARD | ERR_NOEXIT);
				goto error;
		}

		printf(" Downloading packages..\n");

		/* A for loop is used here just to keep Index at a local scope. A while loop would've worked too. */
		for (uint32_t Index = 0; Index < Fetched->TotalPackages;) {
				CURLMsg *MultiMsg;
				int32_t StillRunning = 1, MessagesLeft = 0;

				Increment = Fetched->TotalPackages - Index >= 32 ? 32 : Fetched->TotalPackages % 32;
				PackagesToDownload = 0;

				for (uint32_t LinkIndex = Index; LinkIndex < Increment + Index; LinkIndex++) {
						Package CurPackage = Fetched->PackageList[LinkIndex];

						if (!CurPackage.Download)
								continue;

						uint32_t  PackageNameLength = strlen(CurPackage.Name);

						/* The extra byte is obviously the dash. Well I suppose it's obvious. */
						memcpy(FullURL + LengthURL + LengthVersion + 1, CurPackage.Name, PackageNameLength);
						FullURL[LengthURL + LengthVersion + PackageNameLength + 1] = '\0';

						memcpy(ZipFilePath + RootPartLength, CurPackage.Name, PackageNameLength);
						ZipFilePath[RootPartLength + PackageNameLength] = '\0';

						LinkPointers[PackagesToDownload] = strdup(FullURL);
						BufferPointers[PackagesToDownload] = malloc(CurPackage.ZipSize);
						FileDescriptors[PackagesToDownload] = open(ZipFilePath, O_CREAT | O_RDWR | O_DIRECT, 0640);
						ZipSizes[PackagesToDownload] = CurPackage.ZipSize;
						RequiredChecksums[PackagesToDownload] = Fetched->PackageList[LinkIndex].Checksum;

						if (unlikely(!BufferPointers[PackagesToDownload])) {
								Error("[ERROR]: Unable to allocate one data buffer.", ERR_MEMORY);
						} else if (FileDescriptors[PackagesToDownload] == -1) {
								Error("[ERROR]: Unable to open file %s.", ERR_STANDARD, ZipFilePath);
								goto error;
						} else if (unlikely(!LinkPointers[PackagesToDownload])) {
								Error("[ERROR]: Unable to allocate link pointer during DownloadPackages call.", ERR_STANDARD | ERR_NOEXIT);
								goto error;
						}

						PackagesToDownload += 1;
				}

				/* We got no packages to download so may as well leave the loop early. */
				if (PackagesToDownload == 0)
						break;

				if (unlikely(CurlMultiSetup(BufferPointers, LinkPointers, PackagesToDownload) != 0)) {
						Error("[ERROR]: CurlMultiSetup fail.", ERR_STANDARD | ERR_NOEXIT);
						goto error;
				}

				/* https://curl.se/libcurl/c/multi-app.html */
				do {
						CURLMcode MultiCode = curl_multi_perform(CurlMulti, &StillRunning);
						printf("  Current running curl handles: %02i\r", StillRunning);
						fflush(stdout);

						if (StillRunning)
								MultiCode = curl_multi_poll(CurlMulti, NULL, 0, 1000, NULL);

						if (unlikely(MultiCode != CURLM_OK)) {
								Error("[ERROR]: MultiCode was not CURLM_OK during DownloadPackages. (cURL error: %s)", ERR_STANDARD | ERR_NOEXIT, curl_multi_strerror(MultiCode));
								Error((char*)curl_multi_strerror(MultiCode), ERR_STANDARD | ERR_NOEXIT);

								goto error;
						}
				} while (StillRunning);

				printf("\n");

				/* Make sure everything was downloaded. */
				while ((MultiMsg = curl_multi_info_read(CurlMulti, &MessagesLeft)) != NULL) {
						if (unlikely(MultiMsg->data.result != CURLE_OK)) {
								Error("[ERROR]: One or more packages failed to download.", ERR_STANDARD | ERR_NOEXIT);
								goto error;
						}
				}

				for (uint32_t LinkIndex = 0; LinkIndex < PackagesToDownload; LinkIndex++) {
						uint8_t   Checksum[16];
						char      ChecksumBuf[33];

						md5String(BufferPointers[LinkIndex], Checksum, ZipSizes[LinkIndex]);
						ChecksumToString(Checksum, ChecksumBuf);

						if (unlikely(memcmp(ChecksumBuf, RequiredChecksums[LinkIndex], 32) != 0)) {
								Error("[ERROR]: One or more packages' checksums are not matching.", ERR_STANDARD | ERR_NOEXIT);
								goto error;
						}

						write(FileDescriptors[LinkIndex], BufferPointers[LinkIndex], ZipSizes[LinkIndex]);
						close(FileDescriptors[LinkIndex]);

						free(BufferPointers[LinkIndex]);
						free(LinkPointers[LinkIndex]);
				}

				ResetMultiCurl(PackagesToDownload);
				Index += Increment;

				printf("  Batch downloaded!\n");
		}

		printf(" Download completed!\n");

		free(ZipFilePath);
		free(FullURL);

		return 0;
error:
		free(ZipFilePath);
		free(FullURL);

		for (uint8_t SrcIndex = 0; SrcIndex < PackagesToDownload; SrcIndex++) {
				free(LinkPointers[SrcIndex]);
				free(BufferPointers[SrcIndex]);
				close(FileDescriptors[SrcIndex]);
		}

		return -1;
}

FetchStruct* FetchPackages(char *restrict Version, char *restrict Checksums) {
		MemoryStruct ManifestContent;

		/* Last time I counted there were 35 packages. Hopefully I didn't count them wrong. */
		uint8_t PackageArraySize = 35, CurrentPackage = 0;

		char *FullURL = BuildString(3, CDN_URL, Version, PACKAGE_MANIFEST);
		Package *PackagesData = malloc(sizeof(Package) * PackageArraySize);
		FetchStruct *ReturnStruct = malloc(sizeof(FetchStruct));

		if (unlikely(!PackagesData))
				Error("[FATAL]: Failed to allocate PackagesData within GetPackages call.", ERR_MEMORY);
		else if (unlikely(!ReturnStruct))
				Error("[FATAL]: Failed to allocate ReturnStruct within GetPackages call.", ERR_MEMORY);

		ManifestContent.Memory = malloc(1);
		ManifestContent.Size = 0;

		/* Get the manifest containing all the packages information needed. */
		CURLcode Response = CurlGet(&ManifestContent, (char*)FullURL);

		if (unlikely(Response != CURLE_OK)) {
				Error("[ERROR]: GetPackages call failed to download the Manifest file. (cURL error: %s)", ERR_STANDARD | ERR_NOEXIT, curl_easy_strerror(Response));
				goto error;
		}

		/* Construct PackagesData. Inside the for loop we're also allocating more space for PackagesData if needed.
			* We're skipping first 3 bytes, aka "v0\n" */
		for (uint32_t i = 4; i < ManifestContent.Size; i++) {
				uint32_t SizePosition = 0, ZipSizePosition = 0, NamePosition = 0;
				/* Sizes need to be converted into numbers. */
				char SizeBuf[64], ZipSizeBuf[64];

				if (PackageArraySize == CurrentPackage) {
						/* Allocate PackageArraySize + 4 more space for packages, to avoid making a lot of expensive malloc calls.
							* We might be wasting a bit of memory here. */
						PackageArraySize += 4;
						Package *l_PackagesData = realloc(PackagesData, sizeof(Package) * PackageArraySize);

						if (unlikely(!l_PackagesData))
								Error("[FATAL]: Unable to reallocate PackagesData from GetPackages function.", ERR_MEMORY);

						PackagesData = l_PackagesData;
				}

				/* Maybe I could've did this section in a prettier way. Whatever.
					* We subtract current position by one because we have to clean the carriage return character. */
				/* First read the name of the package. */
				for (; ManifestContent.Memory[i] != '\n'; i++, NamePosition++)
						PackagesData[CurrentPackage].Name[NamePosition] = ManifestContent.Memory[i];
				PackagesData[CurrentPackage].Name[NamePosition - 1] = '\0';

				i++;

				/* Next read the checksum. We know the size of the checksum already. */
				memcpy(PackagesData[CurrentPackage].Checksum, ManifestContent.Memory + i, 32);
				i += 34; /* Skip \r and \n as well. */

				/* Read the compressed size. */
				for (; ManifestContent.Memory[i] != '\n'; i++, SizePosition++)
						ZipSizeBuf[SizePosition] = ManifestContent.Memory[i];
				ZipSizeBuf[SizePosition - 1] = '\0';

				i++;

				/* Read the uncompressed size. */
				for (; ManifestContent.Memory[i] != '\n'; i++, ZipSizePosition++)
						SizeBuf[ZipSizePosition] = ManifestContent.Memory[i];
				SizeBuf[ZipSizePosition - 1] = '\0';

				PackagesData[CurrentPackage].Size    = atoi(SizeBuf);
				PackagesData[CurrentPackage].ZipSize = atoi(ZipSizeBuf);

				/* Update LongestPackageName. */
				if (ReturnStruct->LongestName < NamePosition)
						ReturnStruct->LongestName = NamePosition;

				CurrentPackage++;
		}

		if (Checksums) {
				for (uint8_t i = 0; i < CurrentPackage; i++) {
						if (!strstr(Checksums, PackagesData[i].Checksum)) {
								PackagesData[i].Download = 1;
								continue;
						}

#ifndef NDEBUG
						printf("[DEBUG]: Skipping download for %s\n", PackagesData[i].Name);
#endif
						PackagesData[i].Download = 0;
				}
		}

		ReturnStruct->TotalPackages = CurrentPackage;
		ReturnStruct->PackageList   = PackagesData;

		free(FullURL);
		free(ManifestContent.Memory);
		return ReturnStruct;

error:
		free(PackagesData);
		free(FullURL);
		free(ManifestContent.Memory);
		return NULL;
}
