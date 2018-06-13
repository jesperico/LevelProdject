#include "CircBuffer.h"


CircBuffer::CircBuffer(LPCWSTR buffName, const size_t & circBufferSize, const bool & isProducer, const size_t & chunkSize)
{
	myMutex = CreateMutex(nullptr, false, L"myUniqueName");

	WaitForSingleObject(myMutex, ms); // LOCK

	//CIRCBUFFER
	this->buffName = buffName;
	this->circBufferSize = circBufferSize;
	this->isProducer = isProducer;
	this->chunkSize = chunkSize;

	HANDLE handleMapFile;

	handleMapFile = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, circBufferSize, buffName);

	if(handleMapFile == NULL)
	{
		cout << "Failed to create handle" << endl;
		exit(0);
	}

	pData = (char*)MapViewOfFile(handleMapFile, FILE_MAP_ALL_ACCESS, 0, 0, circBufferSize);
	
	if (pData == NULL)
	{
		cout << "Failed to create Map view of file" << endl;
		CloseHandle(handleMapFile);
		exit(0);
	}


	//SHARED MEMORY

	HANDLE sharedHandle;

	sharedHandle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, (sizeof(size_t)*4), L"uniqueName");

	if (sharedHandle == NULL)
	{
		cout << "Shared Handle: Failed to create handle" << endl;
		exit(0);
	}

	pControl = (char*)MapViewOfFile(sharedHandle, FILE_MAP_ALL_ACCESS, 0, 0, (sizeof(size_t) * 3));
	
	if (pControl == NULL)
	{
		cout << "Shared Handle: Failed to create Map view of file" << endl;
		CloseHandle(sharedHandle);
		exit(0);
	}

    //initialize because this client was the first
    tail = (size_t*)pControl;
    head = tail + 1;
    clients = tail + 2;
    freespace = clients + 1;

	if(GetLastError() != ERROR_ALREADY_EXISTS)// Value to adress
	{
		*head = 0;
		*tail = 0;
		*clients = 1;
        *freespace = circBufferSize;
	}
	else if(!isProducer)
	{ 
		*clients = *clients + 1;
	}

	ReleaseMutex(myMutex); // UNLOCK
}

CircBuffer::~CircBuffer()
{
}

bool CircBuffer::push(const void * msg, size_t length)//write, producer
{
	size_t padding = (256 - (length + sizeof(Header)) % 256);
	size_t nextMsgSize = sizeof(Header) + length + padding;

	if (*head > *tail)
	{
		*freespace = (circBufferSize - *head) + *tail;
	}
	else if (*head < *tail)
	{
		*freespace = *tail - *head;
	}
	else
		*freespace = circBufferSize;
																																											
																																									
	if (nextMsgSize < *freespace)
	{
		Header header;
		header.msgLength = length;
		header.msgPadding = padding;
		header.readsLeft = *clients;

		if ((*head + nextMsgSize) < circBufferSize)
		{
			CopyMemory(pData + localHead, // Destination
				&header,			// Source
				sizeof(Header));	// Length

			localHead += sizeof(Header);

			CopyMemory(pData + localHead,			// Destination
				msg,							// Source
				sizeof(char) * header.msgLength);// Length

			localHead = (localHead + header.msgLength + header.msgPadding) % circBufferSize;
			*head = localHead;

			return true;
		}
		else if (*freespace - (circBufferSize - *head) > nextMsgSize || *head == *tail)
		{
			header.msgPadding += sizeof(Header);

				CopyMemory(pData + localHead, // Destination
					&header,			// Source
					sizeof(Header));	// Length

					localHead = 0;

				CopyMemory(pData + localHead,			// Destination
					msg,							// Source
					sizeof(char) * header.msgLength);// Length

				localHead = (localHead + header.msgLength + header.msgPadding) % circBufferSize;
				*head = localHead;

			return true;
		}
		return false;
	}
	else
	{
		return false;
	}
}

bool CircBuffer::pop(char* msg)//read, consumer
{
	if (localTail != *head)
	{
        WaitForSingleObject(myMutex, ms); // LOCK

		Header header;

		memcpy(&header,			            	// Destination
			(char*)pData + localTail,			// Source
			sizeof(Header));	              // Length
		
		if (header.readsLeft != 0)
		{
			header.readsLeft--;

            memcpy((char*)pData + localTail,
                &header,			
                sizeof(Header));
		}

        size_t nextTailPos = localTail + (header.msgLength + header.msgPadding) + sizeof(Header);

		if (nextTailPos >= circBufferSize)
        {
            localTail = 0;
        }
		else
			localTail += sizeof(Header);

		memcpy(msg,									// Destination	
			(char*)pData + localTail,				// Source						
			sizeof(char) * header.msgLength);		// Length
		
		msg[header.msgLength] = 0;
        																																			
		localTail = (localTail + header.msgLength + header.msgPadding) % circBufferSize;
																																					
		if (header.readsLeft == 0)
		{
			*tail = localTail;
		}
        ReleaseMutex(myMutex); // UNLOCK
		return true;
	}
	else
	{
		return false;
	}
}

