#include "trainer_core.h"

namespace trainer_core {
	PROCESSENTRY32 prepare_empty_process_entry() {
		PROCESSENTRY32 ret;
		memset(&ret, 0, sizeof(ret));
		ret.dwSize = sizeof(ret);
		return ret;
	}

	MODULEENTRY32 prepare_empty_module_entry() {
		MODULEENTRY32 ret;
		memset(&ret, 0, sizeof(ret));
		ret.dwSize = sizeof(ret);
		return ret;
	}

	/**
		Returns 0 if fails.
	*/
	DWORD get_process_id_by_name(PCWSTR name)
	{
		PROCESSENTRY32 process_entry = prepare_empty_process_entry();
		HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (!snapshot) {
			return 0;
		}

		DWORD ret = 0;
		BOOL bypass_first = FALSE;

		if (Process32First(snapshot, &process_entry)) {
			while (Process32Next(snapshot, &process_entry)) {
				if (!wcscmp(process_entry.szExeFile, name)) {
					if (bypass_first) {
						bypass_first = false;
						continue;
					}
					ret = process_entry.th32ProcessID;
					break;
				}
			}
		}
		CloseHandle(snapshot);
		return ret;
	}

	/**
		Returns NULL if fails.
	*/
	LPVOID get_module_offset_by_name(PCWSTR module_name, HANDLE process_handle)
	{
		MODULEENTRY32 module_entry = prepare_empty_module_entry();
		DWORD process_id = GetProcessId(process_handle);
		HANDLE snapshot =
			CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, process_id);
		if (!snapshot) {
			return NULL;
		}

		LPVOID ret = NULL;

		if (Module32First(snapshot, &module_entry)) {
			while (Module32Next(snapshot, &module_entry)) {
				if (!wcscmp(module_entry.szModule, module_name)) {
					ret = static_cast<LPVOID>(module_entry.modBaseAddr);
					break;
				}
			}
		}

		CloseHandle(snapshot);
		return ret;
	}
}
