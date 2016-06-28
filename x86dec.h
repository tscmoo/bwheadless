
#include <cstdint>
#include <array>
struct decoder {
	uint8_t prefix_seg;
	uint8_t prefix_address_size;
	uint8_t prefix_lock;
	uint8_t prefix_operand_size;
	uint8_t prefix_string;

	int operand_size;
	int address_size;
	uint8_t opcode;
	uint8_t opcode2;
	uint8_t opcode3;
	uint8_t modrm;
	int32_t disp;
	std::array<int32_t, 3> op_imm;
	std::array<int, 3> op_imm_size;
	int insn_size;

	bool decode(uint8_t* begin) {
		uint8_t* p = begin;
		opcode = 0;
		opcode2 = 0;
		opcode3 = 0;
		modrm = 0;
		disp = 0;
		op_imm = { 0,0,0 };
		op_imm_size = { 0,0,0 };
		insn_size = 0;

		auto op_disp = [&](int size) {
			if (size == 8) disp = (int8_t)*p++;
			else if (size == 16) disp = (uint16_t)p[0] | ((int16_t)(int8_t)p[1] << 8), p += 2;
			else disp = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)(int8_t)p[3] << 24), p += 4;
		};
		auto op_mr = [&]() {
			if (!(~modrm & (3 << 6))) return;
			uint8_t mod = modrm >> 6 & 3;
			uint8_t rm = modrm & 7;
			if (address_size == 16) {
				if ((mod == 0 && rm == 6) || mod == 2) op_disp(16);
				else if (mod == 1) op_disp(8);
			} else if (address_size == 32) {
				if (rm == 4) {
					if ((*p++ & 7) == 5) {
						return op_disp(mod == 1 ? 8 : 32);
					}
				}
				if ((mod == 0 && rm == 5) || mod == 2) op_disp(32);
				else if (mod == 1) op_disp(8);
			}
		};

		prefix_seg = 0;
		prefix_address_size = 0;
		prefix_lock = 0;
		prefix_operand_size = 0;
		prefix_string = 0;
		while (true) {
			switch (*p) {
			case 0x26: case 0x2e: case 0x36: case 0x3e:
			case 0x64: case 0x65:
				prefix_seg = *p++;
				continue;
			case 0x66:
				prefix_operand_size = *p++;
				continue;
			case 0x67:
				prefix_address_size = *p++;
				continue;
			case 0xf0:
				prefix_lock = *p++;
				continue;
			case 0xf2: case 0xf3:
				prefix_string = *p++;
				continue;
			}
			break;
		}
		operand_size = prefix_operand_size ? 16 : 32;
		address_size = prefix_address_size ? 16 : 32;
		opcode = *p;
		switch (*p++) {
		case 210: case 211:
			((modrm = *p++) >> 3 & 7);
			op_mr();
			break;
		case 208: case 209:
			((modrm = *p++) >> 3 & 7);
			op_mr();
			op_imm[1] = 1;
			break;
		case 128: case 130: case 131: case 192: case 193:
			((modrm = *p++) >> 3 & 7);
			op_mr();
			op_imm_size[1] = 8, op_imm[1] = (int8_t)*p++;
			break;
		case 129:
			((modrm = *p++) >> 3 & 7);
			op_mr();
			operand_size == 16 ? op_imm_size[1] = 16, op_imm[1] = (uint16_t)p[0] | ((int16_t)(int8_t)p[1] << 8), p += 2 : (op_imm_size[1] = 32, op_imm[1] = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((int32_t)(int8_t)p[3] << 24), p += 4);
			break;
		case 142:
			(modrm = *p++);
			break;
		case 98: case 141: case 196: case 197:
			(modrm = *p++);
			if (!(~modrm & (3 << 6))) return false;
			op_mr();
			break;
		case 0: case 1: case 2: case 3: case 8: case 9: case 10: case 11: case 16: case 17: case 18: case 19: case 24: case 25: case 26: case 27: case 32: case 33: case 34: case 35: case 40: case 41: case 42: case 43: case 48: case 49: case 50: case 51: case 56: case 57: case 58: case 59: case 99: case 132: case 133: case 134: case 135: case 136: case 137: case 138: case 139:
			(modrm = *p++);
			op_mr();
			break;
		case 107:
			(modrm = *p++);
			op_mr();
			op_imm_size[2] = 8, op_imm[2] = (int8_t)*p++;
			break;
		case 105:
			(modrm = *p++);
			op_mr();
			operand_size == 16 ? op_imm_size[2] = 16, op_imm[2] = (uint16_t)p[0] | ((int16_t)(int8_t)p[1] << 8), p += 2 : (op_imm_size[2] = 32, op_imm[2] = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((int32_t)(int8_t)p[3] << 24), p += 4);
			break;
		case 216: case 218: case 220: case 222: case 223:
			if (!((~(modrm = *p++) & (3 << 6)) == 0)) {
				if (!((~modrm & (3 << 6)) == 0)) {
					if (!(~modrm & (3 << 6))) return false;
					op_mr();
				}
			}
			break;
		case 221:
			if (!((~(modrm = *p++) & (3 << 6)) == 0)) {
				switch (modrm >> 3 & 7) {
				case 0: case 1: case 2: case 3: case 4: case 6: case 7:
					if (!((~modrm & (3 << 6)) == 0)) {
						if (!(~modrm & (3 << 6))) return false;
						op_mr();
					}
					break;
				}
			}
			break;
		case 219:
			if (!((~(modrm = *p++) & (3 << 6)) == 0)) {
				switch (modrm >> 3 & 7) {
				case 0: case 1: case 2: case 3: case 5: case 7:
					if (!((~modrm & (3 << 6)) == 0)) {
						if (!(~modrm & (3 << 6))) return false;
						op_mr();
					}
					break;
				}
			}
			break;
		case 217:
			if (!((~(modrm = *p++) & (3 << 6)) == 0)) {
				switch (modrm >> 3 & 7) {
				case 0: case 2: case 3: case 4: case 5: case 6: case 7:
					if (!((~modrm & (3 << 6)) == 0)) {
						if (!(~modrm & (3 << 6))) return false;
						op_mr();
					}
					break;
				}
			}
			break;
		case 198:
			if (((modrm = *p++) >> 3 & 7) == 0) {
				op_mr();
				op_imm_size[1] = 8, op_imm[1] = (int8_t)*p++;
			}
			break;
		case 199:
			if (((modrm = *p++) >> 3 & 7) == 0) {
				op_mr();
				operand_size == 16 ? op_imm_size[1] = 16, op_imm[1] = (uint16_t)p[0] | ((int16_t)(int8_t)p[1] << 8), p += 2 : (op_imm_size[1] = 32, op_imm[1] = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((int32_t)(int8_t)p[3] << 24), p += 4);
			}
			break;
		case 143:
			if (((modrm = *p++) >> 3 & 7) == 0) {
				op_mr();
			}
			break;
		case 227:
			if (prefix_address_size == 32) {
				op_imm_size[0] = 8, op_imm[0] = (int8_t)*p++;
			} else {
				op_imm_size[0] = 8, op_imm[0] = (int8_t)*p++;
			}
			break;
		case 160: case 161: case 162: case 163:
			op_disp(address_size);
			break;
		case 194: case 202:
			op_imm_size[0] = 16, op_imm[0] = (uint16_t)p[0] | ((int16_t)(int8_t)p[1] << 8), p += 2;
			break;
		case 200:
			op_imm_size[0] = 16, op_imm[0] = (uint16_t)p[0] | ((int16_t)(int8_t)p[1] << 8), p += 2;
			op_imm_size[1] = 8, op_imm[1] = (int8_t)*p++;
			break;
		case 106: case 112: case 113: case 114: case 115: case 116: case 117: case 118: case 119: case 120: case 121: case 122: case 123: case 124: case 125: case 126: case 127: case 205: case 212: case 213: case 224: case 225: case 226: case 230: case 231: case 235:
			op_imm_size[0] = 8, op_imm[0] = (int8_t)*p++;
			break;
		case 4: case 12: case 20: case 28: case 36: case 44: case 52: case 60: case 168: case 176: case 177: case 178: case 179: case 180: case 181: case 182: case 183: case 228: case 229:
			op_imm_size[1] = 8, op_imm[1] = (int8_t)*p++;
			break;
		case 15:
			opcode2 = *p;
			switch (*p++) {
			case 13:
				((modrm = *p++) >> 3 & 7);
				if (!(~modrm & (3 << 6))) return false;
				op_mr();
				break;
			case 178: case 180: case 181:
				(modrm = *p++);
				if (!(~modrm & (3 << 6))) return false;
				op_mr();
				break;
			case 34: case 35:
				(modrm = *p++);
				if ((~modrm & (3 << 6))) return false;
				op_mr();
				break;
			case 2: case 3: case 64: case 65: case 66: case 67: case 68: case 69: case 70: case 71: case 72: case 73: case 74: case 75: case 76: case 77: case 78: case 79: case 120: case 121: case 144: case 145: case 146: case 147: case 148: case 149: case 150: case 151: case 152: case 153: case 154: case 155: case 156: case 157: case 158: case 159: case 163: case 165: case 171: case 173: case 175: case 176: case 177: case 179: case 182: case 183: case 187: case 188: case 189: case 190: case 191: case 192: case 193:
				(modrm = *p++);
				op_mr();
				break;
			case 164: case 172:
				(modrm = *p++);
				op_mr();
				op_imm_size[2] = 8, op_imm[2] = (int8_t)*p++;
				break;
			case 174:
				if (!((~(modrm = *p++) & (3 << 6)) == 0)) {
					switch (modrm >> 3 & 7) {
					case 0: case 1: case 2: case 3: case 4: case 5: case 7:
						if (!((~modrm & (3 << 6)) == 0)) {
							if (!(~modrm & (3 << 6))) return false;
							op_mr();
						}
						break;
					}
				}
				break;
			case 25: case 26: case 27: case 28: case 29: case 30: case 31: case 195:
				if (!(~(modrm = *p++) & (3 << 6))) return false;
				op_mr();
				break;
			case 240:
				if ((prefix_string ? prefix_string == 0xf2 ? 1 : 2 : prefix_operand_size ? 3 : 0) == 1) {
					if (prefix_string) prefix_string = 0;
					else prefix_operand_size = 0, operand_size = 32;
					(modrm = *p++);
					if (!(~modrm & (3 << 6))) return false;
					op_mr();
				}
				break;
			case 184:
				if ((prefix_string ? prefix_string == 0xf2 ? 1 : 2 : prefix_operand_size ? 3 : 0) == 2) {
					if (prefix_string) prefix_string = 0;
					else prefix_operand_size = 0, operand_size = 32;
					(modrm = *p++);
					op_mr();
				}
				break;
			case 108: case 109:
				if ((prefix_string ? prefix_string == 0xf2 ? 1 : 2 : prefix_operand_size ? 3 : 0) == 3) {
					if (prefix_string) prefix_string = 0;
					else prefix_operand_size = 0, operand_size = 32;
					(modrm = *p++);
					op_mr();
				}
				break;
			case 1:
				if ((~(modrm = *p++) & (3 << 6)) == 0) {
					if ((modrm >> 3 & 7) == 6) {
						if ((~modrm & (3 << 6)) == 0) {
							op_mr();
						}
					}
				} else {
					switch (modrm >> 3 & 7) {
					case 0: case 1: case 2: case 3: case 7:
						if (!((~modrm & (3 << 6)) == 0)) {
							if (!(~modrm & (3 << 6))) return false;
							op_mr();
						}
						break;
					case 6:
						if (!((~modrm & (3 << 6)) == 0)) {
							op_mr();
						}
						break;
					}
				}
				break;
			case 247:
				if ((~(modrm = *p++) & (3 << 6)) == 0) {
					switch (prefix_string ? prefix_string == 0xf2 ? 1 : 2 : prefix_operand_size ? 3 : 0) {
					case 0: case 1: case 2:
						if ((~modrm & (3 << 6)) == 0) {
							if ((~modrm & (3 << 6))) return false;
							op_mr();
						}
						break;
					case 3:
						if (prefix_string) prefix_string = 0;
						else prefix_operand_size = 0, operand_size = 32;
						if ((~modrm & (3 << 6)) == 0) {
							if ((~modrm & (3 << 6))) return false;
							op_mr();
						}
						break;
					}
				}
				break;
			case 22:
				if ((~(modrm = *p++) & (3 << 6)) == 0) {
					switch (prefix_string ? prefix_string == 0xf2 ? 1 : 2 : prefix_operand_size ? 3 : 0) {
					case 0: case 1: case 3:
						if ((~modrm & (3 << 6)) == 0) {
							if ((~modrm & (3 << 6))) return false;
							op_mr();
						}
						break;
					case 2:
						if (prefix_string) prefix_string = 0;
						else prefix_operand_size = 0, operand_size = 32;
						if ((~modrm & (3 << 6)) == 0) {
							op_mr();
						}
						break;
					}
				} else {
					switch (prefix_string ? prefix_string == 0xf2 ? 1 : 2 : prefix_operand_size ? 3 : 0) {
					case 0: case 1:
						if (!((~modrm & (3 << 6)) == 0)) {
							if (!(~modrm & (3 << 6))) return false;
							op_mr();
						}
						break;
					case 3:
						if (prefix_string) prefix_string = 0;
						else prefix_operand_size = 0, operand_size = 32;
						if (!((~modrm & (3 << 6)) == 0)) {
							if (!(~modrm & (3 << 6))) return false;
							op_mr();
						}
						break;
					case 2:
						if (prefix_string) prefix_string = 0;
						else prefix_operand_size = 0, operand_size = 32;
						if (!((~modrm & (3 << 6)) == 0)) {
							op_mr();
						}
						break;
					}
				}
				break;
			case 18:
				if ((~(modrm = *p++) & (3 << 6)) == 0) {
					switch (prefix_string ? prefix_string == 0xf2 ? 1 : 2 : prefix_operand_size ? 3 : 0) {
					case 0: case 3:
						if ((~modrm & (3 << 6)) == 0) {
							if ((~modrm & (3 << 6))) return false;
							op_mr();
						}
						break;
					case 1: case 2:
						if (prefix_string) prefix_string = 0;
						else prefix_operand_size = 0, operand_size = 32;
						if ((~modrm & (3 << 6)) == 0) {
							op_mr();
						}
						break;
					}
				} else {
					switch (prefix_string ? prefix_string == 0xf2 ? 1 : 2 : prefix_operand_size ? 3 : 0) {
					case 0:
						if (!((~modrm & (3 << 6)) == 0)) {
							if (!(~modrm & (3 << 6))) return false;
							op_mr();
						}
						break;
					case 3:
						if (prefix_string) prefix_string = 0;
						else prefix_operand_size = 0, operand_size = 32;
						if (!((~modrm & (3 << 6)) == 0)) {
							if (!(~modrm & (3 << 6))) return false;
							op_mr();
						}
						break;
					case 1: case 2:
						if (prefix_string) prefix_string = 0;
						else prefix_operand_size = 0, operand_size = 32;
						if (!((~modrm & (3 << 6)) == 0)) {
							op_mr();
						}
						break;
					}
				}
				break;
			case 32: case 33:
				if ((~(modrm = *p++) & (3 << 6))) return false;
				op_mr();
				break;
			case 166: case 167:
				modrm = *p++;
				break;
			case 58:
				opcode3 = *p;
				switch (*p++) {
				case 33:
					if ((prefix_string ? prefix_string == 0xf2 ? 1 : 2 : prefix_operand_size ? 3 : 0) == 3) {
						if (prefix_string) prefix_string = 0;
						else prefix_operand_size = 0, operand_size = 32;
						(modrm = *p++);
						if (!(~modrm & (3 << 6))) return false;
						op_mr();
						op_imm_size[2] = 8, op_imm[2] = (int8_t)*p++;
					}
					break;
				case 32:
					if ((prefix_string ? prefix_string == 0xf2 ? 1 : 2 : prefix_operand_size ? 3 : 0) == 3) {
						if (prefix_string) prefix_string = 0;
						else prefix_operand_size = 0, operand_size = 32;
						(modrm = *p++);
						op_imm_size[2] = 8, op_imm[2] = (int8_t)*p++;
					}
					break;
				case 8: case 9: case 10: case 11: case 12: case 13: case 14: case 64: case 65: case 66: case 68: case 96: case 97: case 98: case 99: case 223:
					if ((prefix_string ? prefix_string == 0xf2 ? 1 : 2 : prefix_operand_size ? 3 : 0) == 3) {
						if (prefix_string) prefix_string = 0;
						else prefix_operand_size = 0, operand_size = 32;
						(modrm = *p++);
						op_mr();
						op_imm_size[2] = 8, op_imm[2] = (int8_t)*p++;
					}
					break;
				case 22: case 34:
					if ((prefix_string ? prefix_string == 0xf2 ? 1 : 2 : prefix_operand_size ? 3 : 0) == 3) {
						if (prefix_string) prefix_string = 0;
						else prefix_operand_size = 0, operand_size = 32;
						if (prefix_operand_size == 32) {
							(modrm = *p++);
							op_mr();
							op_imm_size[2] = 8, op_imm[2] = (int8_t)*p++;
						} else {
							(modrm = *p++);
							op_mr();
							op_imm_size[2] = 8, op_imm[2] = (int8_t)*p++;
						}
					}
					break;
				case 20: case 21: case 23:
					if ((prefix_string ? prefix_string == 0xf2 ? 1 : 2 : prefix_operand_size ? 3 : 0) == 3) {
						if (prefix_string) prefix_string = 0;
						else prefix_operand_size = 0, operand_size = 32;
						op_imm_size[2] = 8, op_imm[2] = (int8_t)*p++;
					}
					break;
				case 15:
					switch (prefix_string ? prefix_string == 0xf2 ? 1 : 2 : prefix_operand_size ? 3 : 0) {
					case 0: case 1: case 2:
						(modrm = *p++);
						op_mr();
						op_imm_size[2] = 8, op_imm[2] = (int8_t)*p++;
						break;
					case 3:
						if (prefix_string) prefix_string = 0;
						else prefix_operand_size = 0, operand_size = 32;
						(modrm = *p++);
						op_mr();
						op_imm_size[2] = 8, op_imm[2] = (int8_t)*p++;
						break;
					}
					break;
				}
				break;
			case 56:
				opcode3 = *p;
				switch (*p++) {
				case 42:
					if ((prefix_string ? prefix_string == 0xf2 ? 1 : 2 : prefix_operand_size ? 3 : 0) == 3) {
						if (prefix_string) prefix_string = 0;
						else prefix_operand_size = 0, operand_size = 32;
						(modrm = *p++);
						if (!(~modrm & (3 << 6))) return false;
						op_mr();
					}
					break;
				case 16: case 20: case 21: case 23: case 32: case 33: case 34: case 35: case 36: case 37: case 40: case 41: case 43: case 48: case 49: case 50: case 51: case 52: case 53: case 55: case 56: case 57: case 58: case 59: case 60: case 61: case 62: case 63: case 64: case 65: case 219: case 220: case 221: case 222: case 223:
					if ((prefix_string ? prefix_string == 0xf2 ? 1 : 2 : prefix_operand_size ? 3 : 0) == 3) {
						if (prefix_string) prefix_string = 0;
						else prefix_operand_size = 0, operand_size = 32;
						(modrm = *p++);
						op_mr();
					}
					break;
				case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8: case 9: case 10: case 11: case 28: case 29: case 30:
					switch (prefix_string ? prefix_string == 0xf2 ? 1 : 2 : prefix_operand_size ? 3 : 0) {
					case 0: case 1: case 2:
						(modrm = *p++);
						op_mr();
						break;
					case 3:
						if (prefix_string) prefix_string = 0;
						else prefix_operand_size = 0, operand_size = 32;
						(modrm = *p++);
						op_mr();
						break;
					}
					break;
				case 240:
					switch (prefix_string ? prefix_string == 0xf2 ? 1 : 2 : prefix_operand_size ? 3 : 0) {
					case 0: case 2: case 3:
						(modrm = *p++);
						if (!(~modrm & (3 << 6))) return false;
						op_mr();
						break;
					case 1:
						if (prefix_string) prefix_string = 0;
						else prefix_operand_size = 0, operand_size = 32;
						(modrm = *p++);
						op_mr();
						break;
					}
					break;
				case 241:
					switch (prefix_string ? prefix_string == 0xf2 ? 1 : 2 : prefix_operand_size ? 3 : 0) {
					case 0: case 2: case 3:
						if (!(~(modrm = *p++) & (3 << 6))) return false;
						op_mr();
						break;
					case 1:
						if (prefix_string) prefix_string = 0;
						else prefix_operand_size = 0, operand_size = 32;
						(modrm = *p++);
						op_mr();
						break;
					}
					break;
				}
				break;
			case 128: case 129: case 130: case 131: case 132: case 133: case 134: case 135: case 136: case 137: case 138: case 139: case 140: case 141: case 142: case 143:
				operand_size == 16 ? op_imm_size[0] = 16, op_imm[0] = (uint16_t)p[0] | ((int16_t)(int8_t)p[1] << 8), p += 2 : (op_imm_size[0] = 32, op_imm[0] = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((int32_t)(int8_t)p[3] << 24), p += 4);
				break;
			case 15:
				return false;
				break;
			case 24:
				switch ((modrm = *p++) >> 3 & 7) {
				case 0: case 1: case 2: case 3:
					if (!(~modrm & (3 << 6))) return false;
					op_mr();
					break;
				}
				break;
			case 0:
				switch ((modrm = *p++) >> 3 & 7) {
				case 2: case 3: case 4: case 5:
					op_mr();
					break;
				}
				break;
			case 113: case 114:
				switch ((modrm = *p++) >> 3 & 7) {
				case 2: case 4: case 6:
					switch (prefix_string ? prefix_string == 0xf2 ? 1 : 2 : prefix_operand_size ? 3 : 0) {
					case 0: case 1: case 2:
						if ((~modrm & (3 << 6))) return false;
						op_mr();
						op_imm_size[1] = 8, op_imm[1] = (int8_t)*p++;
						break;
					case 3:
						if (prefix_string) prefix_string = 0;
						else prefix_operand_size = 0, operand_size = 32;
						if ((~modrm & (3 << 6))) return false;
						op_mr();
						op_imm_size[1] = 8, op_imm[1] = (int8_t)*p++;
						break;
					}
					break;
				}
				break;
			case 115:
				switch ((modrm = *p++) >> 3 & 7) {
				case 3: case 7:
					if ((prefix_string ? prefix_string == 0xf2 ? 1 : 2 : prefix_operand_size ? 3 : 0) == 3) {
						if (prefix_string) prefix_string = 0;
						else prefix_operand_size = 0, operand_size = 32;
						if ((~modrm & (3 << 6))) return false;
						op_mr();
						op_imm_size[1] = 8, op_imm[1] = (int8_t)*p++;
					}
					break;
				case 2: case 6:
					switch (prefix_string ? prefix_string == 0xf2 ? 1 : 2 : prefix_operand_size ? 3 : 0) {
					case 0: case 1: case 2:
						if ((~modrm & (3 << 6))) return false;
						op_mr();
						op_imm_size[1] = 8, op_imm[1] = (int8_t)*p++;
						break;
					case 3:
						if (prefix_string) prefix_string = 0;
						else prefix_operand_size = 0, operand_size = 32;
						if ((~modrm & (3 << 6))) return false;
						op_mr();
						op_imm_size[1] = 8, op_imm[1] = (int8_t)*p++;
						break;
					}
					break;
				}
				break;
			case 186:
				switch ((modrm = *p++) >> 3 & 7) {
				case 4: case 5: case 6: case 7:
					op_mr();
					op_imm_size[1] = 8, op_imm[1] = (int8_t)*p++;
					break;
				}
				break;
			case 199:
				switch ((modrm = *p++) >> 3 & 7) {
				case 7:
					if (!(~modrm & (3 << 6))) return false;
					op_mr();
					break;
				case 1:
					if (prefix_operand_size == 32) {
						if (!(~modrm & (3 << 6))) return false;
						op_mr();
					} else {
						if (!(~modrm & (3 << 6))) return false;
						op_mr();
					}
					break;
				case 6:
					switch (prefix_string ? prefix_string == 0xf2 ? 1 : 2 : prefix_operand_size ? 3 : 0) {
					case 0: case 1:
						if (!(~modrm & (3 << 6))) return false;
						op_mr();
						break;
					case 2: case 3:
						if (prefix_string) prefix_string = 0;
						else prefix_operand_size = 0, operand_size = 32;
						if (!(~modrm & (3 << 6))) return false;
						op_mr();
						break;
					}
					break;
				}
				break;
			case 16: case 17: case 42: case 44: case 45: case 81: case 88: case 89: case 90: case 92: case 93: case 94: case 95:
				switch (prefix_string ? prefix_string == 0xf2 ? 1 : 2 : prefix_operand_size ? 3 : 0) {
				case 0:
					(modrm = *p++);
					op_mr();
					break;
				case 1: case 2: case 3:
					if (prefix_string) prefix_string = 0;
					else prefix_operand_size = 0, operand_size = 32;
					(modrm = *p++);
					op_mr();
					break;
				}
				break;
			case 112: case 194:
				switch (prefix_string ? prefix_string == 0xf2 ? 1 : 2 : prefix_operand_size ? 3 : 0) {
				case 0:
					(modrm = *p++);
					op_mr();
					op_imm_size[2] = 8, op_imm[2] = (int8_t)*p++;
					break;
				case 1: case 2: case 3:
					if (prefix_string) prefix_string = 0;
					else prefix_operand_size = 0, operand_size = 32;
					(modrm = *p++);
					op_mr();
					op_imm_size[2] = 8, op_imm[2] = (int8_t)*p++;
					break;
				}
				break;
			case 91: case 111: case 126: case 127:
				switch (prefix_string ? prefix_string == 0xf2 ? 1 : 2 : prefix_operand_size ? 3 : 0) {
				case 0: case 1:
					(modrm = *p++);
					op_mr();
					break;
				case 2: case 3:
					if (prefix_string) prefix_string = 0;
					else prefix_operand_size = 0, operand_size = 32;
					(modrm = *p++);
					op_mr();
					break;
				}
				break;
			case 80: case 215:
				switch (prefix_string ? prefix_string == 0xf2 ? 1 : 2 : prefix_operand_size ? 3 : 0) {
				case 0: case 1: case 2:
					(modrm = *p++);
					if ((~modrm & (3 << 6))) return false;
					op_mr();
					break;
				case 3:
					if (prefix_string) prefix_string = 0;
					else prefix_operand_size = 0, operand_size = 32;
					(modrm = *p++);
					if ((~modrm & (3 << 6))) return false;
					op_mr();
					break;
				}
				break;
			case 197:
				switch (prefix_string ? prefix_string == 0xf2 ? 1 : 2 : prefix_operand_size ? 3 : 0) {
				case 0: case 1: case 2:
					(modrm = *p++);
					if ((~modrm & (3 << 6))) return false;
					op_mr();
					op_imm_size[2] = 8, op_imm[2] = (int8_t)*p++;
					break;
				case 3:
					if (prefix_string) prefix_string = 0;
					else prefix_operand_size = 0, operand_size = 32;
					(modrm = *p++);
					if ((~modrm & (3 << 6))) return false;
					op_mr();
					op_imm_size[2] = 8, op_imm[2] = (int8_t)*p++;
					break;
				}
				break;
			case 196:
				switch (prefix_string ? prefix_string == 0xf2 ? 1 : 2 : prefix_operand_size ? 3 : 0) {
				case 0: case 1: case 2:
					(modrm = *p++);
					op_imm_size[2] = 8, op_imm[2] = (int8_t)*p++;
					break;
				case 3:
					if (prefix_string) prefix_string = 0;
					else prefix_operand_size = 0, operand_size = 32;
					(modrm = *p++);
					op_imm_size[2] = 8, op_imm[2] = (int8_t)*p++;
					break;
				}
				break;
			case 20: case 21: case 40: case 41: case 46: case 47: case 84: case 85: case 86: case 87: case 96: case 97: case 98: case 99: case 100: case 101: case 102: case 103: case 104: case 105: case 106: case 107: case 110: case 116: case 117: case 118: case 209: case 210: case 211: case 212: case 213: case 216: case 217: case 218: case 219: case 220: case 221: case 222: case 223: case 224: case 225: case 226: case 227: case 228: case 229: case 232: case 233: case 234: case 235: case 236: case 237: case 238: case 239: case 241: case 242: case 243: case 244: case 245: case 246: case 248: case 249: case 250: case 251: case 252: case 253: case 254:
				switch (prefix_string ? prefix_string == 0xf2 ? 1 : 2 : prefix_operand_size ? 3 : 0) {
				case 0: case 1: case 2:
					(modrm = *p++);
					op_mr();
					break;
				case 3:
					if (prefix_string) prefix_string = 0;
					else prefix_operand_size = 0, operand_size = 32;
					(modrm = *p++);
					op_mr();
					break;
				}
				break;
			case 198:
				switch (prefix_string ? prefix_string == 0xf2 ? 1 : 2 : prefix_operand_size ? 3 : 0) {
				case 0: case 1: case 2:
					(modrm = *p++);
					op_mr();
					op_imm_size[2] = 8, op_imm[2] = (int8_t)*p++;
					break;
				case 3:
					if (prefix_string) prefix_string = 0;
					else prefix_operand_size = 0, operand_size = 32;
					(modrm = *p++);
					op_mr();
					op_imm_size[2] = 8, op_imm[2] = (int8_t)*p++;
					break;
				}
				break;
			case 19: case 23: case 43: case 231:
				switch (prefix_string ? prefix_string == 0xf2 ? 1 : 2 : prefix_operand_size ? 3 : 0) {
				case 0: case 1: case 2:
					if (!(~(modrm = *p++) & (3 << 6))) return false;
					op_mr();
					break;
				case 3:
					if (prefix_string) prefix_string = 0;
					else prefix_operand_size = 0, operand_size = 32;
					if (!(~(modrm = *p++) & (3 << 6))) return false;
					op_mr();
					break;
				}
				break;
			case 82: case 83:
				switch (prefix_string ? prefix_string == 0xf2 ? 1 : 2 : prefix_operand_size ? 3 : 0) {
				case 0: case 1: case 3:
					(modrm = *p++);
					op_mr();
					break;
				case 2:
					if (prefix_string) prefix_string = 0;
					else prefix_operand_size = 0, operand_size = 32;
					(modrm = *p++);
					op_mr();
					break;
				}
				break;
			case 214:
				switch (prefix_string ? prefix_string == 0xf2 ? 1 : 2 : prefix_operand_size ? 3 : 0) {
				case 1: case 2:
					if (prefix_string) prefix_string = 0;
					else prefix_operand_size = 0, operand_size = 32;
					(modrm = *p++);
					if ((~modrm & (3 << 6))) return false;
					op_mr();
					break;
				case 3:
					if (prefix_string) prefix_string = 0;
					else prefix_operand_size = 0, operand_size = 32;
					(modrm = *p++);
					op_mr();
					break;
				}
				break;
			case 230:
				switch (prefix_string ? prefix_string == 0xf2 ? 1 : 2 : prefix_operand_size ? 3 : 0) {
				case 1: case 2: case 3:
					if (prefix_string) prefix_string = 0;
					else prefix_operand_size = 0, operand_size = 32;
					(modrm = *p++);
					op_mr();
					break;
				}
				break;
			case 124: case 125: case 208:
				switch (prefix_string ? prefix_string == 0xf2 ? 1 : 2 : prefix_operand_size ? 3 : 0) {
				case 1: case 3:
					if (prefix_string) prefix_string = 0;
					else prefix_operand_size = 0, operand_size = 32;
					(modrm = *p++);
					op_mr();
					break;
				}
				break;
			}
			break;
		case 104: case 232: case 233:
			operand_size == 16 ? op_imm_size[0] = 16, op_imm[0] = (uint16_t)p[0] | ((int16_t)(int8_t)p[1] << 8), p += 2 : (op_imm_size[0] = 32, op_imm[0] = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((int32_t)(int8_t)p[3] << 24), p += 4);
			break;
		case 5: case 13: case 21: case 29: case 37: case 45: case 53: case 61: case 169: case 184: case 185: case 186: case 187: case 188: case 189: case 190: case 191:
			operand_size == 16 ? op_imm_size[1] = 16, op_imm[1] = (uint16_t)p[0] | ((int16_t)(int8_t)p[1] << 8), p += 2 : (op_imm_size[1] = 32, op_imm[1] = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((int32_t)(int8_t)p[3] << 24), p += 4);
			break;
		case 154: case 234:
			p += operand_size == 16 ? 4 : 6;
			break;
		case 254:
			switch ((modrm = *p++) >> 3 & 7) {
			case 0: case 1:
				op_mr();
				break;
			}
			break;
		case 246:
			switch ((modrm = *p++) >> 3 & 7) {
			case 2: case 3: case 4: case 5: case 6: case 7:
				op_mr();
				break;
			case 0: case 1:
				op_mr();
				op_imm_size[1] = 8, op_imm[1] = (int8_t)*p++;
				break;
			}
			break;
		case 247:
			switch ((modrm = *p++) >> 3 & 7) {
			case 2: case 3: case 4: case 5: case 6: case 7:
				op_mr();
				break;
			case 0: case 1:
				op_mr();
				operand_size == 16 ? op_imm_size[1] = 16, op_imm[1] = (uint16_t)p[0] | ((int16_t)(int8_t)p[1] << 8), p += 2 : (op_imm_size[1] = 32, op_imm[1] = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((int32_t)(int8_t)p[3] << 24), p += 4);
				break;
			}
			break;
		case 255:
			switch ((modrm = *p++) >> 3 & 7) {
			case 3: case 5:
				if (!(~modrm & (3 << 6))) return false;
				op_mr();
				break;
			case 0: case 1: case 2: case 4: case 6:
				op_mr();
				break;
			}
			break;
		}
		insn_size = p - begin;
		return true;
	}
};
