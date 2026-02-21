#include <Shared.h>

#include <stdio.h>
#include <stdarg.h>

#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <unistd.h>
#include <fcntl.h>

extern char **environ;

#define min(a,b) ( ((a) < (b)) ? (a) : (b))

int32_t ExecProgram(char *Program, uint8_t Silent, uint8_t Disown, ...) {
		char *CurrentArgument;
		char **Packed = malloc(sizeof(char*) + 5);

		if (unlikely(!Packed))
				Error("[ERROR]: Unable to allocate Packed during ExecProgram call.", ERR_MEMORY);

		pid_t NewPID;

		va_list Arguments;
		va_start(Arguments, Disown);

		int WaitStatus;

		uint8_t PackedSize = 5, ElementIndex = 1;

		/* First argument must be the program whatever. */
		Packed[0] = Program;

		do {
				CurrentArgument = va_arg(Arguments, char*);

				if (CurrentArgument != NULL) {
						if (CurrentArgument[0] == '\0')
								continue;
				}

				if (ElementIndex == PackedSize) {
						PackedSize += 2;
						char **NewPointer = realloc(Packed, sizeof(char*) * PackedSize);

						if (unlikely(!NewPointer))
								Error("[ERROR]: Unable to realloc Packed during ExecProgram call.", ERR_MEMORY);

						Packed = NewPointer;
				}

				Packed[ElementIndex] = CurrentArgument;
				ElementIndex += 1;
		} while (CurrentArgument != NULL);

		if ((NewPID = fork()) == 0) {
				if (Silent) {
						int FileDescriptor = open("/dev/null", O_WRONLY);

						dup2(FileDescriptor, 1); /* Make stdout a copy of /dev/null */
						dup2(FileDescriptor, 2); /* Make stderr a copy of /dev/null */

						close(FileDescriptor);
				}

				if (unlikely(execve(Program, Packed, environ) == -1))
						exit(EXIT_FAILURE);
		}

		if (!Disown) {
				waitpid(NewPID, &WaitStatus, 0);

				if (WIFEXITED(WaitStatus) != 1 || WEXITSTATUS(WaitStatus) != 0) {
						Error("[ERROR]: Something went wrong while running %s.", ERR_STANDARD | ERR_NOEXIT, Program);
						return -1;
				}
		}

		va_end(Arguments);

		free(Packed);
		return WEXITSTATUS(WaitStatus);
}

EnvInfoStruct *FetchEnvInfo(char *StudioVersion) {
		FILE *WineVersionFile = NULL;

		const char *WineCommand = " --version";
		char *WineExec = PwootieReadEntry("wine_binary", 0);
		uint8_t FreeWineExec = 1;

		if (unlikely(!WineExec)) {
				WineExec = "wine";
				FreeWineExec = 0;
		}

		uint32_t WineExecLen = strlen(WineExec);
		uint32_t WineCommandLen = strlen(WineCommand);

		char *CommandBuffer = malloc((WineExecLen + WineCommandLen + 1) * sizeof(char));

		if (unlikely(!CommandBuffer))
				Error("[FATAL]: Unable to allocate CommandBuffer during FetchEnvInfo.", ERR_MEMORY);

		memcpy(CommandBuffer, WineExec, WineExecLen);
		memcpy(CommandBuffer + WineExecLen, WineCommand, WineCommandLen + 1);

		EnvInfoStruct *EnvInfo = malloc(sizeof(EnvInfoStruct));
		struct utsname KernelInfo;

		if (unlikely(!EnvInfo))
				Error("[FATAL]: Unable to allocate EnvInfo during FetchEnvInfo.", ERR_MEMORY);

		if (unlikely(uname(&KernelInfo) != 0)) {
				Error("[ERROR]: uname call failed while fetching environment information!", ERR_STANDARD | ERR_NOEXIT);
				goto error;
		}

		uint32_t MachineLen = strlen(KernelInfo.machine);
		uint32_t ReleaseLen = strlen(KernelInfo.release);

		/* 64 is the maximum buffer size. */
		MachineLen = min(MachineLen, 64);
		ReleaseLen = min(ReleaseLen, 64);

		memcpy(EnvInfo->MachineType, KernelInfo.machine, MachineLen);
		memcpy(EnvInfo->KernelRelease, KernelInfo.release, ReleaseLen);
		EnvInfo->PwootieVersion = PWOOTIE_VERSION;
		EnvInfo->RobloxVersion = StudioVersion;
		EnvInfo->SessionType = getenv("XDG_SESSION_TYPE");
		EnvInfo->DesktopEnvironment = getenv("XDG_CURRENT_DESKTOP");

		EnvInfo->MachineType[MachineLen] = '\0';
		EnvInfo->KernelRelease[ReleaseLen] = '\0';

		EnvInfo->SessionType = EnvInfo->SessionType != NULL ? EnvInfo->SessionType : "NULL";
		EnvInfo->DesktopEnvironment = EnvInfo->DesktopEnvironment != NULL ? EnvInfo->DesktopEnvironment : "NULL";

		WineVersionFile = popen(CommandBuffer, "r");

		if (unlikely(!WineVersionFile)) {
				Error("[ERROR]: Failed to popen %s.", ERR_STANDARD | ERR_NOEXIT, CommandBuffer);
				goto error;
		}

		size_t Length = fread(EnvInfo->WineVersion, sizeof(char), 64, WineVersionFile);

		if (likely(Length > 0)) {
				EnvInfo->WineVersion[Length - 1] = '\0';
		} else {
				Error("[ERROR]: Failed to fread WineVersion file.", ERR_STANDARD | ERR_NOEXIT);
				goto error;
		}

		char *GraphicsMode = GetStudioSetting("GraphicsMode");

		switch (GraphicsMode[0]) {
				case '1':
						EnvInfo->Renderer = "Automatic";
						break;
				case '2':
						EnvInfo->Renderer = "Direct3D11";
						break;
				case '4':
						EnvInfo->Renderer = "OpenGL";
						break;
				case '6':
						EnvInfo->Renderer = "Vulkan";
						break;
				default:
						EnvInfo->Renderer = "Unknown";
						break;
		}

		pclose(WineVersionFile);
		free(CommandBuffer);

		if (likely(FreeWineExec))
				free(WineExec);

		return EnvInfo;
error:
		if (FreeWineExec)
				free(WineExec);

		if (WineVersionFile)
				pclose(WineVersionFile);

		free(CommandBuffer);
		free(EnvInfo);

		return NULL;
}
