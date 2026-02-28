#include <Shared.h>
#include <errno.h>
#include <dirent.h>

/* Functions found in Packages.c */
int8_t      DownloadPackages(FetchStruct *Fetched, char *restrict Version, char *restrict Checksums);
int8_t      InstallPackages(FetchStruct *Fetched, char *restrict Version, char *restrict Checksums);
char        *FormatChecksums(FetchStruct *Fetched);
FetchStruct *FetchPackages(char *restrict Version, char *restrict Checksums);

void DeleteVersion(char *Version) {
		char *VersionPath = GetVersionPath(Version, 0);

		if (unlikely(nftw(VersionPath, DeleteFile, 10, FTW_DEPTH | FTW_MOUNT | FTW_PHYS) < 0))
				Error("[ERROR]: Failed to remove %s.", ERR_STANDARD | ERR_NOEXIT, VersionPath);

		free(VersionPath);
}

uint8_t VersionFolderExists(char *Version) {
		/* We could have more checks but whatever. */
		char *VersionPath = GetVersionPath(Version, 0);
		DIR *VersionDirectory = opendir(VersionPath);

		if (VersionDirectory && errno != ENOENT) {
				closedir(VersionDirectory);
				return 1;
		}

		free(VersionPath);
		return 0;
}

/* Install() function handles the installation process using the functions found in Packages.c
	* @return -1 on failure and 0 on success. */
int8_t Install(char *Version, uint8_t CheckVersion) {
		printf("[INFO]: Installing %s.\n", Version);

		/* Before starting, let's check if we already have the right version installed.
			* This process may be skipped by setting CheckVersion flag to 0.*/
		int8_t Status = 0;

		FetchStruct *Fetched = NULL;
		char *LastVersion = NULL;
		char *Checksums = NULL;

		if (PwootieFile) {
				Checksums = PwootieReadEntry("checksums", 0);
				LastVersion = PwootieReadEntry("version", 0);

				if (CheckVersion && LastVersion) {
						if (strcmp(LastVersion, Version) == 0) {
								printf("[INFO]: Roblox studio seems to be up-to-date. Aborting installation process.\n");
								free(LastVersion);
								return 0;
						} else if (VersionFolderExists(Version)) {
								printf("[INFO]: %s folder found. Using that as current roblox version.\n", Version);
								free(LastVersion);
								return 0;
						}
				}
		}

		/* First step: get a list of packages we have to download, along with their checksum, real size and compressed size.
			* FetchStruct also contains some additional information helpful to other functions. */
		Fetched = FetchPackages(Version, Checksums);
		if (unlikely(!Fetched))
				goto error;

		/* Second step: download the packages using the data we got above. */
		Status = DownloadPackages(Fetched, Version, Checksums);
		if (unlikely(Status == -1))
				goto error;

		/* Last step: install the packages. */
		Status = InstallPackages(Fetched, Version, Checksums);
		if (unlikely(Status == -1))
				goto error;

		/* Open the Pwootie file, if it isn't open already. */
		if (unlikely(!PwootieFile))
				OpenPwootieFile();

		/* Very important: download wine and setup the custom WINE prefix for studio. */
		Status = SetupWine(1);
		if (unlikely(Status == -1))
				goto error;

		Status = SetupPrefix();
		if (unlikely(Status == -1))
				goto error;

		PwootieWriteEntry("version", Version);

		/* Create the fast flags file. */
		CreateFFlags(Version, LastVersion);

		/* Delete the old version, if it exists and if the user agrees. */
		if (LastVersion) {
				if (strcmp(LastVersion, Version) != 0) {
						if (AskForConfirmation("Delete the old installed version of studio? [Y/N]"))
								DeleteVersion(LastVersion);
				}

				free(LastVersion);
		}

		/* Store the new checksums. */
		Checksums = FormatChecksums(Fetched);
		PwootieWriteEntry("checksums", Checksums);

		/* Free up the used memory. */
		free(Fetched->PackageList);
		free(Fetched);
		free(Checksums);

		return 0;

error:
		if (Fetched) {
				free(Fetched->PackageList);
				free(Fetched);
		}

		if (LastVersion)
				free(LastVersion);

		if (Checksums)
				free(Checksums);

		Error("[ERROR]: An error was encountered while running Install().", ERR_STANDARD | ERR_NOEXIT);

		return -1;
}
