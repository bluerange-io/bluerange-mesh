#include "FruitySimPipe.h"
#include <Terminal.h>

#include <windows.h>
#include <conio.h>

HANDLE hPipe = INVALID_HANDLE_VALUE;
bool pipeConnected = false;

FruitySimPipe::FruitySimPipe()
{

}

void FruitySimPipe::ConnectPipe()
{
	printf("Connecting Pipe...\n");
	//Create the pipe
	if (hPipe == INVALID_HANDLE_VALUE) {
		hPipe = CreateNamedPipe(TEXT("\\\\.\\pipe\\FruitySimPipe"),
			PIPE_ACCESS_DUPLEX | PIPE_TYPE_BYTE | PIPE_READMODE_BYTE,   // FILE_FLAG_FIRST_PIPE_INSTANCE is not needed but forces CreateNamedPipe(..) to fail if the pipe already exists...
			PIPE_WAIT,
			1,
			PIPE_BUFFER_SIZE * 16,
			PIPE_BUFFER_SIZE * 16,
			NMPWAIT_USE_DEFAULT_WAIT,
			NULL);
	}

	// wait for someone to connect to the pipe
	bool result = ConnectNamedPipe(hPipe, NULL);

	pipeConnected = true;
}

//This will write a message to the pipe
void FruitySimPipe::WriteToPipe(const char* message)
{
	if (!pipeConnected) return;
	//printf("Writing: %s\n", message);
	DWORD bytesWritten;
	boolean success = false;

	WriteFile(hPipe,
			message,
			strlen(message),
			&bytesWritten,
			NULL);

}

//This will write a message to the pipe
void FruitySimPipe::WriteToPipeF(const char* message, ...)
{
	if (!pipeConnected) return;

	char buffer[PIPE_BUFFER_SIZE];
	va_list aptr;
	va_start(aptr, message);
	vsnprintf(buffer, PIPE_BUFFER_SIZE, message, aptr);
	va_end(aptr);

	WriteToPipe(buffer);
}

//This will read all remaining characters on the pipe
char readBuffer[PIPE_BUFFER_SIZE];
char* FruitySimPipe::ReadFromPipe()
{
	if (!pipeConnected) return NULL;

	//printf("Reading from pipe\n");
	int bufferLength = PIPE_BUFFER_SIZE;
	DWORD readBytes;

	 ReadFile(hPipe, readBuffer, bufferLength - 1, &readBytes, NULL);

	/* add terminating zero */
	 readBuffer[readBytes] = '\0';

	/* do something with data in buffer */
	//printf("Read: %s", buffer);

	return readBuffer;
}

void FruitySimPipe::ClosePipe() {
	if (!pipeConnected) return;

	//printf("disconnecting pipe");
	while (DisconnectNamedPipe(hPipe) != 0) {
		printf("disconnecting pipe");
	}
	

	//CloseHandle(hPipe);

}


FruitySimPipe::~FruitySimPipe()
{
}