#include <Shared.h>
#include <errno.h>

FILE *PwootieFile = NULL;
char *PwootieBuffer = NULL;
uint32_t BufferSize = 0;

static void PwootieIncreaseBuffer(uint32_t Increase) {
		PwootieBuffer = realloc(PwootieBuffer, (BufferSize + Increase) * sizeof(char));

		if (unlikely(!PwootieBuffer))
				Error("[FATAL]: Unable to realloc PwootieBuffer during PwootieWriteEntry.", ERR_MEMORY);

		BufferSize += Increase;
}

/* OpenPwootieFile() opens the pwootie file.
	* @return 0 on success and -1 on failure. */
int8_t OpenPwootieFile() {
		/* Check if the file is already open. */
		if (unlikely(PwootieFile))
				return -1;

		char *Path = BuildString(5, getenv("HOME"), "/", INSTALL_DIR, "/", PWOOTIE_DATA);

		/* Try to open the file in r+ mode, if it fails then try creating it. */
		PwootieFile = fopen(Path, "r+");

		if (unlikely(!PwootieFile)) {
				PwootieFile = fopen(Path, "w+");

				/* Maybe creation failed. */
				if (unlikely(!PwootieFile)) {
						Error("[FATAL]: Unable to create PwootieFile.", ERR_STANDARD | ERR_NOEXIT);
						free(Path);
						return -1;
				}
		}

		/* Allocate the PwootieBuffer. */
		fseek(PwootieFile, 0, SEEK_END);

		BufferSize = ftell(PwootieFile);
		PwootieBuffer = malloc(sizeof(char) * BufferSize);

		if (unlikely(!PwootieBuffer))
				Error("[FATAL]: PwootieBuffer failed to be allocated during OpenPwootieFile.", ERR_MEMORY);

		fseek(PwootieFile, 0, SEEK_SET);
		fread(PwootieBuffer, BufferSize, sizeof(char), PwootieFile);

		/* Cleanup. */
		free(Path);

		return 0;
}

/* PwootieGetEntry()'s whole purpose is to get the start of the entry within the buffer.
	* @return -1 if the entry doesn't exist or a positive number representing the index of where the entry starts in the buffer. */
static int32_t PwootieGetEntry(char *Entry) {
		char EntryName[128];

		for (uint32_t SrcIndex = 0; SrcIndex < BufferSize; SrcIndex++) {
				uint8_t EntryIndex = 0;

				do {
						EntryName[EntryIndex] = PwootieBuffer[SrcIndex];
						EntryIndex++;
						SrcIndex++;

						if (unlikely(EntryIndex == 128))
								Error("[FATAL]: EntryIndex reached number 128. Is the PwooteFile corrupt?", ERR_STANDARD);
						else if (unlikely(SrcIndex == BufferSize)) {
								return -1;
						}
				} while (PwootieBuffer[SrcIndex] != '=');

				EntryName[EntryIndex] = '\0';

				if (strcmp(EntryName, Entry) == 0)
						return SrcIndex - EntryIndex;

				while (PwootieBuffer[SrcIndex] != '\n' && SrcIndex < BufferSize)
						SrcIndex++;
		}

		return -1;
}

/* This is the helper function which reads the content of an entry.
	* @return NULL if the entry wasn't added yet.*/
char* PwootieReadEntry(char *Entry, uint32_t ExtraBytes) {
		/* Is the PwootieFile open? */
		if (unlikely(!PwootieFile))
				return NULL;

		uint32_t  DataSize = 1, DataIndex = 0, BufferIndex = 0;
		int32_t   EntryStart = PwootieGetEntry(Entry);

		/* No entry to read from. */
		if (EntryStart == -1)
				return NULL;

		char *Data = malloc(1 * sizeof(char));

		BufferIndex = (uint32_t)EntryStart;

		while (PwootieBuffer[BufferIndex] != '=')
				BufferIndex++;
		BufferIndex++;

		while (PwootieBuffer[BufferIndex] != '\n') {
				if (DataIndex == DataSize) {
						DataSize *= 2;
						Data = realloc(Data, sizeof(char) * (DataSize + 1));

						if (unlikely(!Data))
								Error("[FATAL]: Unable to allocate Data during PwootieReadEntry.", ERR_MEMORY);
				}

				Data[DataIndex] = PwootieBuffer[BufferIndex];
				BufferIndex++;
				DataIndex++;
		}

		if (DataSize - ExtraBytes < ExtraBytes) {
				Data = realloc(Data, sizeof(char) * (DataSize + (DataSize - ExtraBytes) + 1));

				if (unlikely(!Data))
						Error("[FATAL]: Unable to realloc Data during PwootieReadEntry.", ERR_MEMORY);
		}

		Data[DataIndex] = '\0';

		return Data;
}

/* PwootieExit() writes the file and closes the file. This is called at the end of the program.
	* @return nothing */
void PwootieExit() {
		if (unlikely(!PwootieFile))
				return;

		char *Path = BuildString(5, getenv("HOME"), "/", INSTALL_DIR, "/", PWOOTIE_DATA);

		fclose(PwootieFile);
		PwootieFile = fopen(Path, "w");

		if (unlikely(!PwootieFile))
				Error("[FATAL]: Unable to reopen PwootieFile during PwootieExit.", ERR_STANDARD);

		fwrite(PwootieBuffer, sizeof(char), BufferSize, PwootieFile);

		free(PwootieBuffer);
		free(Path);

		fclose(PwootieFile);

		PwootieBuffer = NULL;
		PwootieFile = NULL;
}

/* PwootieWriteEntry() writes an entry to the buffer.
	* @return nothing */
void PwootieWriteEntry(char *restrict Entry, char *restrict Data) {
		uint32_t EntrySize = strlen(Entry);
		uint32_t DataSize = strlen(Data);
		uint32_t RequiredSize = EntrySize + DataSize + 2; /* 2 for the equal sign and the newline byte */
		uint32_t Newline;

		int32_t NewEntrySize;
		int32_t EntryIndex = PwootieGetEntry(Entry);

		if (EntryIndex == -1) {
				PwootieIncreaseBuffer(RequiredSize);
				EntryIndex = BufferSize - RequiredSize;
		} else {
				Newline = EntryIndex;

				while (PwootieBuffer[Newline] != '\n' && Newline != BufferSize)
						Newline++;

				NewEntrySize = RequiredSize - (Newline - EntryIndex + 1);

				if (NewEntrySize > 0)
						PwootieIncreaseBuffer((uint32_t)NewEntrySize);

				if (Newline == BufferSize - NewEntrySize - 1) {
						if (NewEntrySize < 0)
								BufferSize += NewEntrySize - 1;
				} else {
						memmove(PwootieBuffer + Newline + NewEntrySize, PwootieBuffer + Newline, (BufferSize - Newline - NewEntrySize + 1) * sizeof(char));

						if (NewEntrySize < 0)
								BufferSize += NewEntrySize;
				}
		}

		/* Write the entry. */
		memcpy(PwootieBuffer + EntryIndex, Entry, EntrySize);
		EntryIndex += EntrySize;
		PwootieBuffer[EntryIndex] = '=';

		EntryIndex += 1;

		memcpy(PwootieBuffer + EntryIndex, Data, DataSize);
		EntryIndex += DataSize;
		PwootieBuffer[EntryIndex] = '\n';
}
