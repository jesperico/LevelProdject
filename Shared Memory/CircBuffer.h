#pragma once
#include <windows.h>
#include <conio.h>
#include <tchar.h>
#include <vector>
#include <string>
#include <iostream>

using namespace std;

class CircBuffer
{
private:
	LPCWSTR buffName;
	size_t circBufferSize;
	bool isProducer;
	size_t chunkSize;
	char* pData;
	char* pControl;

	//Local pointers
	size_t localTail = 0;
	size_t localHead = 0;

	//Shared Memory
	size_t *head;
	size_t *tail;
	size_t *clients;
    size_t *freespace;

	//Mutex
	DWORD ms = INFINITE;//ms delay
	HANDLE myMutex;

	struct Header //8 byte size
	{
		size_t msgID;
		size_t msgLength;
		size_t msgPadding;
		size_t readsLeft;
	};

public:
	CircBuffer(
		LPCWSTR buffName,          
		const size_t& buffSize,    
		const bool& isProducer,    
		const size_t& chunkSize);	

	~CircBuffer();

	bool push(const void* msg, size_t length);

	bool pop(char* msg);
};

