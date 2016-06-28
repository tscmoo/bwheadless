struct pe_info {
	void*base;
	void*entry;
};

static bool load_pe(const char*path, pe_info*ri, bool overwrite = false, const std::map<std::string, HMODULE>& loaded_modules = {}) {

	HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
	if (h == INVALID_HANDLE_VALUE) {
		log("failed to open '%s' for reading\n", path);
		return false;
	}

	void*addr = 0;

	auto cleanup = [&]() -> bool {
		CloseHandle(h);
		if (addr) VirtualFree(addr, 0, MEM_RELEASE);
		return false;
	};

	auto get = [&](void*dst, size_t size) -> bool {
		DWORD read;
		return ReadFile(h, dst, size, &read, 0) && read == size;
	};
	auto seek = [&](size_t pos) -> bool {
		return SetFilePointer(h, pos, 0, FILE_BEGIN) == pos;
	};

	IMAGE_DOS_HEADER dos;
	if (!get(&dos, sizeof(dos))) return cleanup();
	if (!seek(dos.e_lfanew)) return cleanup();
	DWORD signature;
	if (!get(&signature, 4)) return cleanup();
	if (signature != 0x00004550) return cleanup();
	IMAGE_FILE_HEADER fh;
	if (!get(&fh, sizeof(fh))) return cleanup();

	IMAGE_OPTIONAL_HEADER oh;
	memset(&oh, 0, sizeof(oh));
	get(&oh, sizeof(oh));

	//if (~fh.Characteristics&IMAGE_FILE_EXECUTABLE_IMAGE) log("file is not executable\n");
	//if (fh.Characteristics&IMAGE_FILE_RELOCS_STRIPPED) log("relocations are stripped\n");

	size_t spos = dos.e_lfanew + 4 + sizeof(IMAGE_FILE_HEADER) + fh.SizeOfOptionalHeader;

	std::vector<IMAGE_SECTION_HEADER> sh;
	sh.resize(fh.NumberOfSections);

	if (!seek(spos)) return cleanup();
	if (!get(&sh[0], sizeof(IMAGE_SECTION_HEADER)*fh.NumberOfSections)) return cleanup();

	size_t image_size = sh[fh.NumberOfSections - 1].VirtualAddress + sh[fh.NumberOfSections - 1].Misc.VirtualSize;

	if (fh.Characteristics&IMAGE_FILE_RELOCS_STRIPPED) {
		if (!overwrite) {
			addr = VirtualAlloc((void*)oh.ImageBase, image_size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
			if (!addr) log("failed to allocate memory at the required address %08X\n", oh.ImageBase);
		} else {
			addr = (void*)oh.ImageBase;
			DWORD old_protect;
			if (!VirtualProtect(addr, image_size, PAGE_EXECUTE_READWRITE, &old_protect)) {
				log("VirtualProtect failed with error %d.\n", GetLastError());
				log("We are likely to crash now.\n");
			}
		}
	} else {
		addr = VirtualAlloc(0, image_size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	}
	if (!addr) return cleanup();

	memset(addr, 0, image_size);

	if (!seek(0)) return cleanup();
	if (!get(addr, oh.SectionAlignment)) return cleanup();

	for (size_t i = 0; i < fh.NumberOfSections; i++) {
		if (sh[i].SizeOfRawData) {
			if (!seek(sh[i].PointerToRawData)) return cleanup();
			if (!get((char*)addr + sh[i].VirtualAddress, sh[i].SizeOfRawData)) return cleanup();
		}
	}

	// relocations
	uint8_t*relocs = (uint8_t*)addr + oh.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress;

	size_t pos = 0;
	while (pos < oh.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size) {
		PIMAGE_BASE_RELOCATION r = (PIMAGE_BASE_RELOCATION)(relocs + pos);
		pos += r->SizeOfBlock;
		WORD*w = (WORD*)(r + 1);
		while ((uint8_t*)w < relocs + pos) {
			//log("(DWORD*)((uint8_t*)addr + r->VirtualAddress + (*w&0xfff)); is %p\n", (DWORD*)((uint8_t*)r->VirtualAddress + (*w & 0xfff)));
			if (*w >> 12 == IMAGE_REL_BASED_HIGHLOW) {
				DWORD*target = (DWORD*)((uint8_t*)addr + r->VirtualAddress + (*w & 0xfff));
				*target -= oh.ImageBase - (DWORD)addr;
			}
			++w;
		}
	}

	// imports
	pos = 0;
	while (pos < oh.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size) {
		PIMAGE_IMPORT_DESCRIPTOR import = (PIMAGE_IMPORT_DESCRIPTOR)((uint8_t*)addr + oh.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress + pos);
		if (import->FirstThunk == 0) break;
		const char*name = (const char*)addr + import->Name;
		HMODULE hm = nullptr;
		auto i = loaded_modules.find(name);
		bool manually_loaded = i != loaded_modules.end();
		if (manually_loaded) hm = i->second;
		else hm = LoadLibraryA(name);
		if (!hm) {
			log("failed to load import library '%s'\n", name);
			return cleanup();
		} else {
			DWORD*dw = (DWORD*)((uint8_t*)addr + import->OriginalFirstThunk);
			for (int i = 0; *dw; i++) {
				FARPROC proc;
				if (manually_loaded) {

					PIMAGE_DOS_HEADER hm_dos = (PIMAGE_DOS_HEADER)hm;

					PIMAGE_OPTIONAL_HEADER hm_oh = (PIMAGE_OPTIONAL_HEADER)((char*)hm + hm_dos->e_lfanew + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER));

					auto& exp_entry = hm_oh->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];

					PIMAGE_EXPORT_DIRECTORY export = (PIMAGE_EXPORT_DIRECTORY)((char*)hm + exp_entry.VirtualAddress);

					if (*dw & 0x80000000) {
						DWORD* funcs = (DWORD*)((char*)hm + export->AddressOfFunctions);
						DWORD index = (*dw & 0xffff) - export->Base;
						if (index < export->NumberOfFunctions) proc = (FARPROC)((char*)hm + funcs[index]);
						else proc = nullptr;
					} else {
						log("fixme: load name\n");
						proc = nullptr;
					}

				} else {
					if (*dw & 0x80000000) proc = GetProcAddress(hm, (LPCSTR)(*dw & 0xffff));
					else {
						const char*name = (const char*)addr + *dw + 2;
						proc = GetProcAddress(hm, (const char*)addr + *dw + 2);
					}
				}
				if (proc) {
					*((FARPROC*)((uint8_t*)addr + import->FirstThunk) + i) = proc;
					//if (*dw&0x80000000) log("loaded %d from library '%s'\n",*dw&0xffff,name);
					//else log("loaded '%s' from library '%s'\n",(const char*)addr + *dw + 2,name);
				} else {
					if (*dw & 0x80000000) log("failed to load ordinal %d from library '%s'\n", *dw & 0xffff, name);
					else log("failed to load '%s' from library '%s'\n", (const char*)addr + *dw + 2, name);
					return cleanup();
				}
				++dw;
			}
		}
		pos += sizeof(IMAGE_IMPORT_DESCRIPTOR);
	}

	// todo: TLS, maybe other stuff

	CloseHandle(h);

	ri->base = addr;

	ri->entry = 0;

	if (oh.AddressOfEntryPoint) {
		ri->entry = (uint8_t*)addr + oh.AddressOfEntryPoint;
	}

	return true;
}
