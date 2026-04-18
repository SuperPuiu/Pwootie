#include "../include/Shared.h"

#include <sys/stat.h>
#include <errno.h>
#include <zip.h>
#include "../include/md5.h"

#include <fcntl.h>
#include <unistd.h>

#define OFFICIAL_INSTALLER  "RobloxStudioInstaller.exe"
#define APP_SETTINGS        "AppSettings.xml"
#define APP_SETTINGS_DATA   "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n<Settings>\r\n        <ContentFolder>content</ContentFolder>\r\n        <BaseUrl>http://www.roblox.com</BaseUrl>\r\n</Settings>\r\n"

typedef struct {
    char *filename;
    int index;
    int total;
    int percent;
    uint64_t downloaded;
    uint64_t total_size;
    curl_off_t last_dlnow;
} ParallelProgress;

static uint64_t g_total_downloaded = 0;
static uint64_t g_grand_total = 0;

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
		char *Checksums = malloc((Fetched->TotalPackages * ChecksumSize + Fetched->TotalPackages + 1) * sizeof(char));

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

int8_t InstallPackages(FetchStruct *Fetched, ZipMemoryStruct *ZipData, char *Version) {
		CURLcode  Response;
		FILE      *Installer;

		struct zip_stat *ZipStat  = NULL;

		uint32_t SettingsLen      = strlen(APP_SETTINGS_DATA), LengthVersion = strlen(Version);
		uint32_t InstallerLength  = strlen(OFFICIAL_INSTALLER), TempDirLength = strlen(TEMP_PWOOTIE_FOLDER);
		uint32_t InstallDirLength = strlen(INSTALL_DIR), HomeLength = strlen(getenv("HOME"));
		uint32_t AppSettingsLen   = strlen(APP_SETTINGS);

		/* The 'Official' string is built inside of this function only to reuse it later. Same for 'InstallDir'. */
		char **Instructions   = NULL;
		char *InstallDir      = NULL;
		char *Official        = malloc((InstallerLength + TempDirLength + 64 + 1) * sizeof(char));
		char *FullURL         = BuildString(4, CDN_URL, Version, "-", OFFICIAL_INSTALLER);

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
				uint32_t        InstructionLength = strlen(Instructions[i]);
				zip_error_t     ZipError;
				zip_t           *ZipPointer;
				zip_source_t    *ZipSource;
				zip_file_t      *FileDescriptor;

				char            *Memory;
				FILE            *NewFile;

				ZipStat = malloc(sizeof(struct zip_stat));

				if (unlikely(!ZipStat))
						Error("[FATAL]: Unable to allocate ZipStat during InstallPackages call.", ERR_MEMORY);

				/* Construct required installation path. */
				memcpy(InstallDir + HomeLength + InstallDirLength + LengthVersion + 3, Instructions[i], InstructionLength + 1);

				ZipSource = zip_source_buffer_create(ZipData[i].Data, ZipData[i].Size, 1, &ZipError);

				if (unlikely(!ZipSource)) {
						Error("[ERROR]: Unable to create ZipSource out of buffer %i. (zip error: %s)", ERR_STANDARD | ERR_NOEXIT, i, zip_error_strerror(&ZipError));
						goto error;
				}

				ZipPointer = zip_open_from_source(ZipSource, 0, &ZipError);

				if (unlikely(!ZipPointer)) {
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

		if (unlikely(nftw(Official, DeleteFile, 10, FTW_DEPTH | FTW_MOUNT | FTW_PHYS) < 0))
				Error("[ERROR]: Failed to remove the Pwootie temporary directory (%s).", ERR_STANDARD | ERR_NOEXIT, Official);

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

/* Progress callback for parallel downloads */
static int ParallelProgressCallback(void *clientp, curl_off_t dltotal, curl_off_t dlnow,
                                     curl_off_t ultotal, curl_off_t ulnow) {
    ParallelProgress *p = (ParallelProgress*)clientp;

    (void)ultotal;
    (void)ulnow;

    if (dltotal > 0) {
        int new_percent = (int)((double)dlnow / (double)dltotal * 100);

        if (new_percent != p->percent) {
            /* Update global downloaded counter */
            static curl_off_t old_dlnow[64] = {0};
            static time_t last_time = 0;
            static curl_off_t last_bytes[64] = {0};

            if (dlnow > old_dlnow[p->index - 1]) {
                g_total_downloaded += (dlnow - old_dlnow[p->index - 1]);
                old_dlnow[p->index - 1] = dlnow;
            }
            p->percent = new_percent;

            /* Calculate speed */
            double speed = 0;
            time_t now = time(NULL);
            if (now != last_time && last_bytes[p->index - 1] > 0) {
                speed = (double)(dlnow - last_bytes[p->index - 1]) / (now - last_time) / 1024;
                last_bytes[p->index - 1] = dlnow;
                last_time = now;
            } else if (last_bytes[p->index - 1] == 0) {
                last_bytes[p->index - 1] = dlnow;
                last_time = now;
            }

            /* Move to this file's line and clear it */
            printf("\033[%d;1H", p->index + 2);
            printf("\033[2K");

            /* Print: [num/total] filename (current/total MB) speed KB/s percent% */
            printf("  [%3d/%3d] %-35s (%6.2f / %6.2f MB) %7.2f KB/s %3d%%",
                   p->index, p->total, p->filename,
                   (double)dlnow / 1024 / 1024,
                   (double)dltotal / 1024 / 1024,
                   speed, new_percent);

            /* Update total progress at bottom */
            if (g_grand_total > 0) {
                int total_percent = (int)((double)g_total_downloaded / (double)g_grand_total * 100);
                printf("\033[%d;1H", p->total + 3);
                printf("\033[2K");
                printf("  Total Progress: %3d%% (%.2f / %.2f MB)", total_percent,
                       (double)g_total_downloaded / 1024 / 1024,
                       (double)g_grand_total / 1024 / 1024);
            }

            fflush(stdout);
        }
    }
    return 0;
}
int8_t DownloadPackages(FetchStruct *Fetched, ZipMemoryStruct *ZipData, char *Version) {
    uint32_t LengthURL = strlen(CDN_URL), LengthVersion = strlen(Version);
    char *FullURL = malloc((LengthURL + LengthVersion + Fetched->LongestName + 2) * sizeof(char));
    CURL *handles[64];
    ParallelProgress progress[64];
    MemoryStruct buffers[64];
    CURLM *multi = curl_multi_init();

    if (unlikely(!FullURL))
        Error("[FATAL]: Failed to allocate FullURL.", ERR_MEMORY);

    memcpy(FullURL, CDN_URL, LengthURL);
    memcpy(FullURL + LengthURL, Version, LengthVersion);
    FullURL[LengthURL + LengthVersion] = '-';

    /* Calculate grand total size */
    g_grand_total = 0;
    for (uint32_t i = 0; i < Fetched->TotalPackages; i++) {
        g_grand_total += Fetched->PackageList[i].ZipSize;
    }
    g_total_downloaded = 0;

    /* Clear screen and print header */
    printf("\033[2J\033[H");
    printf(" Downloading packages..\n\n");

    /* Reserve lines for each file */
    for (uint32_t i = 0; i < Fetched->TotalPackages; i++) {
        Package *pkg = &Fetched->PackageList[i];
        printf("  [%3d/%3d] %-35s (%6.2f / %6.2f MB) %7.2f KB/s %3d%%\n",
               i + 1, Fetched->TotalPackages, pkg->Name,
               0.0, (double)pkg->ZipSize / 1024 / 1024, 0.0, 0);
    }

    /* Print total progress line */
    printf("\n  Total Progress:   0%% (0.00 / 0.00 MB)\n");
    fflush(stdout);

    /* Start all downloads in parallel */
    for (uint32_t i = 0; i < Fetched->TotalPackages; i++) {
        Package *pkg = &Fetched->PackageList[i];
        uint32_t name_len = strlen(pkg->Name);

        buffers[i].Memory = malloc(pkg->ZipSize);
        buffers[i].Size = 0;

        if (unlikely(!buffers[i].Memory)) {
            Error("[ERROR]: Failed to allocate buffer for %s", ERR_MEMORY, pkg->Name);
            continue;
        }

        memcpy(FullURL + LengthURL + LengthVersion + 1, pkg->Name, name_len + 1);

        handles[i] = curl_easy_init();
        curl_easy_setopt(handles[i], CURLOPT_URL, FullURL);
        curl_easy_setopt(handles[i], CURLOPT_WRITEFUNCTION, WriteDataCallbackSimple);
        curl_easy_setopt(handles[i], CURLOPT_WRITEDATA, &buffers[i]);
        curl_easy_setopt(handles[i], CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(handles[i], CURLOPT_USERAGENT, "PwootieDownload/1.0");
        curl_easy_setopt(handles[i], CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(handles[i], CURLOPT_XFERINFOFUNCTION, ParallelProgressCallback);

        progress[i].filename = pkg->Name;
        progress[i].index = i + 1;
        progress[i].total = Fetched->TotalPackages;
        progress[i].percent = 0;
        progress[i].downloaded = 0;
        progress[i].total_size = pkg->ZipSize;

        curl_easy_setopt(handles[i], CURLOPT_XFERINFODATA, &progress[i]);

        curl_multi_add_handle(multi, handles[i]);
    }

    /* Main download loop */
    int still_running;
    do {
        curl_multi_perform(multi, &still_running);
        curl_multi_poll(multi, NULL, 0, 100, NULL);
    } while (still_running);

    printf("\n");

    /* Collect results and verify checksums */
    for (uint32_t i = 0; i < Fetched->TotalPackages; i++) {
        uint8_t Checksum[16];
        char ChecksumBuf[33];
        Package *pkg = &Fetched->PackageList[i];

        md5String(buffers[i].Memory, Checksum, buffers[i].Size);
        ChecksumToString(Checksum, ChecksumBuf);

        if (unlikely(memcmp(ChecksumBuf, pkg->Checksum, 32) != 0)) {
            Error("[ERROR]: Checksum mismatch for %s.", ERR_STANDARD | ERR_NOEXIT, pkg->Name);
            return -1;
        }

        ZipData[i].Data = buffers[i].Memory;
        ZipData[i].Size = buffers[i].Size;
        curl_multi_remove_handle(multi, handles[i]);
        curl_easy_cleanup(handles[i]);
    }

    curl_multi_cleanup(multi);
    free(FullURL);
    printf("\n Download completed! (%.2f MB total)\n", (double)g_grand_total / 1024 / 1024);
    return 0;
}

FetchStruct* FetchPackages(ZipMemoryStruct **ZipData, char *restrict Version, char *restrict Checksums) {
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
						if (!strstr(Checksums, PackagesData[i].Checksum))
								continue;

						#ifndef NDEBUG
						printf("[DEBUG]: Skipping download for %s\n", PackagesData[i].Name);
						#endif
						memmove(PackagesData + i, PackagesData + i + 1, (CurrentPackage - i - 1) * sizeof(Package));
						CurrentPackage -= 1;
				}
		}

		*ZipData = malloc(sizeof(ZipMemoryStruct) * CurrentPackage);

		if (unlikely(!*ZipData))
				Error("[FATAL]: Unable to allocate ZipData struct.", ERR_MEMORY);

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
