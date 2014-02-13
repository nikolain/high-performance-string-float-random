// Copyright 2014, ABN Software Inc (http://www.abnsoftware.com).
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
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

#define HAVE_SSE2
#include "..\stringencoders\src\modp_numtoa.h"
#include "..\SFMT-src-1.4.1\SFMT.h"
#include "..\SFMT-src-1.4.1\SFMT.c"

class WriteBuffer {
public:

	WriteBuffer(size_t buffer_element_count, int max_float_digits) 
		: buffer_element_count_(buffer_element_count)
		, buffer_byte_size_(buffer_element_count * (max_float_digits + 5))
	{
		ptr_ = (char*)_aligned_malloc(buffer_byte_size_, 64);
		// memset(ptr_, 0, buffer_byte_size_);
		useful_data_size_ = 0;
	}
	~WriteBuffer() 
	{
		_aligned_free(ptr_);
	}

	char* ptr_;
	size_t useful_data_size_;

	const size_t buffer_element_count_;
	const size_t buffer_byte_size_;
};

CRITICAL_SECTION g_write_queue_cs;
HANDLE g_write_queue_has_more_data_event = NULL;
HANDLE g_write_queue_accepts_more_data_event = NULL;
const int kMaxQueue = 5;
std::queue<WriteBuffer*> g_write_queue;
bool g_done = false;
INT64 g_total_bytes_written = 0;
__int64 g_begin_ticks = 0;
__int64 g_end_ticks = 0;

DWORD WINAPI WriteThreadProc(LPVOID lpParameter) 
{
	HANDLE hFile = HANDLE(lpParameter);
	do 
	{
		bool exit = false;
		EnterCriticalSection(&g_write_queue_cs);
		if (g_write_queue.size() > 0) 
		{
			while (!g_write_queue.empty())
			{
				WriteBuffer* write_buffer = NULL;
				write_buffer = g_write_queue.front();
				g_total_bytes_written += write_buffer->useful_data_size_;
				delete write_buffer;
				g_write_queue.pop();
			}
		}
		else if (g_done)
		{
			exit = true;
		} 
		SetEvent(g_write_queue_accepts_more_data_event);
		LeaveCriticalSection(&g_write_queue_cs);

			/* DWORD bytes_written = 0;
			::WriteFile(hFile, write_buffer->ptr_, write_buffer->useful_data_size_, 
				&bytes_written, NULL);*/
		if (exit) {
			break;
		} else {
			// Wait for new(s).
			WaitForSingleObject(g_write_queue_has_more_data_event, INFINITE);
		}

	} while (true);

	return 1;
}

// Number to string lookup.
typedef char* char_ptr;
char_ptr* g_x = new char_ptr[10001];

DWORD WINAPI RandomThreadProc(LPVOID lpParameter) 
{
	const int buffer_element_count = 30000;
	const int max_float_digits = 8;

	sfmt_t sfmt;
	sfmt_init_gen_rand(&sfmt, GetCurrentThreadId());
	uint32_t* randoms = (uint32_t*) _aligned_malloc(sizeof(uint32_t)*buffer_element_count, 64);
	short* dga = (short*) _aligned_malloc(sizeof(short)*(buffer_element_count + 32)*2, 64);

	bool done = false;

	WriteBuffer* write_buffer = new WriteBuffer(buffer_element_count, max_float_digits);
	// std::ofstream output();
	while (!done) 
	{
		// Prepare a block of numbers for writing.
		char* write_ptr = write_buffer->ptr_;

		sfmt_fill_array32(&sfmt, randoms, buffer_element_count);

		// Compute digits
		for (int k = 0; k < buffer_element_count; ++k)
		{
			// Format each float to string and append to buffer.
			// float random = float(rand()) / RAND_MAX;
			// float random = float(randoms[k]) / 4294967296.0f;
			// int fractional_part = random *  1000000000;
			// this turns the fractional part of the float back into usable integer.
			double f1 = double(randoms[k]) / 429496.7296;
			short d1 = short(f1);
			double f2 = (f1 - d1)*10000.0;
			short d2 = short(f2 + 0.5);

			dga[2*k] = d1;
			dga[2*k+1] = d2;
		}

		*(write_ptr++) = '0';
		*(write_ptr++) = '.';
		// convert to string form.
		for (int k = 0; k < 2*buffer_element_count; k+=2)
		{
			*((int*)(write_ptr)) = *((int*)(g_x[dga[k]]));
			*((int*)(write_ptr + 4)) = *((int*)(g_x[dga[k + 1]]));
			// printf("%.8lf => %s\n", double(randoms[k/2]) / 4294967296.0, write_ptr-2);
			*((int*)(write_ptr + 8)) = '\r\n0.';
			write_ptr+=12;
		}
		// get rid of last 0.
		write_ptr-=2;
		// Compute how many bytes to write.
		write_buffer->useful_data_size_ =  write_ptr - write_buffer->ptr_;

		// Enqueue for writing.
		// while (write_buffer) {
			EnterCriticalSection(&g_write_queue_cs);
			/*if (g_write_queue.size() < kMaxQueue) 
			{
				g_write_queue.push(write_buffer);
				SetEvent(g_write_queue_has_more_data_event);
				write_buffer = NULL;
			} else 
			{
				// ops.
		    }*/
			g_total_bytes_written += write_buffer->useful_data_size_;
			done = g_done;
			LeaveCriticalSection(&g_write_queue_cs);
			/* if (write_buffer) {
				// slow down writing, queue is full
				// printf("S");
				WaitForSingleObject(g_write_queue_accepts_more_data_event, 10);
			}*/
		// }
	}

	_aligned_free(randoms);
	_aligned_free(dga);

	return 0;
}


int _tmain(int argc, _TCHAR* argv[])
{	
	// Init critical section;
	InitializeCriticalSection(&g_write_queue_cs);
	g_write_queue_has_more_data_event = CreateEvent(NULL, FALSE, FALSE, NULL);
	g_write_queue_accepts_more_data_event = CreateEvent(NULL, FALSE, TRUE, NULL);

	for (int i = 0; i<10000; ++i)
	{
		g_x[i] = new char[5];
		sprintf_s(g_x[i],8,"%04d", i);
	}
	g_x[10000] = "9999";

	HANDLE hFile = ::CreateFile(L"output.txt", GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, NULL);
	if (hFile == INVALID_HANDLE_VALUE) 
	{
		printf("Oppps");
		exit(-1);
	}

	// Launch a writer thread.
	HANDLE hWriteThread = CreateThread(NULL, 0, &WriteThreadProc, hFile,0, 0);

	HANDLE hRandomThread1 = CreateThread(NULL, 0, &RandomThreadProc, 0, 0, 0);
	HANDLE hRandomThread2 = CreateThread(NULL, 0, &RandomThreadProc, 0, 0, 0);
	HANDLE hRandomThread3 = CreateThread(NULL, 0, &RandomThreadProc, 0, 0, 0);
	HANDLE hRandomThread4 = CreateThread(NULL, 0, &RandomThreadProc, 0, 0, 0);
	//HANDLE hRandomThread3 = CreateThread(NULL, 0, &RandomThreadProc, 0, 0, 0);
	//HANDLE hRandomThread4 = CreateThread(NULL, 0, &RandomThreadProc, 0, 0, 0);

	g_begin_ticks = GetTickCount64();

	::Sleep(20000);

	// Let the writing thread know we are done.
	EnterCriticalSection(&g_write_queue_cs);
	g_done = true;
	LeaveCriticalSection(&g_write_queue_cs);
	SetEvent(g_write_queue_has_more_data_event);
	// Wait for writing thread to finish.
	WaitForSingleObject(hRandomThread1, INFINITE);
	WaitForSingleObject(hRandomThread2, INFINITE);
    WaitForSingleObject(hRandomThread3, INFINITE);
	WaitForSingleObject(hRandomThread4, INFINITE);
	WaitForSingleObject(hWriteThread, INFINITE);

	g_end_ticks = GetTickCount64();

	::CloseHandle(hFile);

	__int64 delta = g_end_ticks - g_begin_ticks;

	printf("Speed %f Mb per sec\n", (g_total_bytes_written * 1000.0) / (1024.0 * 1024 * delta));

	char c;
	scanf("%c", &c);
	return 0;
}

