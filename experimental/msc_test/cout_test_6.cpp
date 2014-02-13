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
		ptr_ = new char[buffer_byte_size_];
		memset(ptr_, 0, buffer_byte_size_);
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
INT64 g_total_bytes_written = 0;
__int64 g_begin_ticks = 0;
__int64 g_end_ticks = 0;

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
			g_total_bytes_written += write_buffer->useful_data_size_;
		}
		else if (g_done)
		{
			exit = true;
		} 
		SetEvent(g_write_queue_accepts_more_data_event);
		LeaveCriticalSection(&g_write_queue_cs);

		if (write_buffer) 
		{
			/* DWORD bytes_written = 0;
			::WriteFile(hFile, write_buffer->ptr_, write_buffer->useful_data_size_, 
				&bytes_written, NULL);*/
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
	const size_t max_num = 1000000000;
	const int buffer_element_count = 10000;
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

	typedef char* char_ptr;

	char_ptr* x = new char_ptr[1001];
	for (int i = 0; i<1001; ++i)
	{
		x[i] = new char[8];
		sprintf_s(x[i],8,"%03d", i);
	}
	x[1000] = "999";

	// Launch a writer thread.
	HANDLE hThread = CreateThread(NULL, 0, &WriteThreadProc, hFile,0, 0);

	sfmt_t sfmt;
	sfmt_init_gen_rand(&sfmt, 1234);
	uint32_t* randoms = (uint32_t*) _aligned_malloc(sizeof(uint32_t)*buffer_element_count, 32);
	short* dga = (short*) _aligned_malloc(sizeof(short)*(buffer_element_count + 32)*2, 32);

	g_begin_ticks = GetTickCount64();

	// std::ofstream output();
	int finish = max_num / buffer_element_count;
	for (size_t i=0; i < finish; ++i) 
	{
		// Prepare a block of numbers for writing.
		WriteBuffer* write_buffer = new WriteBuffer(buffer_element_count, max_float_digits);
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
			float f1 = randoms[k] / 4294967.296f;
			short d1 = short(f1);
			float f2 = (f1 - d1)*1000;
			short d2 = short(f2 + 0.5);

			dga[2*k] = d1;
			dga[2*k+1] = d2;
		}

		// convert to string form.
		for (int k = 0; k < 2*buffer_element_count; k+=2)
		{
			*(write_ptr) = '0';
			*(write_ptr + 1) = '.';
			//printf("%s\n", write_buffer->ptr_);
			*((int*)(write_ptr + 2)) = *((int*)(x[dga[k]]));
			//printf("%s\n", write_buffer->ptr_);
			*((int*)(write_ptr + 5)) = *((int*)(x[dga[k + 1]]));
			*(write_ptr + 8) = '\r';
			*(write_ptr + 9) = '\n';
			write_ptr+=10;
			//printf("%s\n", write_buffer->ptr_);
		}


			// printf("%f => %d, %d => 0.%s%s\n", randoms[k]/4294967296.0f, d1, d2, x[d1], x[d2]);

			//digits

			// write_ptr += modp_dtoa(random, write_ptr, max_float_digits);
/*			*(write_ptr++) = '1';
			*(write_ptr++) = '1';
			*(write_ptr++) = '1';
			*(write_ptr++) = '1';
			*(write_ptr++) = '1';
			*(write_ptr++) = '1';
			*(write_ptr++) = '1';
			*(write_ptr++) = '1';
			*(write_ptr++) = '1';
*/			//*(write_ptr++) = '\r';
			//*(write_ptr++) = '\n';
		//}
		// Compute how many bytes to write.
		write_buffer->useful_data_size_ =  write_ptr - write_buffer->ptr_;

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

	g_end_ticks = GetTickCount64();

	// Let the writing thread know we are done.
	EnterCriticalSection(&g_write_queue_cs);
	g_done = true;
	LeaveCriticalSection(&g_write_queue_cs);
	SetEvent(g_write_queue_has_more_data_event);
	// Wait for writing thread to finish.
	WaitForSingleObject(hThread, INFINITE);

	::CloseHandle(hFile);

	_aligned_free(randoms);

	__int64 delta = g_end_ticks - g_begin_ticks;

	printf("Speed %f Mb per sec\n", (g_total_bytes_written * 1000.0) / (1024.0 * 1024 * delta));

	char c;
	scanf("%c", &c);
	return 0;
}

