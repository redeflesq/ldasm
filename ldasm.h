#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct _ldasm_tables
{
	uint8_t flags[256];
	uint8_t flags_ex[256];
} ldasm_tables;

typedef struct _ldasm_insn
{
	uint8_t  imm_offset;
	uint8_t  imm_size;
	uint8_t  disp_offset;
	uint8_t  disp_size;
	uint8_t  opcd_offset;
	uint8_t  opcd_size;
	uint8_t  rex;
	uint8_t  flags;
	uint8_t  modrm;
	uint8_t  sib;
} ldasm_insn;

enum ldasm_flags
{
	DF_INVALID = 1 << 0,
	DF_PREFIX = 1 << 1,
	DF_REX = 1 << 2,
	DF_MODRM = 1 << 3,
	DF_SIB = 1 << 4,
	DF_DISP = 1 << 5,
	DF_IMM = 1 << 6,
	DF_RELATIVE = 1 << 7
};

/**
 * @brief Initialize disassembler tables
 */
bool ldasm_init(ldasm_tables* tables);

/**
 * @brief Disassemble one instruction from code buffer
 */
size_t ldasm(const void* code, const ldasm_tables* tables, ldasm_insn* ld, bool is64);

/**
 * @brief Calculate size of a procedure
 */
size_t ldasm_size_of_proc(void* proc, const ldasm_tables* tables, bool is64);

/** 
 * @brief Resolve the final jump target by recursively following relative jumps
 */
void* ldasm_resolve_jmp(void* proc, const ldasm_tables* tables, bool is64);