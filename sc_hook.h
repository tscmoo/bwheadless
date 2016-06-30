#ifndef __SC_HOOK_H__
#define __SC_HOOK_H__

#include <stdio.h>
#include <windows.h>

enum {
	hookflag_eax = 1, hookflag_ecx = 2, hookflag_edx = 4, hookflag_ebx = 8, hookflag_esp = 0x10, hookflag_ebp = 0x20, hookflag_esi = 0x40, hookflag_edi = 0x80,
	hookflag_callee_cleanup = 0x100
};
#define hookflag_reg_all (hookflag_eax|hookflag_ecx|hookflag_edx|hookflag_ebx|hookflag_ebp|hookflag_esi|hookflag_edi)

#define HOOK_CDECL    (0)
#define HOOK_STDCALL  (hookflag_callee_cleanup)
#define HOOK_THISCALL (hookflag_ecx | hookflag_callee_cleanup)
#define HOOK_FASTCALL (hookflag_ecx | hookflag_edx | hookflag_callee_cleanup)

struct hook_struct {
	uint32_t ref_arg[32];
	uint32_t arg[32];
	uint32_t retval;
	bool calloriginal;
	uint32_t user[16];
	void*retaddress;
	uint32_t _eax;
	union {
		uint32_t _ecx;
		void* _this;
	};
	uint32_t _edx, _ebx, _esp, _ebp, _esi, _edi;
};

struct hook_function;
typedef void(*hook_proc)(hook_struct* e, hook_function* _f);

struct hook_function {
	hook_proc pre;
	hook_proc post;
	int flags;
	int args;
	void* address;
	void* hook_code;
	size_t hook_code_size;
	void* entry;
	size_t entry_size;
	void* patch_address;
	void* user;
};

void* hook_generate_entry(void* address, void* out_ins, size_t* out_size, void* read_address = nullptr, void* write_address = nullptr);
hook_function *hook_new(void* address, hook_proc pre, hook_proc post, int flags, int args);
hook_function *hook_new_remote(HANDLE remote_process_handle, void* address, hook_proc pre, hook_proc post, int flags, int args);
void hook_delete(hook_function*);
void hook_activate(hook_function*);
void hook_deactivate(hook_function*);
void hook_activate_remote(HANDLE remote_process_handle, hook_function*);
hook_function* hook(void* address, hook_proc pre, hook_proc post, int flags, int args);
hook_function* hook_remote(HANDLE remote_process_handle, void* address, hook_proc pre, hook_proc post, int flags, int args);
size_t hook_generate(void* out, hook_function* f, void* write_address = nullptr, hook_function* read_f = nullptr);

#endif