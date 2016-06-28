#define _CRT_SECURE_NO_WARNINGS
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <windows.h>

#include <array>
#include <vector>

#include "sc_hook.h"

#include "strf.h"

template<typename...T>
std::string format(const char*fmt, T&&...args) {
	std::string r;
	tsc::strf::format(r, fmt, std::forward<T>(args)...);
	return r;
}

HANDLE output_handle = INVALID_HANDLE_VALUE;

template<typename...T>
void log(const char*fmt, T&&...args) {
	auto s = format(fmt, std::forward<T>(args)...);
	DWORD written;
	WriteFile(output_handle, s.data(), s.size(), &written, nullptr);
}

void inject(HANDLE h_proc);
void image_set(const void*dst, const void*src, size_t size);
extern int is_injected;
extern HMODULE hmodule;
DWORD main_thread_id;

void fatal_error(const char* desc) {
	log("fatal error: %s\n", desc);
	TerminateProcess(GetCurrentProcess(), (UINT)-1);
}

std::string escape_nonprintable(const char* ptr) {
	std::string str;
	str.reserve(strlen(ptr) + 8);
	for (const char* c = ptr; *c; ++c) {
		if (*c == '\\') str += "\\\\";
		else if (*c == '\n' || (*c >= 0x20 && *c <= 0x7e)) str += *c;
		else {
			str += "\\x";
			auto hex = [](int n) {
				return n <= 9 ? '0' + n : 'a' + (n - 10);
			};
			str += hex(*c >> 8);
			str += hex(*c & 0xf);
		}
	}
	return str;
}

std::string remove_nonprintable(const char* ptr) {
	std::string str;
	str.reserve(strlen(ptr) + 8);
	for (const char* c = ptr; *c; ++c) {
		if (*c == '\n' || (*c >= 0x20 && *c <= 0x7e)) str += *c;
	}
	return str;
}

template<typename T>
T& offset(void* p, size_t offset) {
	return *(T*)(((char*)p) + offset);
}

void string_copy(char* dst, const char* src, size_t max_size) {
	const char* e = dst + max_size - 1;
	while (*src && dst != e) *dst++ = *src++;
	*dst = 0;
}

int InitializeNetworkProvider(int id) {
	int r;
	__asm {
		mov ebx, id;
		mov edx, 0x4D3CC0;
		call edx;
		mov r, eax;
	}
	return r;
}

template<typename F>
void enum_games(const F& f) {
	void* f_ptr = (void*)&f;
	void(__stdcall*cb_ptr)(void*, void*) = [](void* data, void* user) {
		(*(const F*)user)(data);
	};
	_asm {
		mov edi, f_ptr;
		push cb_ptr;
		mov edx, 0x4D37C0;
		call edx;
	}
}

int JoinNetworkGame(void* data) {
	int r;
	__asm {
		mov ebx, data;
		mov edx, 0x4D3B50;
		call edx;
		mov r, eax
	};
	return r;
}

void change_race(int race, int slot = 8) {
	__asm {
		mov eax, slot;
		push race;
		mov edx, 0x452370;
		call edx;
	}
}

const char* get_string(uint16_t index) {
	const char* r;
	__asm {
		movzx edx, index;
		mov eax, 0x4BD0C0;
		call eax;
		mov r, eax;
	}
}

int SErrGetLastError() {
	return ((int(*)())0x4100FA)();
}

void make_stat_string(char* out_str, void* data, size_t len) {
	__asm {
		mov eax, out_str;
		mov ebx, data;
		push len;
		mov edx, 0x472300;
		call edx;
	}
}

uint32_t get_mode_flags(void* data) {
	uint32_t r;
	__asm {
		mov ecx, data;
		mov edx, 0x4AADA0;
		call edx;
		mov r, eax;
	}
	return r;
}

void SetReplayData(void* game_data, void* players, void* player_colors) {
	__asm {
		mov ebx, player_colors;
		mov edx, players;
		mov eax, game_data;
		mov ecx, 0x4DEA10;
		call ecx;
	}
}


void newGame(int a) {
	__asm {
		mov eax, a;
		mov edx, 0x485BB0;
		call edx;
	}
}

int GameLoop_State(int a) {
	int r;
	__asm {
		mov ecx, a;
		mov edx, 0x4D9670;
		call edx;
		mov r, eax;
	}
	return r;
}

void UpdateVisibilityHash(int y) {
	__asm {
		mov ebx, y;
		mov edx, 0x497C30;
		call edx;
	};
}

int16_t& g_game_mode = *(int16_t*)0x596904;
int16_t& g_tileset = *(int16_t*)0x57F1DC;

int& g_local_storm_id = *(int*)0x51268C;

bool& g_is_multiplayer = *(bool*)0x57F0B4;
int& g_is_host = *(int*)0x596888;

int& g_allow_random = *(int*)0x6D11C8;
bool& g_is_replay = *(bool*)0x6D0F14;

void* g_game_data = (void*)0x5967F8;
void* g_players = (void*)0x57EEE0;
void* g_player_colors = (void*)0x57F21C;

void run() {
	log("this is WinMain\n");

	const char* player_name = "playername";

	bool host_game = false;
	const char* game_name = "gamename";

	((void(*)())0x04DAF30)(); // PreInitData

	((void(*)())0x4D7390)(); // loadInitIscriptBIN

	g_is_multiplayer = true;
	*(bool*)0x58F440 = true; // isExpansion
	g_game_mode = 4;

	string_copy((char*)0x57EE9C, player_name, 25); // player name

	if (!InitializeNetworkProvider('SMEM')) {
		fatal_error("InitializeNetworkProvider failed");
	}

	log("game mode is %d\n", g_game_mode);

	if (host_game) {

		const char* map_fn = "maps/(2)Ride of Valkyries.scx";

		uint32_t data[8];
		if (!((int(__stdcall*)(const char*, uint32_t*, int))0x4BF5D0)(map_fn, data, 0)) { // ReadMapData
			log("failed to read map '%s'\n", map_fn);
			fatal_error("failed to load map");
		}

		const char* scenario_name = get_string(data[0] & 0xffff);
		const char* scenario_description = get_string(data[0] >> 16);

		std::array<uint8_t, 8>& player_force = (std::array<uint8_t, 8>&)data[1];
		std::array<uint16_t, 4>& force_name_index = (std::array<uint16_t, 4>&)data[3];
		std::array<uint8_t, 4>& force_flags = (std::array<uint8_t, 4>&)data[5];

		for (size_t i = 0; i < 8; ++i) log("player %d force: %d\n", i, player_force[i]);
		for (size_t i = 0; i < 4; ++i) log("force %d name '%s' flags %d\n", i, remove_nonprintable(get_string(force_name_index[i])).c_str(), force_flags[i]);

		uint16_t& map_tile_width = *(uint16_t*)0x57F1D6;
		uint16_t& map_tile_height = *(uint16_t*)0x57F1D6;

		log("scenario name: '%s'  description: '%s'\n", remove_nonprintable(scenario_name).c_str(), remove_nonprintable(scenario_description).c_str());
		log("size %d %d\n", map_tile_width, map_tile_height);

		void* players_struct = (void*)0x59BDB0;
		int players_count = 0;
		for (size_t i = 0; i < 12; ++i) {
			int type = offset<uint8_t>(players_struct, 0x24 * i);
			if (type == 2 || type == 6) ++players_count;
		}

		log("%d players\n", players_count);

		std::array<uint8_t, 0x8d> create_info {};

		string_copy(&offset<char>(&create_info, 0x4), player_name, 24);
		string_copy(&offset<char>(&create_info, 0x34), game_name, 25);
		string_copy(&offset<char>(&create_info, 0x4d), scenario_name, 32);

		offset<uint16_t>(&create_info, 0x20) = map_tile_width;
		offset<uint16_t>(&create_info, 0x22) = map_tile_height;

		uint32_t game_type = 0x10002;

		offset<uint8_t>(&create_info, 0x24) = 1; // active player count
		offset<uint8_t>(&create_info, 0x25) = players_count;
		offset<uint8_t>(&create_info, 0x27) = 1; // game state?
		offset<uint32_t>(&create_info, 0x28) = game_type; // game type (melee)
		offset<uint16_t>(&create_info, 0x30) = g_tileset;

		int& game_speed = *(int*)0x6CDFD4;

		game_speed = 6;
		offset<uint32_t>(&create_info, 0x6d) = game_type; // game type (melee)
		offset<uint16_t>(&create_info, 0x71) = 0;
		offset<uint16_t>(&create_info, 0x73) = 0;

		offset<uint8_t>(&create_info, 0x75) = 1; // various game settings stuff
		offset<uint8_t>(&create_info, 0x76) = 1; // some of these would need to be changed for ums
		offset<uint8_t>(&create_info, 0x77) = 1;
		offset<uint8_t>(&create_info, 0x78) = 2;
		offset<uint8_t>(&create_info, 0x79) = 2;
		offset<uint8_t>(&create_info, 0x7a) = 0;
		offset<uint8_t>(&create_info, 0x7b) = 3;
		offset<uint8_t>(&create_info, 0x7c) = 1;
		offset<uint8_t>(&create_info, 0x7d) = 0;
		offset<uint8_t>(&create_info, 0x7e) = 1;

		offset<uint32_t>(&create_info, 0x84) = 50; // starting minerals
		offset<uint32_t>(&create_info, 0x88) = 0;  // starting gas

		offset<uint32_t>(&create_info, 0x2c) = ((uint32_t(*)())0x4D9CD0)(); // makeStringHash

		char stat_string[128];
		make_stat_string(stat_string, create_info.data() + 0x1c, 128);

		log("stat string '%s'\n", escape_nonprintable(stat_string));

		void* net_create_info = create_info.data() + 0x6d;

		uint32_t mode_flags = get_mode_flags(net_create_info);

		log("mode_flags %x\n", mode_flags);

		int(__stdcall*SNetCreateLadderGame)(const char* game_name, const char* password, const char* stat, uint32_t game_type, uint32_t ladder_type, uint32_t mode_flags, void* net_create_info, size_t net_create_info_size, int players, const char* host_player_name, const char*, int* player_id);
		(void*&)SNetCreateLadderGame = (void*)0x410118;

		if (!SNetCreateLadderGame(game_name, nullptr, stat_string, game_type, 0, mode_flags, net_create_info, 0x20, players_count, player_name, "", &g_local_storm_id)) {
			log("failed to create game, error %d\n", SErrGetLastError());
			fatal_error("failed to create game");
		}

		memcpy(g_game_data, create_info.data(), create_info.size());
		g_is_host = 1;

	} else {

		void* join_game_data = nullptr;
		while (true) {

			enum_games([&](void* ptr) {
				log("game %s\n", (char*)ptr + 4);
				if (!join_game_data) join_game_data = ptr;
			});

			if (join_game_data) break;

			log(".");

			Sleep(100);
		}

		log("game mode is %d\n", g_game_mode);

		log("joining game '%s' on map '%s'\n", (char*)join_game_data + 4, (char*)join_game_data + 0x4d);

		if (!JoinNetworkGame(join_game_data)) {
			fatal_error("JoinNetworkGame failed");
		}

	}

	if (!((int(*)())0x4D4130)()) {
		if (g_is_host) fatal_error("failed to host game");
		else fatal_error("failed to join game");
	}

	log("game mode is %d\n", *(int*)0x596904);

	int race = 2; // 0 zerg, 1 terran, 2 protoss, 6 random

	int& local_nation_id = *(int*)0x512684;
	bool race_changed = false;

	while (true) {

		int r = ((int(__stdcall*)(int))0x4D4340)(0); // LobbyLoopCnt
		if (r != 81) log("r %d\n", r);

		if (local_nation_id != -1 && !race_changed) {
			race_changed = true;
			change_race(race, local_nation_id);
			log("race changed\n");

			const char* cstr = "hello world";
			__asm {
				mov edi, cstr;
				mov edx, 0x4707D0;
				call edx;
			}
		}

		if (r == 83) break;

		if (r == 81) Sleep(1);
	}

	int& game_mode = *(int*)0x596904;

	if (game_mode != 1) {
		log("error: game_mode is %d\n", game_mode);
		fatal_error("unexpected game mode");
	}
	game_mode = 3;

	g_allow_random = 1;
	if (!((int(*)())0x4EF100)()) { // LoadGameInit
		fatal_error("LoadGameInit failed");
	}
	g_allow_random = 0;

	if (!g_is_replay) {
		SetReplayData(g_game_data, g_players, g_player_colors);
	}
	((void(*)())0x4CE440)(); // closeLoadGameFile


	*(int*)0x51CE98 = 1; // update_tiles_countdown

	int& game_state = *(int*)0x6D11EC;
	game_state = 1; // game state

	((void(__stdcall*)(int))0x488790)(0); // TickCountSomething

	((void(*)())0x4D9530)(); // DoGameLoop

	*(int*)0x6D121C = 0; // mapStarted

	log("local_mask is %d\n", *(int*)0x057F0B0);
	log("g_LocalNationID is %d\n", *(int*)0x512684);

	newGame(1);

	((void(*)())0x4D0820)(); // loseSightSelection

	*(int*)0x57EEBC = 0; // turnCounter

	((void(*)())0x485AA0)(); // GameKeepAlive

	bool& allow_send_turn = *(bool*)0x57EE78;
	while (true) {
		int r = ((int(*)())0x487070)(); // gameLoopTurns
		//log("GameLoopTurns %d\n", r);
		if (r) break;
		if (game_state == 0) break;
	}

	*(int*)0x51CEA0 = 1; // currentSpeedLatFrameCount
	*(char*)0x51CE9D = 0; // wantMoreTurns

	while (game_state) {

		int r = ((int(*)())0x485F70)(); // RecvMessage

		//log("recv %d\n", r);

		*(int*)0x051CE94 = 0; // nextTickCount

		GameLoop_State(0);

		int game_loop_break_reason = *(int*)0x6D11F0;

// 		log("gameloop break reason is %d\n", *(int*)0x6D11F0);
// 		log("elapsedTime: %d\n", *(int*)0x057F23C);
// 
// 		log("active player count: %d\n", ((int(*)())0x4C40F0)());

		int map_tile_height = *(int16_t*)0x057F1D6;
		((void(__stdcall*)(int, int))0x4982D0)(0, map_tile_height - 1); // DoVisibilityUpdate

		if (game_loop_break_reason >= 5) Sleep(1);

	}

	log("game over\n");
}

void _WinMain_pre(hook_struct*e, hook_function*_f) {

	e->calloriginal = false;
	e->retval = 0;

	try {
		run();
	} catch (const std::exception& e) {
		log("exception %s\n", e.what());
		fatal_error("exception");
	}

	log("done!\n");

	TerminateProcess(GetCurrentProcess(), 0);

}

void _doNetTBLError_pre(hook_struct*e, hook_function*_f) {

	log("net error %d\n", e->arg[0]);

	uint16_t* tbl = *(uint16_t**)0x6D1220;
	size_t n = e->arg[0];
	if (n <= tbl[0]) {
		const char* str = (char*)tbl + tbl[n];
		log("%s\n", str);
	} else log("no error string\n");

	fatal_error("network error");

}

void _on_lobby_chat_pre(hook_struct* e, hook_function* _f) {
	e->calloriginal = false;
	log("chat (%d): %s\n", e->arg[0], (char*)e->_eax);
}

void _on_lobby_start_game_pre(hook_struct* e, hook_function* _f) {
	e->calloriginal = false;
	log("game start!\n");
}

void _timeoutProcDropdown_pre(hook_struct* e, hook_function* _f) {
	e->calloriginal = false;

	bool& allow_send_turn = *(bool*)0x57EE78;

	while (!allow_send_turn) {
		log("waiting for players...\n");
		((int(*)())0x485F70)(); // RecvMessage

		int r = ((int(*)())0x486580)(); // RecvSaveTurns
		Sleep(250);
	}

}

void _SetInGameInputProcs_pre(hook_struct* e, hook_function* _f) {
	e->calloriginal = false;
	//log("SetInGameInputProcs called from %p\n", e->retaddress);
	if (e->retaddress == (void*)0x46161A || e->retaddress == (void*)0x4616aa) {
		log("game has ended\n");
		TerminateProcess(GetCurrentProcess(), 0);
	}
	//log("ignoring SetInGameInputProcs\n");
}

void _DisplayTextMessage_pre(hook_struct* e, hook_function* _f) {
	e->calloriginal = false;
	log(":: %s\n", remove_nonprintable((const char*)e->_eax).c_str());
}

HANDLE handle_process = INVALID_HANDLE_VALUE;

int main() {

	output_handle = GetStdHandle(STD_OUTPUT_HANDLE);

	if (is_injected) {
		
		AttachConsole(ATTACH_PARENT_PROCESS);

		log("hello world\n");
		log("I am loaded at 0x%p, target process is loaded at 0x%p\n", hmodule, GetModuleHandle(0));

		log("load bwapi: %p\n", LoadLibraryA("BWAPI.dll"));

		HANDLE h = OpenThread(THREAD_SUSPEND_RESUME, FALSE, main_thread_id);

		hook((void*)0x4E0AE0, _WinMain_pre, nullptr, HOOK_STDCALL, 4);

		hook((void*)0x4BB300, _doNetTBLError_pre, nullptr, HOOK_STDCALL | hookflag_eax | hookflag_ecx | hookflag_edx, 1);

		hook((void*)0x4B8F10, _on_lobby_chat_pre, nullptr, HOOK_STDCALL | hookflag_eax, 1);

		hook((void*)0x44FD30, _on_lobby_start_game_pre, nullptr, HOOK_STDCALL, 1);

		hook((void*)0x4A3380, _timeoutProcDropdown_pre, nullptr, HOOK_CDECL, 0);
		
		hook((void*)0x484CC0, _SetInGameInputProcs_pre, nullptr, HOOK_CDECL, 0);

		hook((void*)0x48CD30, _DisplayTextMessage_pre, nullptr, HOOK_STDCALL | hookflag_eax, 3);

		auto r = ResumeThread(h);
		log("resume: %d error %d\n", r, GetLastError());

		ExitThread(0);

	} else {

		SetConsoleCtrlHandler([](DWORD type) {
			if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT || type == CTRL_CLOSE_EVENT) {
				if (handle_process != INVALID_HANDLE_VALUE) TerminateProcess(handle_process, (UINT)-1);
			}
			return FALSE;
		}, TRUE);

		STARTUPINFOA si;
		PROCESS_INFORMATION pi;
		memset(&si, 0, sizeof(si));
		si.cb = sizeof(si);

		std::string cmd = "Starcraft_multiinstance.exe";
		//std::string cmd = "Starcraft.exe";
		std::string dir = ".";
		if (!CreateProcessA(0, (LPSTR)cmd.c_str(), 0, 0, TRUE, CREATE_SUSPENDED, 0, dir.c_str(), &si, &pi)) {
			log("Failed to start '%s'; error %d\n", cmd.c_str(), GetLastError());
			return -1;
		}

		handle_process = pi.hProcess;

		image_set(&main_thread_id, &pi.dwThreadId, sizeof(pi.dwThreadId));
		inject(pi.hProcess);

		WaitForSingleObject(pi.hProcess, INFINITE);

	}

}

