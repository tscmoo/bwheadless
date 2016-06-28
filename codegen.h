
#include <stdlib.h>
#include <string.h>
#include <new>

class out_buf {
public:
	virtual void puc(uint8_t) = 0;
	virtual void pus(uint16_t) = 0;
	virtual void pui(uint32_t) = 0;
	virtual uint32_t addr() = 0;
};
class out_buf_nop:public out_buf {
public:
	uint32_t size;
	unsigned int chksum;
	out_buf_nop() : size(0), chksum(0) {}
	virtual void puc(uint8_t x) {chksum+=x;size++;}
	virtual void pus(uint16_t x) {chksum+=x;size+=2;}
	virtual void pui(uint32_t x) {chksum+=x;size+=4;}
	virtual uint32_t addr() {return 0x10000000 + size;}
	bool operator!=(const out_buf_nop&n) {
		return size!=n.size||chksum!=n.chksum;
	}
};
class out_buf_ptr:public out_buf {
public:
	unsigned char*c,*oc;
	unsigned int chksum;
	out_buf_ptr(unsigned char*c) : c(c), oc(c), chksum(0) {}
	virtual void puc(uint8_t x) {chksum+=x;*c++=x;}
	virtual void pus(uint16_t x) {chksum+=x;*(uint16_t*)c=(uint16_t)(x);c+=2;}
	virtual void pui(uint32_t x) {chksum+=x;*(uint32_t*)c=(uint32_t)(x);c+=4;}
	virtual uint32_t addr() {return (uint32_t)c;}
	bool operator!=(const out_buf_ptr&n) {
		return c!=n.c||oc!=n.oc||chksum!=n.chksum;
	}
};
#define x1(x) c->puc((uint8_t)(x))
#define x2(x) c->pus((uint16_t)(x))
#define x4(x) c->pui(x)
#define xaddr c->addr()

enum reg {badreg=-1,eax=0,ecx=1,edx=2,ebx=3,esp=4,ebp=5,esi=6,edi=7};
enum cc_codes { cc_eq, cc_ne, cc_gt, cc_gtu, cc_lt, cc_ltu, cc_ge, cc_geu, cc_le, cc_leu };
class sib {
public:
	int ss,index,base;
	uint32_t disp;
	sib() {}
	sib(int ss,int index,int base) : ss(ss), index(index), base(base) {}
	void mk(out_buf*c) const {
		x1(ss<<6|index<<3|base);
	}
};
class sib_nomul: public sib {
public:
	using sib::ss;
	using sib::index;
	using sib::base;
	using sib::disp;
	sib_nomul(reg _index,reg _base) {
		ss=0, index=_index, base=_base;
	}
	sib_nomul(reg _index,uint32_t disp32) {
		ss=0, index=_index, base=ebp, disp=disp32;
	}
	sib_nomul(reg _base) {
		ss=0, index=esp, base=_base;
	}
	sib_nomul(uint32_t disp32) {
		ss=0, index=esp, base=ebp, disp=disp32;
	}
};
#define defsib(name,_ss) \
class sib_##name:public sib { \
public: \
	using sib::ss; \
	using sib::index; \
	using sib::base; \
	using sib::disp; \
	sib_##name(reg _index,reg _base) { \
	ss=_ss, index=_index, base=_base; \
} \
	sib_##name(reg _base) { \
	ss=_ss, index=esp, base=_base; \
} \
};
defsib(x2,1);
defsib(x4,2);
defsib(x8,3);
class modrm {
public:
	int mod,r,rm;
	bool has_imm32;
	bool has_imm8;
	bool has_sib;
	uint32_t imm;
	sib s;
	modrm() : has_imm32(false), has_imm8(false), has_sib(false) {}
	void mk(out_buf*c) const {
		x1(mod<<6|r<<3|rm);
		if (has_sib) s.mk(c);
		if (has_imm32||(has_sib&&mod==0&&s.base==ebp)) x4(imm);
		else if (has_imm8) x1(imm);
	}
};
class modrm_nodisp:public modrm {
public:
	using modrm::mod; using modrm::r; using modrm::rm;
	using modrm::has_imm32; using modrm::has_imm8; using modrm::has_sib;
	using modrm::imm; using modrm::s;
	modrm_nodisp(reg _r, reg _rm) {
		mod=0; r=_r; rm=_rm;
		//if (_rm==esp||_rm==ebp)  bad :(
	}
	modrm_nodisp(reg _r, uint32_t _imm) {
		mod=0; r=_r; rm=ebp; imm=_imm;
		has_imm32=true;
	}
	modrm_nodisp(reg _r, sib _s) {
		mod=0; r=_r; rm=esp; s=_s;
		has_sib = true;
	}
};
class modrm_disp8:public modrm {
public:
	using modrm::mod; using modrm::r; using modrm::rm;
	using modrm::has_imm32; using modrm::has_imm8; using modrm::has_sib;
	using modrm::imm; using modrm::s;
	modrm_disp8(reg _r, reg _rm,uint32_t disp) {
		mod=1; r=_r; rm=_rm; imm=disp;
		has_imm8=true;
		//if (_rm==esp) bad :(
	}
	modrm_disp8(reg _r,uint32_t disp, sib _s) {
		mod=1; r=_r; rm=esp; imm=disp; s = _s;;
		has_imm8=true; has_sib=true;
	}
};
class modrm_disp32:public modrm {
public:
	using modrm::mod; using modrm::r; using modrm::rm;
	using modrm::has_imm32; using modrm::has_imm8; using modrm::has_sib;
	using modrm::imm; using modrm::s;
	modrm_disp32(reg _r, reg _rm,uint32_t disp) {
		mod=2; r=_r; rm=_rm; imm=disp;
		has_imm32=true;
		//if (_rm==esp) bad :(
	}
	modrm_disp32(reg _r,uint32_t disp, sib _s) {
		mod=2; r=_r; rm=esp; imm=disp; s = _s;;
		has_imm32=true; has_sib=true;
	}
};
class modrm_dispx:public modrm {
public:
	using modrm::mod; using modrm::r; using modrm::rm;
	using modrm::has_imm32; using modrm::has_imm8; using modrm::has_sib;
	using modrm::imm; using modrm::s;
	modrm_dispx(reg _r, reg _rm,uint32_t disp) {
		r=_r; rm=_rm; imm=disp;
		if (disp==0&&_rm!=ebp) mod=0;
		else if ((int8_t)disp==disp) {mod=1;has_imm8=true;}
		else {mod=2; has_imm32=true;}
		//if (_rm==esp) bad :(
	}
	modrm_dispx(reg _r,uint32_t disp, sib _s) {
		r=_r; rm=esp; imm=disp; s = _s;;
		has_sib=true;
		if (disp==0) mod=0;
		else if ((int8_t)disp==disp) {mod=1;has_imm8=true;}
		else {mod=2; has_imm32=true;}
	}
};
class modrm_reg:public modrm {
public:
	using modrm::mod; using modrm::r; using modrm::rm;
	using modrm::has_imm32; using modrm::has_imm8; using modrm::has_sib;
	using modrm::imm; using modrm::s;
	modrm_reg(reg _r, reg _rm) {
		mod=3; r=_r; rm=_rm;
	}
};

class codegen {
public:
	out_buf*c;
	int esp_val;
	codegen(out_buf*p) : c(p), esp_val(0) {
		memset(used_registers,0,sizeof(used_registers));
	}
	void nop() {
		x1(0x90);
	}
	template<int bits> void mov_r_rm(const modrm&rm);
	template<int bits> void mov_rm_r(const modrm&rm);
	template<int bits> void mov_r_imm(reg r, uint32_t imm);
	template<int bits> void mov_rm_imm(modrm rm, uint32_t imm);
	template<int bits> void lea_r_rm(const modrm&rm);
	void call_rel32(uint32_t addr) {
		x1(0xe8);
		x4(addr-xaddr-4);
	}
	void int3() {
		x1(0xcc);
	}
	void ret() {
		x1(0xc3);
	}
	void ret_imm16(uint32_t bytes) {
		x1(0xc2);
		x2(bytes);
	}
	void pop_m32(modrm m) {
		x1(0x8F);
		m.r=0;
		m.mk(c);
		esp_val+=4;
	}
	void pop_r32(reg r) {
		x1(0x58+r);
		esp_val+=4;
	}
	void push_rm32(modrm rm) {
		x1(0xFF);
		rm.r=6;
		rm.mk(c);
		esp_val-=4;
	}
	void push_r32(reg r) {
		x1(0x50+r);
		esp_val-=4;
	}
	void push_imm8(int8_t imm) {
		x1(0x6A);
		x1(imm);
		esp_val-=4;
	}
	void push_imm32(uint32_t imm) {
		x1(0x68);
		x4(imm);
		esp_val-=4;
	}
	void jmp_rel32(uint32_t addr) {
		x1(0xe9);
		x4(addr-xaddr-4);
	}
	void jmp_rel8(uint32_t addr) {
		x1(0xeb);
		x1(addr-xaddr-1);
	}
	void jmp_relx(uint32_t addr) {if ((int8_t)(addr-xaddr-2)!=(uint32_t)(addr-xaddr-2)) jmp_rel32(addr); else jmp_rel8(addr);}
	void jz_rel32(uint32_t addr) {
		x2(0x840f);
		x4(addr-xaddr-4);
	}
	void jz_rel8(uint32_t addr) {
		x1(0x74);
		x1(addr-xaddr-1);
	}
	void jz_relx(uint32_t addr) {if ((int8_t)(addr-xaddr-2)!=(uint32_t)(addr-xaddr-2)) jz_rel32(addr); else jz_rel8(addr);}
	void jnz_rel32(uint32_t addr) {
		x2(0x850f);
		x4(addr-xaddr-4);
	}
	void jnz_rel8(uint32_t addr) {
		x1(0x75);
		x1(addr-xaddr-1);
	}
	void jnz_relx(uint32_t addr) {if ((int8_t)(addr-xaddr-2)!=(uint32_t)(addr-xaddr-2)) jnz_rel32(addr); else jnz_rel8(addr);}
	void add_eax_imm32(uint32_t imm) {x1(0x05);x4(imm);}
	void add_al_imm8(uint32_t imm) {x1(0x04);x1(imm);}
	template<int rmbits,int immbits,typename immtype> void add_rm_imm(modrm rm,immtype imm);
	template<int bits> void add_rm_immx(modrm rm,uint32_t imm);
	template<int bits> void test_rm_r(const modrm&rm);
	template<int bits> void add_rm_r(const modrm&rm);
	template<int bits> void add_r_rm(const modrm&rm);
	template<int bits> void sub_rm_r(const modrm&rm);
	template<int bits> void sub_r_rm(const modrm&rm);
	template<int bits> void jmp_rm(modrm rm);
	template<int bits> void cmp_rm_r(const modrm&rm);
	template<int bits> void jcc_rel(cc_codes cc,uint32_t addr);
	template<int bits> void shl_rm_imm(modrm rm,uint8_t imm);
	template<int bits> void shr_rm_imm(modrm rm,uint8_t imm);
	template<int bits> void sar_rm_imm(modrm rm,uint8_t imm);
	template<int bits> void shl_rm_cl(modrm rm) {
		if (bits==32) x1(0xd3);
		else if (bits==16) x2(0xd366);
		else x1(0xd2);
		rm.r = 4;
		rm.mk(c);
	}
	template<int bits> void shr_rm_cl(modrm rm) {
		if (bits==32) x1(0xd3);
		else if (bits==16) x2(0xd366);
		else x1(0xd2);
		rm.r = 5;
		rm.mk(c);
	}
	template<int bits> void sar_rm_cl(modrm rm) {
		if (bits==32) x1(0xd3);
		else if (bits==16) x2(0xd366);
		else x1(0xd2);
		rm.r = 7;
		rm.mk(c);
	}
	template<int bits> void rol_rm_imm(modrm rm,uint8_t imm) {
		if (bits==32) x1(0xc1);
		else if (bits==16) x2(0xc166);
		else x1(0xc0);
		rm.r = 0;
		rm.mk(c);
	}
	template<int bits> void rol_rm_cl(modrm rm) {
		if (bits==32) x1(0xd3);
		else if (bits==16) x2(0xd366);
		else x1(0xd2);
		rm.r = 0;
		rm.mk(c);
	}
	template<int bits> void ror_rm_imm(modrm rm,uint8_t imm) {
		if (bits==32) x1(0xc1);
		else if (bits==16) x2(0xc166);
		else x1(0xc0);
		rm.r = 1;
		rm.mk(c);
	}
	template<int bits> void ror_rm_cl(modrm rm) {
		if (bits==32) x1(0xd3);
		else if (bits==16) x2(0xd366);
		else x1(0xd2);
		rm.r = 1;
		rm.mk(c);
	}
	template<int bits> void xor_rm_imm(modrm rm,uint32_t imm) {
		if (bits==32) x1(0x81);
		else if (bits==16) x2(0x8166);
		else x1(0x80);
		rm.r = 6;
		rm.mk(c);
		if (bits==32) x4(imm);
		else if (bits==16) x2(imm);
		else x1(imm);
	}
	template<int bits> void xor_rm_r(const modrm&rm) {
		if (bits==32) x1(0x31);
		else if (bits==16) x2(0x3166);
		else x1(0x30);
		rm.mk(c);
	}
	template<int bits> void xor_r_rm(const modrm&rm) {
		if (bits==32) x1(0x33);
		else if (bits==16) x2(0x3366);
		else x1(0x32);
		rm.mk(c);
	}
	template<int bits> void or_rm_imm(modrm rm,uint32_t imm) {
		if (bits==32) x1(0x81);
		else if (bits==16) x2(0x8166);
		else x1(0x80);
		rm.r = 1;
		rm.mk(c);
		if (bits==32) x4(imm);
		else if (bits==16) x2(imm);
		else x1(imm);
	}
	template<int bits> void or_rm_r(const modrm&rm) {
		if (bits==32) x1(0x09);
		else if (bits==16) x2(0x0966);
		else x1(0x08);
		rm.mk(c);
	}
	template<int bits> void or_r_rm(const modrm&rm) {
		if (bits==32) x1(0x0b);
		else if (bits==16) x2(0x0b66);
		else x1(0x0a);
		rm.mk(c);
	}
	template<int bits> void and_rm_imm(modrm rm,uint32_t imm) {
		if (bits==32) x1(0x81);
		else if (bits==16) x2(0x8166);
		else x1(0x80);
		rm.r = 4;
		rm.mk(c);
		if (bits==32) x4(imm);
		else if (bits==16) x2(imm);
		else x1(imm);
	}
	template<int bits> void and_rm_r(const modrm&rm) {
		if (bits==32) x1(0x21);
		else if (bits==16) x2(0x2166);
		else x1(0x20);
		rm.mk(c);
	}
	template<int bits> void and_r_rm(const modrm&rm) {
		if (bits==32) x1(0x23);
		else if (bits==16) x2(0x2366);
		else x1(0x22);
		rm.mk(c);
	}
	template<int bits> void not_rm(modrm rm) {
		if (bits==32) x1(0xf7);
		else if (bits==16) x2(0xf766);
		else x1(0xf6);
		rm.r = 2;
		rm.mk(c);
	}
	void cld() {
		x1(0xfc);
	}
	int used_registers[8];
	reg get_free_preserved_reg() {
#define x(x) if (used_registers[x]<2) {if (used_registers[x]==0) push_r32(x);used_registers[x]=2;return x;}
		x(ebx);
		x(ebp);
		x(esi);
		x(edi);
		return badreg;
#undef x
	}
	void release_preserved_reg(reg r) {
		used_registers[r]=1;
	}
	void restore_preserved_registers() {
#define x(x) if (used_registers[x]!=0) {pop_r32(x);used_registers[x]=0;}
		x(edi);
		x(esi);
		x(ebp);
		x(ebx);
#undef x
	}
	void restore_esp() {
		//if (esp_val) add_rm_immx<32>(modrm_reg(badreg,esp),-esp_val);
	}
};

template<>
void codegen::mov_r_rm<32>(const modrm&rm) {
	x1(0x8b);
	rm.mk(c);
}
template<>
void codegen::mov_r_rm<16>(const modrm&rm) {
	x2(0x8b66);
	rm.mk(c);
}
template<>
void codegen::mov_r_rm<8>(const modrm&rm) {
	x1(0x8a);
	rm.mk(c);
}
template<>
void codegen::mov_rm_r<32>(const modrm&rm) {
	x1(0x89);
	rm.mk(c);
};
template<>
void codegen::mov_rm_r<16>(const modrm&rm) {
	x1(0x66);
	x1(0x89);
	rm.mk(c);
};
template<>
void codegen::mov_rm_r<8>(const modrm&rm) {
	x1(0x88);
	rm.mk(c);
};
template<>
void codegen::mov_r_imm<32>(reg r, uint32_t imm) {
	x1(0xb8 + r);
	x4(imm);
}
template<>
void codegen::mov_r_imm<16>(reg r, uint32_t imm) {
	x2(0xb866 + r);
	x2(imm);
}
template<>
void codegen::mov_r_imm<8>(reg r, uint32_t imm) {
	x1(0xb0 + r);
	x1(imm);
}
template<>
void codegen::mov_rm_imm<32>(modrm rm, uint32_t imm) {
	rm.r = 0;
	x1(0xc7);
	rm.mk(c);
	x4(imm);
}
template<>
void codegen::mov_rm_imm<16>(modrm rm, uint32_t imm) {
	rm.r = 0;
	x2(0xc766);
	rm.mk(c);
	x2(imm);
}
template<>
void codegen::mov_rm_imm<8>(modrm rm, uint32_t imm) {
	rm.r = 0;
	x1(0xc6);
	rm.mk(c);
	x1(imm);
}
template<>
void codegen::lea_r_rm<32>(const modrm&rm) {
	x1(0x8d);
	rm.mk(c);
}
template<>
void codegen::add_rm_imm<32,8,int8_t>(modrm rm,int8_t imm) {
	rm.r=0;
	if (rm.rm==esp) esp_val+=imm;
	x1(0x83);
	rm.mk(c);
	x1(imm);
}
template<>
void codegen::add_rm_imm<32,32,uint32_t>(modrm rm,uint32_t imm) {
	rm.r=0;
	if (rm.rm==esp) esp_val+=imm;
	x1(0x81);
	rm.mk(c);
	x4(imm);
}
template<>
void codegen::add_rm_immx<32>(modrm rm,uint32_t imm) {if ((int8_t)imm!=imm) add_rm_imm<32,32>(rm,imm); else add_rm_imm<32,8>(rm,(int8_t)imm);}
template<>
void codegen::add_rm_imm<16,8,int8_t>(modrm rm,int8_t imm) {
	rm.r=0;
	if (rm.rm==esp) esp_val+=imm;
	x2(0x8366);
	rm.mk(c);
	x1(imm);
}
template<>
void codegen::add_rm_imm<16,16,uint16_t>(modrm rm,uint16_t imm) {
	rm.r=0;
	if (rm.rm==esp) esp_val+=imm;
	x2(0x8166);
	rm.mk(c);
	x2(imm);
}
template<>
void codegen::add_rm_immx<16>(modrm rm,uint32_t imm) {if ((int8_t)imm!=imm) add_rm_imm<16,16>(rm,(uint16_t)imm); else add_rm_imm<16,8>(rm,(int8_t)imm);}
template<>
void codegen::add_rm_imm<8,8,int8_t>(modrm rm,int8_t imm) {
	rm.r=0;
	if (rm.rm==esp) esp_val+=imm;
	x1(0x80);
	rm.mk(c);
	x1(imm);
}
template<>
void codegen::add_rm_immx<8>(modrm rm,uint32_t imm) {add_rm_imm<8,8>(rm,(int8_t)imm);}
template<>
void codegen::add_rm_r<32>(const modrm&rm) {
	x1(0x01);
	rm.mk(c);
}
template<>
void codegen::add_rm_r<16>(const modrm&rm) {
	x2(0x0166);
	rm.mk(c);
}
template<>
void codegen::add_rm_r<8>(const modrm&rm) {
	x1(0x00);
	rm.mk(c);
}
template<>
void codegen::add_r_rm<32>(const modrm&rm) {
	x1(0x03);
	rm.mk(c);
}
template<>
void codegen::add_r_rm<16>(const modrm&rm) {
	x2(0x0366);
	rm.mk(c);
}
template<>
void codegen::add_r_rm<8>(const modrm&rm) {
	x1(0x02);
	rm.mk(c);
}
template<>
void codegen::sub_rm_r<32>(const modrm&rm) {
	x1(0x29);
	rm.mk(c);
}
template<>
void codegen::sub_rm_r<16>(const modrm&rm) {
	x2(0x2966);
	rm.mk(c);
}
template<>
void codegen::sub_rm_r<8>(const modrm&rm) {
	x1(0x28);
	rm.mk(c);
}
template<>
void codegen::sub_r_rm<32>(const modrm&rm) {
	x1(0x2b);
	rm.mk(c);
}
template<>
void codegen::sub_r_rm<16>(const modrm&rm) {
	x2(0x2b66);
	rm.mk(c);
}
template<>
void codegen::sub_r_rm<8>(const modrm&rm) {
	x1(0x2a);
	rm.mk(c);
}
template<>
void codegen::jmp_rm<32>(modrm rm) {
	rm.r=4;
	x1(0xff);
	rm.mk(c);
}
template<>
void codegen::cmp_rm_r<32>(const modrm&rm) {
	x1(0x3b);
	rm.mk(c);
}
// cc_eq, cc_ne, cc_gt, cc_gtu, cc_lt, cc_ltu, cc_ge, cc_geu, cc_le, cc_leu
template<>
void codegen::jcc_rel<32>(cc_codes cc,uint32_t addr) {
	switch (cc) {
			case cc_eq:
				x2(0x840f); // je/jz
				break;
			case cc_ne:
				x2(0x850f); // jne/jnz
				break;
			case cc_gt:
				x2(0x8f0f); // jg/jnle
				break;
			case cc_gtu:
				x2(0x870f); // ja/jnbe
				break;
			case cc_lt:
				x2(0x8c0f); // jl/jnge
				break;
			case cc_ltu:
				x2(0x820f); // jb/jc/jnae
				break;
			case cc_ge:
				x2(0x8d0f); // jge/jnl
				break;
			case cc_geu:
				x2(0x830f); // jae/jnc/jnb
				break;
			case cc_le:
				x2(0x8e0f); // jle/jng
				break;
			case cc_leu:
				x2(0x860f); // jbe/jna
				break;
			default:
				x1(0xcc);
	}
	x4(addr-xaddr-4);
}
template<>
void codegen::test_rm_r<32>(const modrm&rm)  {
	x1(0x85);
	rm.mk(c);
};
template<>
void codegen::test_rm_r<16>(const modrm&rm)  {
	x2(0x8566);
	rm.mk(c);
};
template<>
void codegen::test_rm_r<8>(const modrm&rm)  {
	x1(0x84);
	rm.mk(c);
};
template<>
void codegen::shl_rm_imm<32>(modrm rm,uint8_t imm) {
	rm.r = 4;
	x1(0xc1);
	rm.mk(c);
	x1(imm);
}
template<>
void codegen::shl_rm_imm<16>(modrm rm,uint8_t imm) {
	rm.r = 4;
	x2(0xc166);
	rm.mk(c);
	x1(imm);
}
template<>
void codegen::shl_rm_imm<8>(modrm rm,uint8_t imm) {
	rm.r = 4;
	x1(0xc0);
	rm.mk(c);
	x1(imm);
}
template<>
void codegen::shr_rm_imm<32>(modrm rm,uint8_t imm) {
	rm.r = 5;
	x1(0xc1);
	rm.mk(c);
	x1(imm);
}
template<>
void codegen::shr_rm_imm<16>(modrm rm,uint8_t imm) {
	rm.r = 5;
	x2(0xc166);
	rm.mk(c);
	x1(imm);
}
template<>
void codegen::shr_rm_imm<8>(modrm rm,uint8_t imm) {
	rm.r = 5;
	x1(0xc0);
	rm.mk(c);
	x1(imm);
}
template<>
void codegen::sar_rm_imm<32>(modrm rm,uint8_t imm) {
	rm.r = 7;
	x1(0xc1);
	rm.mk(c);
	x1(imm);
}
template<>
void codegen::sar_rm_imm<16>(modrm rm,uint8_t imm) {
	rm.r = 7;
	x2(0xc166);
	rm.mk(c);
	x1(imm);
}
template<>
void codegen::sar_rm_imm<8>(modrm rm,uint8_t imm) {
	rm.r = 7;
	x1(0xc0);
	rm.mk(c);
	x1(imm);
}

#undef x4
#undef x3
#undef x2
#undef x1