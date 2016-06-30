#include <cstdint>

#include "codegen.h"
#include "x86dec.h"

#include "sc_hook.h"
int hookflag_regs[8] = { hookflag_eax,hookflag_ecx,hookflag_edx,hookflag_ebx,hookflag_esp,hookflag_ebp,hookflag_esi,hookflag_edi };

void* hook_generate_entry(void* address, void* out_ins, size_t* out_size, void* read_address, void* write_address) {
	if (!read_address) read_address = address;
	if (!write_address) write_address = out_ins;
	decoder dec;
	uint8_t* write_out = (uint8_t*)write_address;
	uint8_t* code_out = (uint8_t*)out_ins;
	size_t total_size = 0;
	void* ret_addr = address;
	size_t size;
	uint8_t* dec_addr = (uint8_t*)read_address;
	uint8_t* code_addr = (uint8_t*)address;
	// follow any jumps if the function starts with them
	for (int i = 0; i < 10; i++) {
		if (!dec.decode(dec_addr)) {
			printf("hook entry %p; failed to decode instruction at %p (%p)", address, dec_addr, code_addr);
			throw std::runtime_error("hook_generate_entry failed");
		}
		size = dec.insn_size;
		dec_addr += size;
		code_addr += size;
		bool is_jmp = dec.opcode == 0xe9 || dec.opcode == 0xeb;
		if (is_jmp) {
			ret_addr = code_addr + dec.op_imm[0];
			continue;
		} else break;
	}
	while (total_size < 5) {
		total_size += size;

		// oh no, a relative jump. we must patch it up
		// these are all the instructions with an address-relative immediate
		// all except the loop functions have the immediate as the first operand
		// (loops have it as their second)
		bool is_jmp_short = dec.opcode == 0xeb;
		bool is_jmp_near = dec.opcode == 0xe9;
		bool is_call_near = dec.opcode == 0xe8;
		bool is_cond_jmp_short = dec.opcode >= 0x70 && dec.opcode <= 0x7f;
		bool is_cond_jmp_near = dec.opcode == 0x0f && dec.opcode2 >= 0x80 && dec.opcode2 <= 0x8f;
		bool is_loop = dec.opcode == 0xe0 || dec.opcode == 0xe1 || dec.opcode == 0xe2;
		bool is_j_cxz = dec.opcode == 0xe3;
		if (is_jmp_short || is_jmp_near || is_call_near || is_cond_jmp_short || is_cond_jmp_near || is_loop || is_j_cxz) {
			uint32_t addr = (uint32_t)(code_addr + dec.op_imm[0]);
			// check that the branch is not inside the entry
			// this routine will still generate invalid code if a function starts with, for instance, 74 00
			// but that is extremely unlikely to happen
			if (addr<(uint32_t)ret_addr || addr>(uint32_t)ret_addr + total_size) {
				uint8_t* p = dec_addr - size;
				// to be sure it'll reach, we must patch them up to use 32-bit offsets
				if (dec.op_imm_size[0] == 32) {
					// just copy the opcode
					memcpy(write_out, p, size - 4);
					write_out += size - 4;
					code_out += size - 4;
				} else if (dec.op_imm_size[0] == 16) {
					// remove the prefix, and copy the opcode
					memcpy(write_out, p + 1, size - 2 - 1);
					write_out += size - 2 - 1;
					code_out += size - 2 - 1;
				} else if (dec.op_imm_size[0] == 8) {
					uint8_t i = *p;
					if (is_jmp_short) {
						*write_out++ = 0xe9; // jmp short is 0xeb, jmp near is 0xe9
						++code_out;
					} else if (is_cond_jmp_short) { // jcc
												  // the conditional short jmp instructions are 0x70 - 0x7f
												  // the conditional long jmp instructions are 0x80-0x8f (after the 2-byte prefix 0x0f)
						*write_out++ = 0x0f;
						++code_out;
						*write_out++ = i + 0x10;
						++code_out;
					} else {
						// fixme: replace loopne, loope, loop, jcxz and jecxz by cmp and jc instructions
						//        haven't bothered since those instructions are mostly unused
						printf("hook entry %p: failed to convert instruction at %p from 8-bit relative branch to 32-bit", address, dec_addr);
						throw std::runtime_error("hook_generate_entry failed");
					}
				}
				*(uint32_t*)write_out = addr - (uint32_t)(code_out + 4);
				write_out += 4;
				code_out += 4;
				continue;
			}
		}
		memcpy(write_out, dec_addr - size, size);
		write_out += size;
		code_out += size;

		if (total_size < 5) {
			bool is_jmp = dec.opcode == 0xe9 || dec.opcode == 0xeb;
			bool is_ret = dec.opcode == 0xc2 || dec.opcode == 0xc3;
			if (is_ret || is_jmp) {
				// function end, but let's not give up quite yet... there should be some padding
				uint8_t*p = dec_addr;
				while ((*p == 0xcc || *p == 0x00) && total_size < 5) {
					total_size++;
					p++;
				}
				if (total_size >= 5) break;
				// oh well...
				printf("hook entry %p; function too small; ret or non-conditional branch found at %p (%p)", address, dec_addr, code_addr);
				throw std::runtime_error("hook_generate_entry failed");
			}

			if (!dec.decode(dec_addr)) {
				printf("hook entry %p; failed to decode instruction at %p (%p)", address, dec_addr, code_addr);
				throw std::runtime_error("hook_generate_entry failed");
			}
			size = dec.insn_size;
			dec_addr += size;
			code_addr += size;
		}
	}
	// and the jump back to the real function
	*write_out++ = 0xe9;
	++code_out;
	*(uint32_t*)write_out = ((uint32_t)ret_addr + total_size) - (uint32_t)(code_out + 4);
	write_out += 4;
	++code_out;

	if (out_size) *out_size = write_out - (uint8_t*)write_address;

	return ret_addr;
}
hook_function* hook_new(void* address, hook_proc pre, hook_proc post, int flags, int args) {
	hook_function* f = new hook_function;
	memset(f, 0, sizeof(f));
	f->address = address;
	f->pre = pre;
	f->post = post;
	f->flags = flags;
	f->args = args;
	uint8_t* p = (uint8_t*)VirtualAlloc(nullptr, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	if (!p) {
		printf("VirtualAlloc failed, error %d\n", GetLastError());
		throw std::runtime_error("hook_new failed");
	}
	f->entry = p;
	f->patch_address = hook_generate_entry(address, f->entry, &f->entry_size);
	f->hook_code = (void*)(((uint32_t)(p + f->entry_size - 1) & ~15) + 16); // align to 16 bytes
	f->hook_code_size = hook_generate(f->hook_code, f);
	return f;
}
hook_function* hook_new_remote(HANDLE remote_process_handle, void* address, hook_proc pre, hook_proc post, int flags, int args) {
	hook_function* f = new hook_function;
	memset(f, 0, sizeof(f));
	uint8_t* p = (uint8_t*)VirtualAllocEx(remote_process_handle, nullptr, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	if (!p) {
		printf("VirtualAllocEx failed, error %d\n", GetLastError());
		throw std::runtime_error("hook_new_remote failed");
	}
	hook_function* remote_f = (hook_function*)(p + 0x1000 - sizeof(hook_function));
	std::array<uint8_t, 0x100> read_buffer;
	if (!ReadProcessMemory(remote_process_handle, address, read_buffer.data(), read_buffer.size(), nullptr)) {
		printf("ReadProcessMemory failed, error %d\n", GetLastError());
		throw std::runtime_error("hook_new_remote failed");
	}
	std::array<uint8_t, 0x1000> write_buffer;
	f->address = address;
	f->pre = pre;
	f->post = post;
	f->flags = flags;
	f->args = args;
	f->entry = p;
	f->patch_address = hook_generate_entry(address, f->entry, &f->entry_size, read_buffer.data(), write_buffer.data() + ((uint8_t*)f->entry - p));
	f->hook_code = (void*)(((uint32_t)(p + f->entry_size - 1) & ~15) + 16); // align to 16 bytes
	f->hook_code_size = hook_generate(f->hook_code, remote_f, write_buffer.data() + ((uint8_t*)f->hook_code - p), f);
	memcpy(write_buffer.data() + ((uint8_t*)remote_f - p), f, sizeof(hook_function));

	if (!WriteProcessMemory(remote_process_handle, p, write_buffer.data(), write_buffer.size(), nullptr)) {
		printf("WriteProcessMemory failed, error %d\n", GetLastError());
		throw std::runtime_error("hook_new_remote failed");
	}
	return f;
}
void hook_delete(hook_function* f) {
	if (f->hook_code) VirtualFree(f->hook_code, 0, MEM_RELEASE);
	delete f;
}
void hook_activate(hook_function* f) {
	// patch up patch_address to jump to entry

	DWORD oldprot;
	VirtualProtect(f->patch_address, 0x100, PAGE_EXECUTE_READWRITE, &oldprot);
	uint8_t* out = (uint8_t*)f->patch_address;
	*out++ = 0xe9;
	*(uint32_t*)out = (uint8_t*)f->hook_code - (out + 4);
	out += 4;
}
//void hook_deactivate(hook_function*) {
// no support yet, because i'm lazy and don't need it yet
//}
void hook_activate_remote(HANDLE remote_process_handle, hook_function* f) {
	// patch up patch_address to jump to entry
	DWORD oldprot;
	VirtualProtectEx(remote_process_handle, f->patch_address, 0x100, PAGE_EXECUTE_READWRITE, &oldprot);
	char buf[5];
	uint8_t* out = (uint8_t*)buf;
	*out++ = 0xe9;
	*(uint32_t*)out = (uint8_t*)f->hook_code - ((uint8_t*)f->patch_address + 1 + 4);
	out += 4;
	if (!WriteProcessMemory(remote_process_handle, f->patch_address, buf, 5, nullptr)) {
		printf("WriteProcessMemory failed, error %d\n", GetLastError());
		throw std::runtime_error("hook_activate_remote failed");
	}
}
hook_function* hook(void* address, hook_proc pre, hook_proc post, int flags, int args) {
	hook_function* f = hook_new(address, pre, post, flags, args);
	hook_activate(f);
	return f;
}
hook_function* hook_remote(HANDLE remote_process_handle, void* address, hook_proc pre, hook_proc post, int flags, int args) {
	hook_function* f = hook_new_remote(remote_process_handle, address, pre, post, flags, args);
	hook_activate_remote(remote_process_handle, f);
	return f;
}
size_t hook_generate(void* out, hook_function* f, void* write_address, hook_function* read_f) {
	if (!write_address) write_address = out;
	if (!read_f) read_f = f;

	class code_buf_t : public out_buf {
	public:
		uint8_t* code_c;
		uint8_t* write_c;
		code_buf_t(uint8_t* code_c, uint8_t* write_c) : code_c(code_c), write_c(write_c) {}
		virtual void puc(uint8_t x) { *write_c++ = x; ++code_c;  }
		virtual void pus(uint16_t x) { *(uint16_t*)write_c = (uint16_t)(x); write_c += 2; code_c += 2; }
		virtual void pui(uint32_t x) { *(uint32_t*)write_c = (uint32_t)(x); write_c += 4; code_c += 4; }
		virtual uint32_t addr() { return (uint32_t)code_c; }
	};

	code_buf_t code_buf((uint8_t*)out, (uint8_t*)write_address);
	codegen gen(&code_buf);

	//gen.int3();

	gen.add_rm_immx<32>(modrm_reg(badreg, esp), -(int)sizeof(hook_struct));

#define rm_h(r,o,...) modrm_dispx(r,-gen.esp_val - sizeof(hook_struct) + (offsetof(hook_struct,o) __VA_ARGS__),sib_nomul(esp))

	for (int i = 0; i < 8; i++) {
		if (read_f->flags&hookflag_regs[i]) gen.mov_rm_r<32>(rm_h((reg)i, _eax, +i * 4));
	}

	gen.mov_r_rm<32>(modrm_dispx(eax, -gen.esp_val, sib_nomul(esp)));
	gen.mov_rm_r<32>(rm_h(eax, retaddress));

	int r = 2;
	for (int i = 0; i < read_f->args; i++) {
		gen.mov_r_rm<32>(modrm_dispx((reg)r, -gen.esp_val + 4 + i * 4, sib_nomul(esp)));
		gen.mov_rm_r<32>(rm_h((reg)r, arg[i]));
		gen.mov_rm_r<32>(rm_h((reg)r, ref_arg[i]));
		if (--r == -1) r = 2;
	}
	if (read_f->pre) {
		gen.mov_rm_imm<8>(rm_h(badreg, calloriginal), 1);
		if (-gen.esp_val == sizeof(hook_struct)) gen.mov_r_rm<32>(modrm_reg(eax, esp));
		else gen.lea_r_rm<32>(rm_h(eax, ref_arg[0]));
		gen.push_imm32((uint32_t)f);
		gen.push_r32(eax);
		gen.call_rel32((uint32_t)read_f->pre);
		gen.add_rm_immx<32>(modrm_reg(badreg, esp), 8);
		gen.mov_r_rm<8>(rm_h(eax, calloriginal));
		gen.test_rm_r<8>(modrm_reg(eax, eax));
		gen.push_imm32(0);
		uint32_t* retaddr = (uint32_t*)(code_buf.write_c - 4);
		for (int i = 0; i < 8; i++) {
			if (read_f->flags&hookflag_regs[i]) gen.mov_r_rm<32>(rm_h((reg)i, _eax, +4 * i));
		}
		gen.jnz_relx((uint32_t)read_f->entry);
		gen.mov_r_rm<32>(rm_h(eax, retval));
		gen.add_rm_immx<32>(modrm_reg(badreg, esp), 4 + (read_f->flags&hookflag_callee_cleanup ? 4 * read_f->args : 0));
		while (gen.c->addr() % 4) gen.nop();
		*retaddr = (uint32_t)gen.c->addr();
	} else {
		for (int i = 0; i < 8; i++) {
			if (read_f->flags&hookflag_regs[i]) gen.mov_r_rm<32>(rm_h((reg)i, _eax, +4 * i));
		}
		gen.call_rel32((uint32_t)read_f->entry);
		if (read_f->flags&hookflag_callee_cleanup) gen.esp_val += 4 * read_f->args;
	}
	if (read_f->post) {
		gen.mov_rm_r<32>(rm_h(eax, retval));
		if (-gen.esp_val == sizeof(hook_struct)) gen.mov_r_rm<32>(modrm_reg(eax, esp));
		else gen.lea_r_rm<32>(rm_h(eax, ref_arg[0]));
		gen.push_imm32((uint32_t)f);
		if (-gen.esp_val == sizeof(hook_struct)) gen.push_r32(esp);
		else gen.push_r32(eax);
		gen.call_rel32((uint32_t)read_f->post);
		gen.mov_r_rm<32>(rm_h(eax, retval));
	}

#undef rm_h
	gen.add_rm_immx<32>(modrm_reg(badreg, esp), -gen.esp_val);
	if (read_f->flags&hookflag_callee_cleanup && read_f->args) gen.ret_imm16(read_f->args * 4);
	else gen.ret();
	return code_buf.code_c - (uint8_t*)out;
}

