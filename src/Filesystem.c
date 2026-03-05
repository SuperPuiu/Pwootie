#include <Shared.h>

/* TODO: Implement a non-linux dependent solution for other platforms. */
#include <sys/sendfile.h>
#include <sys/stat.h>

#include <errno.h>
#include <stdarg.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>

static char *NewCopyPath;
static uint32_t NewCopyPathRootPos, OldCopyPathRootPos;

/* Used internally by nftw.
	* @return 0 on success and -1 on failure.*/
int32_t DeleteFile(const char *FilePath, const struct stat*, int32_t, struct FTW *) {
		if (unlikely(remove(FilePath) < 0)) {
				Error("DeleteFile() call failed on %s.", ERR_STANDARD | ERR_NOEXIT, FilePath);
				return -1;
		}

		return 0;
}

/* Used internally by nftw.
	* @return 0 on success and -1 on failure. */
static int32_t CopyEntry(const char *FilePath, const struct stat *StatBuffer, int32_t FileType, struct FTW*) {
		if (FileType == FTW_F) {
				int InputDescriptor, OutputDescriptor;
				off_t Copied = 0;

				InputDescriptor = open(FilePath, O_RDONLY, 0640);

				if (unlikely(InputDescriptor == -1)) {
						Error("[ERROR]: Unable to open InputDescritor %s.", ERR_STANDARD | ERR_NOEXIT, FilePath);
						return -1;
				}

				memcpy(NewCopyPath + NewCopyPathRootPos, FilePath + OldCopyPathRootPos, strlen(FilePath + OldCopyPathRootPos) + 1);
				OutputDescriptor = open(NewCopyPath, O_RDWR | O_CREAT, 0640);

				if (unlikely(OutputDescriptor == -1)) {
						Error("[ERROR]: Unable to open OutputDescriptor %s.", ERR_STANDARD | ERR_NOEXIT, NewCopyPath);
						close(InputDescriptor);
						return -1;
				}

				/* https://stackoverflow.com/questions/2180079/how-can-i-copy-a-file-on-unix-using-c */
				do {
						ssize_t Written = sendfile(OutputDescriptor, InputDescriptor, &Copied, SSIZE_MAX);

						if (unlikely(Written == -1)) {
								Error("[ERROR]: sendfile() failure.", ERR_STANDARD | ERR_NOEXIT);
								close(InputDescriptor);
								close(OutputDescriptor);
								return -1;
						}

						Copied += Written;
				} while (Copied < StatBuffer->st_size);

				close(InputDescriptor);
				close(OutputDescriptor);
				return 0;
		} else if (FileType == FTW_D) {
				uint32_t Len = strlen(FilePath + OldCopyPathRootPos);
				memcpy(NewCopyPath + NewCopyPathRootPos, FilePath + OldCopyPathRootPos, Len);
				NewCopyPath[NewCopyPathRootPos + Len] = '/';
				NewCopyPath[NewCopyPathRootPos + Len + 1] = 0;

				if (unlikely(mkdir(NewCopyPath, 0755) && errno != EEXIST)) {
						Error("[ERROR]: mkdir failure on %s.", ERR_STANDARD | ERR_NOEXIT, NewCopyPath);
						return -1;
				}

				return 0;
		} else {
				Error("[ERROR]: %s is not an entry we can copy.", ERR_STANDARD | ERR_NOEXIT);
				return -1;
		}

		return 0;
}

/* CopyRelativeDir copies one relative folder to another.
	* Basically, the char* arguments are appended to `$HOME/.robloxstudio/` for the initial paths.
	* @return 0 on success and -1 on failure. */
int8_t CopyRelativeDir(char *Old, char *New) {
		char *OldCopyPath = GetVersionPath(Old, 256);
		NewCopyPath = GetVersionPath(New, 256);

		NewCopyPathRootPos = strlen(NewCopyPath);
		OldCopyPathRootPos = strlen(OldCopyPath);

		if (unlikely(mkdir(NewCopyPath, 0755) && errno != EEXIST)) {
				Error("[ERROR]: Unable to make directory %s.", ERR_STANDARD | ERR_NOEXIT, NewCopyPath);
				goto error;
		}

		if (unlikely(nftw(OldCopyPath, CopyEntry, 10, FTW_MOUNT | FTW_PHYS)))
				goto error;

		free(OldCopyPath);
		free(NewCopyPath);
		return 0;

error:
		free(OldCopyPath);
		free(NewCopyPath);
		return -1;
}

/* ReadFileToBuffer reads the whole provided file to a dynamically allocated buffer.
	* If provided, PtrSize is set to the maximum size of the file.
	* @return always a buffer. */
char *ReadFileToBuffer(FILE *Ptr, uint32_t *PtrSize) {
		uint32_t Size;
		char *Buffer;

		fseek(Ptr, 0, SEEK_END);
		Size = ftell(Ptr);
		fseek(Ptr, 0, SEEK_SET);

		Buffer = malloc(Size * sizeof(char));

		if (unlikely(!Buffer))
				Error("[FATAL]: Unable to allocate Buffer during ReadFileToBuffer.", ERR_MEMORY);

		fread(Buffer, Size, sizeof(char), Ptr);
		fseek(Ptr, 0, SEEK_SET);

		if (PtrSize)
				*PtrSize = Size;

		return Buffer;
}

/* GetVersionPath() constructs the path to the provided studio version. User must free this manually.
	* @return always the version path. */
char *GetVersionPath(char *Version, uint32_t ExtraBytes) {
		uint32_t InstallDirLength = strlen(INSTALL_DIR), HomeLength = strlen(getenv("HOME"));
		uint32_t VersionLength = strlen(Version);
		uint32_t Total = InstallDirLength + HomeLength + VersionLength + 3;

		char *VersionPath = malloc((Total + ExtraBytes) * sizeof(char));

		if (unlikely(!VersionPath))
				Error("[FATAL]: Unable to allocate VersionPath during DeleteVersion call.", ERR_MEMORY);

		memcpy(VersionPath, getenv("HOME"), HomeLength);
		memcpy(VersionPath + HomeLength + 1, INSTALL_DIR, InstallDirLength);
		memcpy(VersionPath + HomeLength + InstallDirLength + 2, Version, VersionLength);
		VersionPath[HomeLength] = VersionPath[HomeLength + InstallDirLength + 1] = '/';
		VersionPath[Total - 1] = '\0';

		return VersionPath;
}

/* BuildDirectoryTree() builds a path of directories.
	* Use SkipBytes if you want to control where the search for the delimiter begins.
	* @return 0 on success and -1 on failure. */
int8_t BuildDirectoryTree(char *Path, uint32_t SkipBytes) {
		/* https://stackoverflow.com/questions/7430248/creating-a-new-directory-in-c
			* Basically, get a pointer to the next separator, replace it with a null byte, create the directory,
			* then revert the string modification and repeat. Pretty sure this is not thread safe but we don't care. */
		char *Separated = strchr(Path + 1 + SkipBytes, '/');

		while (Separated != NULL) {
				*Separated = '\0';

				if (unlikely(mkdir(Path, 0755) && errno != EEXIST)) {
						Error("[FATAL]: Mkdir failed during BuildDirectoryTree for %s call.", ERR_STANDARD | ERR_NOEXIT, Path);
						return -1;
				}

				*Separated = '/';
				Separated = strchr(Separated + 1, '/');
		}

		return 0;
}

/* ConvertPath() converts a path from a windows valid path to an unix valid path.
	* @return NULL all time. */
void ConvertPath(char *Path) {
		uint32_t PathLen = strlen(Path);

		for (uint8_t SrcIndex = 0; SrcIndex < PathLen; SrcIndex++) {
				if (Path[SrcIndex] == 92) {
						Path[SrcIndex] = '/';
						SrcIndex++;

						if (Path[SrcIndex] == 92) {
								memmove(Path + SrcIndex, Path + SrcIndex + 1, (PathLen - SrcIndex - 1) * sizeof(char));
								PathLen -= 1;
						}
				}
		}

		/* Hide bugs under the rug because we're good programmers. I don't know why this is required really. */
		Path[PathLen] = '\0';
}

/* BuildString(uint8_t Elements, ...) is usually used for paths, thus it's placement in Filesystem.c.
	* The caller must free the string.
	* @return the built char. */
char *BuildString(uint8_t Elements, ...) {
		va_list Arguments;
		uint64_t Size = 1, LastIndex = 0;
		uint64_t AllSizes[Elements];
		char *Str = NULL;

		/* Collect the string lengths. */
		va_start(Arguments, Elements);

		for (uint8_t Arg = 0; Arg < Elements; Arg++) {
				uint32_t StrLen = strlen(va_arg(Arguments, char*));
				Size += StrLen;
				AllSizes[Arg] = StrLen;
		}

		/* Reset the list and now concatenate the strings. */
		va_start(Arguments, Elements);
		Str = malloc(Size * sizeof(char));

		if (unlikely(!Str))
				Error("[FATAL]: Failed to allocate new string memory during BuildString call.\n", ERR_MEMORY);

		for (uint8_t Arg = 0; Arg < Elements; Arg++) {
				memcpy(Str + LastIndex, va_arg(Arguments, char*), AllSizes[Arg]);
				LastIndex += AllSizes[Arg];
		}

		Str[Size - 1] = '\0';

		va_end(Arguments);

		return Str;
}

uint64_t QueryDiskSpace() {
		return 0;
}
