#include "../include/Shared.h"
#include <string.h>

#define CURL_ARRAY_SIZE 32

typedef struct {
		FILE *FileStream;
		char *RemoteFileName, *DownloadPath;
		uint32_t RemoteNameSize, FileNameSize;
} DownloadStruct;

static CURL *CurlHandle;
static CURL *CurlDownloadHandle;
static CURL *CurlDownloadArray[CURL_ARRAY_SIZE];

static MemoryStruct MultiHandlesStructs[CURL_ARRAY_SIZE];

CURLM *CurlMulti;

static size_t WriteMemoryCallback    (void *Contents, size_t Size, size_t DataSize, void *UserPointer);
static size_t WriteFileCallback      (void *Contents, size_t _Size, size_t DataSize, void *DownloadInfo);
static size_t ParseDownloadHeader    (void *HeaderPtr, size_t Size, size_t DataSize, void *Info);
size_t WriteDataCallbackSimple(void *Contents, size_t Size, size_t DataSize, void *UserPointer);
/* Progress callback data */
typedef struct {
    char *filename;
    int current;
    int total;
    uint64_t size;
} ProgressData;

/* Progress callback for download percentage */
static int DownloadProgressCallback(void *clientp, curl_off_t dltotal, curl_off_t dlnow,
                                     curl_off_t ultotal, curl_off_t ulnow) {
    ProgressData *data = (ProgressData*)clientp;
    (void)ultotal;
    (void)ulnow;

    if (dltotal > 0 && data->filename) {
        int percent = (int)((double)dlnow / (double)dltotal * 100);
        printf("\r  [%d/%d] %s: %3d%% (%lld/%lld bytes)",
               data->current, data->total, data->filename,
               percent, (long long)dlnow, (long long)dltotal);
        fflush(stdout);
    }
    return 0;
}

void SetupHandles() {
		CurlHandle = curl_easy_init(), CurlDownloadHandle = curl_easy_init();

		curl_easy_setopt(CurlHandle, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(CurlHandle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
		curl_easy_setopt(CurlHandle, CURLOPT_USERAGENT, "PwootieFetch/1.0");

		curl_easy_setopt(CurlDownloadHandle, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(CurlDownloadHandle, CURLOPT_WRITEFUNCTION, WriteFileCallback);
		curl_easy_setopt(CurlDownloadHandle, CURLOPT_USERAGENT, "PwootieDownload/1.0");
		curl_easy_setopt(CurlDownloadHandle, CURLOPT_HEADERFUNCTION, ParseDownloadHeader);

		CurlMulti = curl_multi_init();

		for (uint32_t CurrentIndex = 0; CurrentIndex < CURL_ARRAY_SIZE; CurrentIndex++) {
				CurlDownloadArray[CurrentIndex] = curl_easy_init();

				curl_easy_setopt(CurlDownloadArray[CurrentIndex], CURLOPT_FOLLOWLOCATION, 1L);
				curl_easy_setopt(CurlDownloadArray[CurrentIndex], CURLOPT_WRITEFUNCTION, WriteDataCallbackSimple);
				curl_easy_setopt(CurlDownloadArray[CurrentIndex], CURLOPT_USERAGENT, "PwootieDownload/1.0");
		}
}

void ResetMultiCurl(uint16_t Total) {
		for (uint16_t CurrentIndex = 0; CurrentIndex < Total; CurrentIndex++)
				curl_multi_remove_handle(CurlMulti, CurlDownloadArray[CurrentIndex]);
}

CURLcode CurlGet(MemoryStruct *Chunk, char *WithURL) {
		curl_easy_setopt(CurlHandle, CURLOPT_URL, WithURL);
		curl_easy_setopt(CurlHandle, CURLOPT_WRITEDATA, (void *)Chunk);
		return curl_easy_perform(CurlHandle);
}

int8_t CurlMultiSetup(char **Buffers, char **Links, uint16_t Total, void *ProgressDataVoid) {
    ProgressData *ProgressArray = (ProgressData*)ProgressDataVoid;

    if (unlikely(Total > CURL_ARRAY_SIZE))
        return -1;

    for (uint16_t CurrentIndex = 0; CurrentIndex < Total; CurrentIndex++) {
        MultiHandlesStructs[CurrentIndex].Memory = Buffers[CurrentIndex];
        MultiHandlesStructs[CurrentIndex].Size = 0;

        curl_easy_setopt(CurlDownloadArray[CurrentIndex], CURLOPT_URL, Links[CurrentIndex]);
        curl_easy_setopt(CurlDownloadArray[CurrentIndex], CURLOPT_WRITEDATA, &MultiHandlesStructs[CurrentIndex]);
        curl_easy_setopt(CurlDownloadArray[CurrentIndex], CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(CurlDownloadArray[CurrentIndex], CURLOPT_XFERINFOFUNCTION, DownloadProgressCallback);

        if (ProgressArray) {
            curl_easy_setopt(CurlDownloadArray[CurrentIndex], CURLOPT_XFERINFODATA, &ProgressArray[CurrentIndex]);
        }

        curl_multi_add_handle(CurlMulti, CurlDownloadArray[CurrentIndex]);
    }

    return 0;
}

static int8_t GetNameFromContent(char const *ContentDisposition, DownloadStruct *StructPtr) {
		char const *Key = "filename=";
		char *Value = NULL;
		uint16_t SrcIndex = 0;

		Value = strcasestr(ContentDisposition, Key);

		if (unlikely(!Value)) {
				Error("[ERROR]: No key value found in Content-disposition.", ERR_STANDARD | ERR_NOEXIT);
				return -1;
		}

		/* Move to the value's content. */
		Value += strlen(Key);

		for (; Value[SrcIndex] != '\0' && Value[SrcIndex] != ';' && Value[SrcIndex] != '\n' && Value[SrcIndex] != '\r'; SrcIndex++) {
				if (StructPtr->RemoteNameSize == SrcIndex) {
						StructPtr->RemoteNameSize *= 2;
						StructPtr->RemoteFileName = realloc(StructPtr->RemoteFileName, StructPtr->RemoteNameSize * sizeof(char));

						if (unlikely(!StructPtr->RemoteFileName))
								Error("[FATAL]: Unable to realloc RemoteFileName.", ERR_MEMORY);
				}

				StructPtr->RemoteFileName[SrcIndex] = Value[SrcIndex];
		}

		StructPtr->RemoteFileName[SrcIndex] = '\0';
		StructPtr->FileNameSize = SrcIndex;
		return 0;
}

static size_t ParseDownloadHeader (void *HeaderPtr, size_t Size, size_t DataSize, void *Info) {
		const size_t RealSize = Size * DataSize;
		const char *Header = HeaderPtr;
		const char *Tag = "Content-disposition:";
		DownloadStruct *DownloadInfo = Info;

		if (DownloadInfo->FileStream)
				return RealSize;

		if (strncasecmp(Header, Tag, strlen(Tag)) != 0)
				return RealSize;

		if (unlikely(GetNameFromContent(Header + strlen(Tag), DownloadInfo) == -1))
				Error("Failed to get any type of remote name.\n", ERR_STANDARD | ERR_NOEXIT);
		printf("Remote file name: %s\n", DownloadInfo->RemoteFileName);

		return RealSize;
}

CURLcode CurlDownload(FILE *File, char *WithURL) {
		DownloadStruct Temp = {
				.FileStream = File
		};

		curl_easy_setopt(CurlDownloadHandle, CURLOPT_URL, WithURL);
		curl_easy_setopt(CurlDownloadHandle, CURLOPT_WRITEDATA, &Temp);
		curl_easy_setopt(CurlDownloadHandle, CURLOPT_HEADERDATA, &Temp);
		return curl_easy_perform(CurlDownloadHandle);
}

ResponseStruct* CurlDownloadNoFile(char *WithURL, char *DownloadPath) {
		DownloadStruct Temp = {
				.RemoteNameSize = 1,
				.RemoteFileName = malloc(sizeof(char)),
				.DownloadPath = DownloadPath
		};

		ResponseStruct *Response = malloc(sizeof(ResponseStruct));

		curl_easy_setopt(CurlDownloadHandle, CURLOPT_URL, WithURL);
		curl_easy_setopt(CurlDownloadHandle, CURLOPT_WRITEDATA, &Temp);
		curl_easy_setopt(CurlDownloadHandle, CURLOPT_HEADERDATA, &Temp);

		Response->Response = curl_easy_perform(CurlDownloadHandle);
		Response->FileStream = Temp.FileStream;
		Response->FileNameSize = Temp.RemoteNameSize == 1 ? strlen(Temp.RemoteFileName) : Temp.FileNameSize;

		Response->FileName = Temp.RemoteFileName;
		Response->FreeName = Temp.RemoteNameSize != 1;
		return Response;
}

static size_t WriteFileCallback(void *Contents, size_t Size, size_t nmemb, void *DownloadInfo) {
		DownloadStruct *Info = DownloadInfo;

		if (!Info->FileStream) {
				char TempBuffer[PATH_MAX];
				uint32_t DownloadPathLen = strlen(Info->DownloadPath);

				if (Info->RemoteNameSize == 1) {
						free(Info->RemoteFileName);

						Info->RemoteFileName = "temp";
						Info->RemoteNameSize = 0;
				}

				memcpy(TempBuffer, Info->DownloadPath, DownloadPathLen);
				memcpy(TempBuffer + DownloadPathLen, Info->RemoteFileName, strlen(Info->RemoteFileName) + 1);
				Info->FileStream = fopen(TempBuffer, "w");

				if (unlikely(!Info->FileStream))
						Error("[FATAL]: Unable to open %s for download.", ERR_STANDARD, TempBuffer);
		}

		return fwrite(Contents, Size, nmemb, Info->FileStream); /* Written bytes. */
}

size_t WriteDataCallbackSimple(void *Contents, size_t Size, size_t DataSize, void *UserPointer) {
		size_t RequiredSize = Size * DataSize;
		MemoryStruct *Memory = (MemoryStruct*)UserPointer;

		memcpy(&(Memory->Memory[Memory->Size]), Contents, RequiredSize);
		Memory->Size += RequiredSize;
		return RequiredSize;
}

static size_t WriteMemoryCallback(void *Contents, size_t Size, size_t DataSize, void *UserPointer) {
		size_t RequiredSize = Size * DataSize;
		MemoryStruct *Memory = (MemoryStruct*)UserPointer;

		char *Pointer = realloc(Memory->Memory, Memory->Size + RequiredSize + 1);

		if (unlikely(Pointer == NULL)) {
				printf("WirteMemoryCallback: not enough memory.\n");
				return 0;
		}

		Memory->Memory = Pointer;
		memcpy(&(Memory->Memory[Memory->Size]), Contents, RequiredSize);
		Memory->Size += RequiredSize;
		Memory->Memory[Memory->Size] = 0;

		return RequiredSize;
}
