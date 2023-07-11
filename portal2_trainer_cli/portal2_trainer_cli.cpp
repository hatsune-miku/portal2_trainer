/*
* portal2_trainer_cli.cpp
* 
* Author: EGGTARTc
* E-mail: l2a1knla#gmail.com
*/

#ifndef WIN32
#error This program must be compiled in Windows x86 in release mode.
#endif

#ifdef _DEBUG || _WIN64
#error This program must not compile in debug mode or x64! It uses complex memory manipulation and requires the x86 release build.
#endif

#include <iostream>
#include <cstring>
#include <string>
#include <format>

#include <trainer_core.h>

// ==============================================

const static wchar_t* GLOBAL_MUTEX_NAME           = L"MUTEX_PORTAL2_TRAINER_EGGTARTC";
const static wchar_t* GLOBAL_TARGET_PROCESS       = L"portal2.exe";
const static char*    GLOBAL_TARGET_PROCESS_ANSI  = "portal2.exe";

typedef struct {
    LPVOID client_base;
    LPVOID server_base;
    LPVOID engine_base;
} portal2_dll_base;

typedef struct {
    DWORD engine_base;
    DWORD remote_addr;
    char* string_buffer;
    int string_length;
} ConsoleCommand;


// ==============================================

static bool global_interrupted;
static HANDLE global_mutex;

// ==============================================

#define log_error(s) log(31, s)
#define log_info(s) log(37, s)
#define log_info_alter(s) log(32, s)
#define log_debug(s) log(36, s)

void log(int color, const std::string&& s) {
	std::cerr << "\033[" << color << "m * " << s << "\033[0m\n";
}

BOOLEAN __stdcall on_control_c_clicked(_In_ DWORD control_type)
{
	if (control_type == CTRL_C_EVENT || control_type == CTRL_CLOSE_EVENT) {
		global_interrupted = true;
		log_debug("Interrupted");
		return TRUE;
	}
	return FALSE;
}

void finish()
{
	log_debug("Exiting...");
	system("pause");

	if (global_mutex != NULL) {
		CloseHandle(global_mutex);
	}

	exit(0);
}

/*
* This function is to be injected in the game process.
* We must not do any stack/heap access.
*/
DWORD __stdcall remote_payload(LPVOID lparam) {
    ConsoleCommand* command = (ConsoleCommand*)lparam;

    char* string_buffer = command->string_buffer;
    int string_length = command->string_length;

    // char __thiscall sub_5FB5A380(_DWORD * this, void* a2, int a3)
    // x64 __fastcall: ecx, rdx, r8, r9
    // `a2` the string buffer.
    // `a3` the string length.
    DWORD remote_console_addr = command->remote_addr;

    // DWORD eax_value = command->engine_base + 0x68EEC0;
    // DWORD edx_value = command->engine_base + 0x67666C;
    DWORD ecx_value = command->engine_base + 0x674638;

    _asm {
        pushad;
        pushfd;

        // mov eax, [eax_value]; // engine.dll+68EEC0, Computer configurations.
        // mov edx, [edx_value]; // engine.dll+67666C, We do not know what it does. It just adds up along with time.

        mov ecx, ecx_value;  // engine.dll+674638, Command history that seperated by '\0'.
                             // ecx must be set as the call convension is `thiscall`.
                             // ecx represents the `this` pointer (first parameter).

        push string_length;
        push string_buffer;
        call remote_console_addr;

        popfd;
        popad;
    }

    return 0;
}

/*
* Program initialization.
*/
void init()
{
    log_info("PORTAL 2 TRAINER BY EGGTARTc");
    log_info("============================");

	// Init global variables.
	global_interrupted = false;

	// Create mutex.
	global_mutex = CreateMutex(NULL, TRUE, GLOBAL_MUTEX_NAME);
	if (global_mutex == NULL || GetLastError() == ERROR_ALREADY_EXISTS) {
		log_error("Another instance is already running!");
		finish();
		return;
	}

    // Handle Ctrl+C.
	SetConsoleCtrlHandler((PHANDLER_ROUTINE)on_control_c_clicked, TRUE);
}

/*
* Keep waiting for target process.
* Returns process handle.
* NULL if OpenProcess failed.
*/
HANDLE wait_for_process()
{
	DWORD process_id = 0;

	while (process_id == 0) {
		Sleep(500);
        log_info(std::format("Waiting for {}", GLOBAL_TARGET_PROCESS_ANSI));
		process_id = trainer_core::get_process_id_by_name(GLOBAL_TARGET_PROCESS);
	}

	log_info("Opening process...");

    // 真男人直接魔法数字，梦回易语言写挂（
	return OpenProcess(2035711, FALSE, process_id);
}

void set_sv_cheats(HANDLE process_handle, portal2_dll_base* bases, int value) {
	WriteProcessMemory(
		process_handle,
		static_cast<char*>(bases->engine_base) + 0x66C778, 
		&value, sizeof(value), NULL
	);
}

void set_sv_portal_placement_never_fail(HANDLE process_handle, portal2_dll_base* bases, int value) {
	WriteProcessMemory(
		process_handle,
		static_cast<char*>(bases->client_base) + 0xA10D50,
		&value, sizeof(value), NULL
	);
}

void set_sv_allow_mobile_portals(HANDLE process_handle, portal2_dll_base* bases, int value) {
	WriteProcessMemory(
		process_handle,
		static_cast<char*>(bases->client_base) + 0xA0D660,
		&value, sizeof(value), NULL
	);
}

void run_developer_console_command(HANDLE process_handle, portal2_dll_base* bases, const char* command) {

    #define FAIL(s) log_error(s);finish();return

    // Inject string.
    int string_size = strlen(command) + 1;
    LPVOID remote_string_addr = VirtualAllocEx(process_handle, NULL, string_size, MEM_COMMIT, PAGE_READWRITE);
    if (remote_string_addr == NULL) {
        FAIL("Error on injecting command into target (VirtualAllocEx).");
    }

    if (!WriteProcessMemory(process_handle, remote_string_addr, command, string_size, NULL)) {
        FAIL("Error on injecting command into target (WriteProcessMemory).");
    }

    // Inject struct.
    ConsoleCommand param = {
        .engine_base = reinterpret_cast<DWORD>(bases->engine_base),
        .remote_addr = reinterpret_cast<DWORD>(bases->engine_base) + 0x28A2C0,
        .string_buffer = reinterpret_cast<char*>(remote_string_addr),
        .string_length = string_size
    };

    LPVOID remote_command_addr = VirtualAllocEx(process_handle, NULL, sizeof(ConsoleCommand), MEM_COMMIT, PAGE_READWRITE);
    if (remote_command_addr == NULL) {
        FAIL("Error on injecting command structure into target (VirtualAllocEx).");
    }

    if (!WriteProcessMemory(process_handle, remote_command_addr, &param, sizeof(ConsoleCommand), NULL)) {
        FAIL("Error on injecting command structure into target (WriteProcessMemory).");
    }

    // Inject function.
    size_t function_size = 0x400;
    LPVOID remote_function_addr = VirtualAllocEx(process_handle, NULL, function_size, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (remote_function_addr == NULL) {
        FAIL("Error on allocating execute & readwrite memory (VirtualAllocEx).");
    }

    if (!WriteProcessMemory(process_handle, remote_function_addr, remote_payload, function_size, NULL)) {
        FAIL("Error on injecting payload into target (WriteProcessMemory).");
    }

    // Launch the payload.
    HANDLE thread_handle = CreateRemoteThread(
        process_handle,
        NULL,
        0,
        (LPTHREAD_START_ROUTINE)remote_function_addr,
        remote_command_addr,
        0,
        NULL
    );

    if (thread_handle == NULL) {
        FAIL("Error on launching payload.");
    }
    WaitForSingleObject(thread_handle, INFINITE);

    VirtualFreeEx(process_handle, remote_function_addr, function_size, MEM_DECOMMIT);
    VirtualFreeEx(process_handle, remote_command_addr, sizeof(ConsoleCommand), MEM_DECOMMIT);
    VirtualFreeEx(process_handle, remote_string_addr, string_size, MEM_DECOMMIT);
}

void main_loop(HANDLE process_handle, portal2_dll_base* bases)
{
    while (!global_interrupted) {
		Sleep(500);
	}
}

int main(int argc, const char** argv) {
	init();

	HANDLE process_handle = wait_for_process();
	if (process_handle == NULL) {
		log_error("Target process found, but unable to open.");
		log_error("Please run this program as administrator.");
		finish();
		return 0;
	}

	log_info("Finding submodules...");
	portal2_dll_base bases{0};
	bases.engine_base = trainer_core::get_module_offset_by_name(L"engine.dll", process_handle);
	bases.client_base = trainer_core::get_module_offset_by_name(L"client.dll", process_handle);
	bases.server_base = trainer_core::get_module_offset_by_name(L"server.dll", process_handle);

#define TEST_MODULE(m)\
	if (bases.##m##_base == NULL) {\
		 log_error(#m".dll not loaded! Please first enter the game then rerun this program."); finish(); }\
	else {log_info(#m".dll base: " + std::format("0x{:x}", \
			reinterpret_cast<unsigned long long>(bases.##m## _base)));}

	TEST_MODULE(engine);
	TEST_MODULE(client);
	TEST_MODULE(server);

#undef TEST_MODULE

#define ENABLE_FEATURE(name,description,action)\
    do {\
        action;\
        log_info_alter("Enabled: " name);\
        log_info("\t" description);\
        log_info("");\
    } while (0)

	log_info("Enter main loop...");

    set_sv_cheats(process_handle, &bases, 1);

    ENABLE_FEATURE(
        "Portal Anywhere",
        "Can place portals on any plain surfaces.",
        set_sv_portal_placement_never_fail(process_handle, &bases, 1)
    );

    ENABLE_FEATURE(
        "Mobile Portals",
        "Can place portals on moving platforms.",
        set_sv_allow_mobile_portals(process_handle, &bases, 1)
    );

    // 但是这个有时候不灵呢，不知道咋回事
    ENABLE_FEATURE(
        "\"God Mode\" Switch",
        "Press `g` to toggle invincible mode.",
        run_developer_console_command(process_handle, &bases, "bind \"g\" \"god\"")
    );

    ENABLE_FEATURE(
        "\"Noclip\" Switch",
        "Press `v` to toggle fly mode.",
        run_developer_console_command(process_handle, &bases, "bind \"v\" \"noclip\"")
    );

    ENABLE_FEATURE(
        "Dissolve Gun",
        "Press `-` to dissolve object that the crosshair is aimed at.",
        run_developer_console_command(process_handle, &bases, "bind \"-\" \"ent_fire !picker dissolve\"")
    );

    ENABLE_FEATURE(
        "Spawn Cube",
        "Press `b` to generate a standard weighted cube.",
        run_developer_console_command(process_handle, &bases, "bind \"b\" \"ent_create_portal_weighted_cube\"")
    );

    ENABLE_FEATURE(
        "Companion Cube",
        "Press `c` to turn a normal cube into a companion cube.",
        run_developer_console_command(process_handle, &bases, "bind \"c\" \"ent_fire !picker skin 1\"")
    );

    ENABLE_FEATURE(
        "Kill",
        "Press `k` to suicide.",
        run_developer_console_command(process_handle, &bases, "bind \"k\" \"kill\"")
    );

    ENABLE_FEATURE(
        "Alohomora",
        "Press `o` to open a closed door that aimed by the crosshair.",
        run_developer_console_command(process_handle, &bases, "bind \"o\" \"ent_fire !picker open\"")
    );

#undef ENABLE_FEATURE
	
    log_info("Press Ctrl+C to disable this program.");
    

	main_loop(process_handle, &bases);

    log_debug("Disabling all cheats...");
    
    set_sv_cheats(process_handle, &bases, 0);
    set_sv_allow_mobile_portals(process_handle, &bases, 0);
    set_sv_portal_placement_never_fail(process_handle, &bases, 0);

	// CloseHandle(process_handle);

	finish();
	return 0;
}
