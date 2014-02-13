// Copyright 2014, ABN Software Inc (http://www.abnsoftware.com).
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// 	You may obtain a copy of the License at
// 
// http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// 
// Warning: 
// 
// C++ is not a toy. Not suitable for children under three years old. 
//
// Extreme crash hazard, please keep your head clear.

#include "stdafx.h"
#include <stdlib.h>
#include <windows.h>
#include <queue>

#include "..\stringencoders\src\modp_numtoa.h"

class WriteBuffer {
public:

	WriteBuffer(size_t buffer_element_count, int max_float_digits) 
		: buffer_element_count_(buffer_element_count)
		, buffer_byte_size_(buffer_element_count * (max_float_digits + 5))
	{
		ptr_ = new char[buffer_byte_size_];
		useful_data_size_ = 0;
	}
	~WriteBuffer() 
	{
		delete[] ptr_;
	}

	char* ptr_;
	size_t useful_data_size_;

	const size_t buffer_element_count_;
	const size_t buffer_byte_size_;
};

CRITICAL_SECTION g_write_queue_cs;
HANDLE g_write_queue_has_more_data_event = NULL;
HANDLE g_write_queue_accepts_more_data_event = NULL;
const int kMaxQueue = 3;
std::queue<WriteBuffer*> g_write_queue;
bool g_done = false;

DWORD WINAPI WriteThreadProc(LPVOID lpParameter) 
{
	HANDLE hFile = HANDLE(lpParameter);
	do 
	{
		WriteBuffer* write_buffer = NULL;
		bool exit = false;
		EnterCriticalSection(&g_write_queue_cs);
		if (g_write_queue.size() > 0) 
		{
			write_buffer = g_write_queue.front();
			g_write_queue.pop();
		}
		else if (g_done)
		{
			exit = true;
		} 
		SetEvent(g_write_queue_accepts_more_data_event);
		LeaveCriticalSection(&g_write_queue_cs);

		if (write_buffer) 
		{
			DWORD bytes_written = 0;
			::WriteFile(hFile, write_buffer->ptr_, write_buffer->useful_data_size_, 
				&bytes_written, NULL);
			delete write_buffer;
		} 
		else if (exit) {
			break;
		} else {
			// Wait for new(s).
			WaitForSingleObject(g_write_queue_has_more_data_event, INFINITE);
		}

	} while (true);

	return 1;
}

int _tmain(int argc, _TCHAR* argv[])
{
	const size_t max_num = 10000000;
	const int buffer_element_count = 100000;
	const int max_float_digits = 8;
	
	// Init critical section;
	InitializeCriticalSection(&g_write_queue_cs);
	g_write_queue_has_more_data_event = CreateEvent(NULL, FALSE, FALSE, NULL);
	g_write_queue_accepts_more_data_event = CreateEvent(NULL, FALSE, TRUE, NULL);

	HANDLE hFile = ::CreateFile(L"output.txt", GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, NULL);
	if (hFile == INVALID_HANDLE_VALUE) 
	{
		printf("Oppps");
		exit(-1);
	}

	// Launch a writer thread.
	HANDLE hThread = CreateThread(NULL, 0, &WriteThreadProc, hFile,0, 0);

	// std::ofstream output();
	for (size_t i=0; i < max_num / buffer_element_count; ++i) 
	{
		// Prepare a block of numbers for writing.
		WriteBuffer* write_buffer = new WriteBuffer(buffer_element_count, max_float_digits);
		char* write_ptr = write_buffer->ptr_;
		for (int k = 0; k < buffer_element_count; ++k)
		{
			// Format each float to string and append to buffer.
			float random = float(rand()) / RAND_MAX;			
			write_ptr += modp_dtoa(random, write_ptr, max_float_digits);
			*(write_ptr++) = '\r';
			*(write_ptr++) = '\n';
		}
		// Compute how many bytes to write.
		write_buffer->useful_data_size_ = write_ptr - write_buffer->ptr_;

		// Enqueue for writing.
		while (write_buffer) {
			EnterCriticalSection(&g_write_queue_cs);
			if (g_write_queue.size() < kMaxQueue) 
			{
				// ops.
				g_write_queue.push(write_buffer);
				SetEvent(g_write_queue_has_more_data_event);
				write_buffer = NULL;
			}
			LeaveCriticalSection(&g_write_queue_cs);
			if (write_buffer) {
				// slow down writing, queue is full
				printf("S");
				WaitForSingleObject(g_write_queue_accepts_more_data_event, 200);
			}
		}
	}

	// Let the writing thread know we are done.
	g_done = true;
	SetEvent(g_write_queue_has_more_data_event);

	// Wait for writing thread to finish.
	WaitForSingleObject(hThread, INFINITE);

	::CloseHandle(hFile);
	return 0;
}

