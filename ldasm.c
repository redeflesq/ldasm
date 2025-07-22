#include "ldasm.h"
#include "rle.h"

#include <memory.h>

// Instruction format:
// | prefix | REX | opcode | modR/M | SIB | disp8/16/32 | imm8/16/32/64 |

#define OP_NONE             0x00
#define OP_INVALID          0x80

#define OP_DATA_I8          0x01
#define OP_DATA_I16         0x02
#define OP_DATA_I16_I32     0x04
#define OP_DATA_I16_I32_I64 0x08
#define OP_EXTENDED         0x10
#define OP_RELATIVE         0x20
#define OP_MODRM            0x40
#define OP_PREFIX           0x80

static bool decompress_lookup_table(uint8_t* out, size_t size)
{
	if (!out || size != 256)
		return false;

	size_t lookup_table_len = 144;

	uint32_t lookup_table[] = {
		0x01400499, 0x04000204, 0x02040140, 0x40049900, 0x00020401, 0x04014004, 0x04210002, 0x80040140,
		0x01400400, 0x00808404, 0x04014004, 0x40040080, 0x80040138, 0x40020023, 0x44048004, 0x044101CC,
		0x41211000, 0x0C410244, 0x000A8540, 0x01000506, 0x04080108, 0x04013C00, 0x01080006, 0x41020808,
		0x02410002, 0x03444140, 0x00020200, 0x0002FF01, 0x01024004, 0x40080002, 0x01042104, 0x06E42402,
		0x80000421, 0x02800200, 0x03400200, 0x40020006,
	};

	decompress_rle(out, lookup_table, lookup_table_len);

	return true;
}

static bool decompress_lookup_table_ex(uint8_t* out, size_t size)
{
	if (!out || size != 256)
		return false;

	size_t lookup_table_ex_len = 83;

	uint32_t lookup_table_ex[] = {
		0x80400405, 0x00800005, 0x16804080, 0x06400941, 0x40040080, 0x06408050, 0x06400880, 0x50008000,
		0x05EF5180, 0x04403080, 0x00400341, 0x80024002, 0x10C74004, 0x03401024, 0x40414000, 0x00038002,
		0x0D414094, 0x40074140, 0x41034041, 0x00084006, 0x0080402F,
	};

	decompress_rle(out, lookup_table_ex, lookup_table_ex_len);

	return true;
}

bool ldasm_init(ldasm_tables* tables)
{
	if (!tables)
		return false;

	if (!decompress_lookup_table(tables->flags, 256))
		return false;

	if (!decompress_lookup_table_ex(tables->flags_ex, 256))
		return false;

	return true;
}

size_t ldasm(const void* code, const ldasm_tables* tables, ldasm_insn* ld, bool is64)
{
	if (!code || !tables || !ld)
		return 0;

	uint8_t* p = (uint8_t*)code;
	uint8_t s, op, f;
	uint8_t rexw, pr_66, pr_67;

	s = rexw = pr_66 = pr_67 = 0;

	/* init output data */
	memset(ld, 0, sizeof(ldasm_insn));

	/* phase 1: parse prefixies */
	while (tables->flags[*p] & OP_PREFIX) {
		if (*p == 0x66) pr_66 = 1u;
		if (*p == 0x67) pr_67 = 1u;
		++p; ++s;
		ld->flags |= DF_PREFIX;
		if (s == 15u) {
			ld->flags |= DF_INVALID;
			return s;
		} //if
	}

	if (is64) {
		/* parse REX prefix */
		if (*p >> 4u == 4u) {
			ld->rex = *p;
			rexw = (ld->rex >> 3u) & 1u;
			ld->flags |= DF_REX;
			++p; ++s;
		} //if

		/* can be only one REX prefix */
		if (*p >> 4 == 4) {
			ld->flags |= DF_INVALID;
			return ++s;
		} //if
	}

	/* phase 2: parse opcode */
	ld->opcd_offset = (uint8_t)(p - (uint8_t*)code);
	ld->opcd_size = 1;
	op = *p++; ++s;

	/* is 2 byte opcode? */
	if (op == 0x0F) {
		op = *p++; ++s;
		++ld->opcd_size;
		f = tables->flags_ex[op];
		if (f & OP_INVALID) {
			ld->flags |= DF_INVALID;
			return s;
		} //if
		/* for SSE instructions */
		if (f & OP_EXTENDED) {
			op = *p++; ++s;
			++ld->opcd_size;
		} //if
	}
	else {
		f = tables->flags[op];
		/* pr_66 = pr_67 for opcodes A0-A3 */
		if (op >= 0xA0 && op <= 0xA3) pr_66 = pr_67;
	} //if

	/* phase 3: parse ModR/M, SIB and DISP */
	if (f & OP_MODRM) {
		uint8_t mod = (*p >> 6);
		uint8_t ro = (*p & 0x38) >> 3;
		uint8_t rm = (*p & 7);

		ld->modrm = *p++; ++s;
		ld->flags |= DF_MODRM;

		/* in F6,F7 opcodes immediate data present if R/O == 0 */
		if (op == 0xF6 && (ro == 0 || ro == 1))
			f |= OP_DATA_I8;
		if (op == 0xF7 && (ro == 0 || ro == 1))
			f |= OP_DATA_I16_I32_I64;


		/* is SIB byte exist? */
		if (mod != 3 && rm == 4 && (is64 || !pr_67)) {
			ld->sib = *p++; ++s;
			ld->flags |= DF_SIB;

			/* if base == 5 and mod == 0 */
			if ((ld->sib & 7) == 5 && mod == 0) {
				ld->disp_size = 4;
			} //if
		} //if

		switch (mod) {
		case 0u:
			if (is64) {
				if (rm == 5u) {
					ld->disp_size = 4u;
					ld->flags |= DF_RELATIVE;
				} //if
			}
			else if (pr_67) {
				if (rm == 6u) ld->disp_size = 2u;
			}
			else {
				if (rm == 5u) ld->disp_size = 4u;
			} //if
			break;
		case 1u:
			ld->disp_size = 1u;
			break;
		case 2u:
			if (is64)
				ld->disp_size = 4u;
			else if (pr_67)
				ld->disp_size = 2u;
			else
				ld->disp_size = 4u;
			break;
		}

		if (ld->disp_size) {
			ld->disp_offset = (uint8_t)(p - (uint8_t*)code);
			p += ld->disp_size;
			s += ld->disp_size;
			ld->flags |= DF_DISP;
		} //if
	}

	/* phase 4: parse immediate data */

	// error..  48 F7 C6 01 00 00 00 parsed incorrect
	//
	//if ((rexw || (is64 && op >= 0xA0u && op <= 0xA3u)) && f & OP_DATA_I16_I32_I64)
	//    ld->imm_size = 8u;
	//else if (f & OP_DATA_I16_I32 || f & OP_DATA_I16_I32_I64)
	//    ld->imm_size = 4u - (pr_66 << 1u);


	if ((is64 && rexw && (op >= 0xB8 && op <= 0xBF)) && (f & OP_DATA_I16_I32_I64)) {
		ld->imm_size = 8u;
	}
	else if (f & OP_DATA_I16_I32_I64) {
		ld->imm_size = 4u - (pr_66 << 1u);
	}

	/* if exist, add OP_DATA_I16 and OP_DATA_I8 size */
	ld->imm_size += f & 3u;

	if (ld->imm_size) {
		s += ld->imm_size;
		ld->imm_offset = (uint8_t)(p - (uint8_t*)code);
		ld->flags |= DF_IMM;
		if (f & OP_RELATIVE)
			ld->flags |= DF_RELATIVE;
	} //if

	/* instruction is too long */
	if (s > 15u) ld->flags |= DF_INVALID;

	return s;
}

// from https://github.com/DarthTon/Blackbone/blob/master/src/BlackBone/Asm/LDasm.c#L775
size_t ldasm_size_of_proc(void* proc, const ldasm_tables* tables, bool is64)
{
	uint32_t  length;
	uint8_t* opcode;
	uint32_t  result = 0;
	ldasm_insn data = { 0 };

	do {
		length = ldasm((uint8_t*)proc, tables, &data, is64);

		opcode = (uint8_t*)proc + data.opcd_offset;
		result += length;

		if ((length == 1) && (*opcode == 0xCC))
			break;

		if ((length == 1) && (*opcode == 0xC3))
			break;

		if ((length == 3) && (*opcode == 0xC2))
			break;

		proc = (void*)((size_t)proc + length);

	} while (length);

	return result;
}

// from https://github.com/DarthTon/Blackbone/blob/master/src/BlackBone/Asm/LDasm.c#L806
void* ldasm_resolve_jmp(void* proc, const ldasm_tables* tables, bool is64)
{
	uint32_t  length;
	uint8_t* opcode;
	ldasm_insn data = { 0 };

	length = ldasm((uint8_t*)proc, tables, &data, is64);
	opcode = (uint8_t*)proc + data.opcd_offset;

	// Recursive unwind
	if (length == 5 && data.opcd_size == 1 && *opcode == 0xE9) {
		uint32_t delta = *(int32_t*)((uintptr_t)proc + data.opcd_size);
		return ldasm_resolve_jmp((void*)((uintptr_t)proc + delta + length), tables, is64);
	}

	return proc;
}