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
#include <iostream>
#include <fstream>
#include <windows.h>

int _tmain(int argc, _TCHAR* argv[])
{
	size_t max_num = 100000000;
	const int max_char_per_float = 10;
	const size_t buffer_element_count = 100000;
	size_t buffer_byte_size = buffer_element_count * (max_char_per_float + 5);
	char* write_buffer = new char[buffer_byte_size];
	HANDLE hFile = ::CreateFile(L"output.txt", GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, NULL);
	// std::ofstream output();
	for (size_t i=0; i < max_num / buffer_element_count; ++i) 
	{
		int buffer_position = 0;
		for (int k = 0; k < buffer_element_count; ++k)
		{
			char number_buffer[32];
			int pos = 0;
			int dec;
			int sign;
			float random = float(rand()) / RAND_MAX;
			_ecvt_s(number_buffer, random, max_char_per_float, &dec, &sign);
			if (sign)
			{
				write_buffer[buffer_position++] = '-';
			}
			while(pos<dec) 
			{ 
				write_buffer[buffer_position++] = number_buffer[pos++]; 
			}
			write_buffer[buffer_position++] = '.';
			while (number_buffer[pos])
			{
				write_buffer[buffer_position++] = number_buffer[pos++];
			}
			write_buffer[buffer_position++] = '\r';
			write_buffer[buffer_position++] = '\n';
		}
		DWORD bytes_written = 0;
		::WriteFile(hFile, write_buffer, buffer_position, &bytes_written, NULL);
	}

	::CloseHandle(hFile);
	return 0;
}

