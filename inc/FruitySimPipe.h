#pragma once

#define PIPE_BUFFER_SIZE 1024

class FruitySimPipe
{
private:


public:
	FruitySimPipe();
	static void ConnectPipe();
	static void WriteToPipe(const char * message);
	static void WriteToPipeF(const char * message, ...);
	static char* ReadFromPipe();
	static void ClosePipe();
	~FruitySimPipe();
};

