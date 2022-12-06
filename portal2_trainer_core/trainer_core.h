#pragma once

#include <Windows.h>
#include <TlHelp32.h>

namespace trainer_core {
	PROCESSENTRY32 prepare_empty_process_entry();

	MODULEENTRY32 prepare_empty_module_entry();

	DWORD get_process_id_by_name(PCWSTR name);

	LPVOID get_module_offset_by_name(PCWSTR module_name, HANDLE process_handle);
}
