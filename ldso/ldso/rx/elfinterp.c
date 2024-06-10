/* RX ELF shared library loader suppport
 *
 * Copyright (C) 2022 Yoshinori Sato
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the above contributors may not be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* Program to load an ELF binary on a linux system, and run it.
   References to symbols in sharable libraries can be resolved by either
   an ELF sharable library or a linux style of shared library. */

#include "ldso.h"

extern int _dl_linux_resolve(void);

unsigned long _dl_linux_resolver (struct elf_resolve *tpnt, int reloc_offet)
{
	ELF_RELOC *this_reloc;
	char *strtab;
	ElfW(Sym) *symtab;
	int symtab_index;
	char *rel_addr;
	char *new_addr;
	struct funcdesc_value funcval;
	struct funcdesc_value volatile *got_entry;
	char *symname;
	struct symbol_ref sym_ref;

	rel_addr = (char *)tpnt->dynamic_info[DT_JMPREL];

	this_reloc = (ELF_RELOC *)(intptr_t)(rel_addr + reloc_offet);
	symtab_index = ELF_R_SYM(this_reloc->r_info);

	symtab = (ElfW(Sym) *) tpnt->dynamic_info[DT_SYMTAB];
	strtab = (char *) tpnt->dynamic_info[DT_STRTAB];
	sym_ref.sym = &symtab[symtab_index];
	sym_ref.tpnt = NULL;
	symname= strtab + symtab[symtab_index].st_name;

	/* Address of GOT entry fix up */
	got_entry = (struct funcdesc_value *) DL_RELOC_ADDR(tpnt->loadaddr, this_reloc->r_offset);

	/* Get the address to be used to fill in the GOT entry.  */
	new_addr = _dl_find_hash(symname, &_dl_loaded_modules->symbol_scope, NULL, 0, &sym_ref);
	if (!new_addr) {
		new_addr = _dl_find_hash(symname, NULL, NULL, 0, &sym_ref);
		if (!new_addr) {
			_dl_dprintf(2, "%s: can't resolve symbol '%s'\n", _dl_progname, symname);
			_dl_exit(1);
		}
	}

	funcval.entry_point = new_addr;
	funcval.got_value = sym_ref.tpnt->loadaddr.got_value;

#if defined (__SUPPORT_LD_DEBUG__)
	if (_dl_debug_bindings) {
		_dl_dprintf(_dl_debug_file, "\nresolve function: %s", symname);
		if (_dl_debug_detail)
			_dl_dprintf(_dl_debug_file,
				    "\n\tpatched (%x,%x) ==> (%x,%x) @ %x\n",
				    got_entry->entry_point, got_entry->got_value,
				    funcval.entry_point, funcval.got_value,
				    got_entry);
	}
	if (1 || !_dl_debug_nofixups) {
		*got_entry = funcval;
	}
#else
	*got_entry = funcval;
#endif

	return got_entry;
}

static int
_dl_parse(struct elf_resolve *tpnt, struct r_scope_elem *scope,
	  unsigned long rel_addr, unsigned long rel_size,
	  int (*reloc_fnc) (struct elf_resolve *tpnt, struct r_scope_elem *scope,
			    ELF_RELOC *rpnt, ElfW(Sym) *symtab, char *strtab))
{
	int i;
	char *strtab;
	int goof = 0;
	ElfW(Sym) *symtab;
	ELF_RELOC *rpnt;
	int symtab_index;

	/* Now parse the relocation information */
	rpnt = (ELF_RELOC *) rel_addr;
	rel_size = rel_size / sizeof(ELF_RELOC);

	symtab = (ElfW(Sym) *) tpnt->dynamic_info[DT_SYMTAB];
	strtab = (char *) tpnt->dynamic_info[DT_STRTAB];

	  for (i = 0; i < rel_size; i++, rpnt++) {
	        int res;

		symtab_index = ELF_R_SYM(rpnt->r_info);

		debug_sym(symtab,strtab,symtab_index);
		debug_reloc(symtab,strtab,rpnt);

		res = reloc_fnc (tpnt, scope, rpnt, symtab, strtab);

		if (res==0) continue;

		_dl_dprintf(2, "\n%s: ",_dl_progname);

		if (symtab_index)
		  _dl_dprintf(2, "symbol '%s': ", strtab + symtab[symtab_index].st_name);

		if (unlikely(res <0))
		{
		        int reloc_type = ELF_R_TYPE(rpnt->r_info);
			_dl_dprintf(2, "can't handle reloc type %x\n", reloc_type);
			_dl_exit(-res);
		}
		if (unlikely(res >0))
		{
			_dl_dprintf(2, "can't resolve symbol\n");
			goof += res;
		}
	  }
	  return goof;
}

static int
_dl_do_reloc (struct elf_resolve *tpnt,struct r_scope_elem *scope,
	      ELF_RELOC *rpnt, ElfW(Sym) *symtab, char *strtab)
{
	int reloc_type;
	int symtab_index;
	char *symname;
	unsigned long *reloc_addr;
	unsigned long symbol_addr;
	struct symbol_ref sym_ref;
	struct elf_resolve *def_mod = 0;
	int goof = 0;

	reloc_addr = (unsigned long *) DL_RELOC_ADDR(tpnt->loadaddr, rpnt->r_offset);

	reloc_type = ELF_R_TYPE(rpnt->r_info);
	symtab_index = ELF_R_SYM(rpnt->r_info);
	symbol_addr = 0;
	sym_ref.sym = &symtab[symtab_index];
	sym_ref.tpnt = NULL;
	symname = strtab + symtab[symtab_index].st_name;

	if (symtab_index) {
		if (ELF_ST_BIND (symtab[symtab_index].st_info) == STB_LOCAL) {
			symbol_addr = (unsigned long) DL_RELOC_ADDR(tpnt->loadaddr, symtab[symtab_index].st_value);
			def_mod = tpnt;
		} else {
			symbol_addr =  (unsigned long)_dl_find_hash(symname, scope, tpnt,
							elf_machine_type_class(reloc_type), &sym_ref);

			/*
			 * We want to allow undefined references to weak symbols - this might
			 * have been intentional.  We should not be linking local symbols
			 * here, so all bases should be covered.
			 */
			if (!symbol_addr && (ELF_ST_TYPE(symtab[symtab_index].st_info) != STT_TLS)
				&& (ELF_ST_BIND(symtab[symtab_index].st_info) != STB_WEAK)) {
				/* This may be non-fatal if called from dlopen.  */
				return 1;

			}
			if (_dl_trace_prelink) {
				_dl_debug_lookup (symname, tpnt, &symtab[symtab_index],
						&sym_ref, elf_machine_type_class(reloc_type));
			}
			def_mod = sym_ref.tpnt;
		}
	} else {
		/*
		 * Relocs against STN_UNDEF are usually treated as using a
		 * symbol value of zero, and using the module containing the
		 * reloc itself.
		 */
		symbol_addr = symtab[symtab_index].st_value;
		def_mod = tpnt;
	}

#if defined (__SUPPORT_LD_DEBUG__)
	{
		unsigned long old_val;

		if (reloc_type != R_RX_NONE)
			old_val = *reloc_addr;
#endif
		switch (reloc_type) {
			case R_RX_NONE:
				break;
			case R_RX_DIR32:
				*reloc_addr += symbol_addr;
				break;
			case R_RX_GLOB_DAT:
			case R_RX_JMP_SLOT:
				*reloc_addr = symbol_addr;
				break;
			case R_RX_RELATIVE:
				*reloc_addr = DL_RELOC_ADDR(tpnt->loadaddr, *reloc_addr);
				break;
#ifdef __FDPIC__
			case R_RX_FUNCDESC_VALUE:
				{
					struct funcdesc_value funcval;
					struct funcdesc_value *dst = (struct funcdesc_value *) reloc_addr;

					funcval.entry_point = (void*)symbol_addr;
					/* Add offset to section address for local symbols.  */
					if (ELF_ST_BIND(symtab[symtab_index].st_info) == STB_LOCAL)
					  funcval.entry_point += *reloc_addr;
					funcval.got_value = def_mod->loadaddr.got_value;
					*dst = funcval;
				}
				break;
			case R_RX_FUNCDESC:
				{
				  unsigned long reloc_value = *reloc_addr;

				  if (symbol_addr)
					reloc_value = (unsigned long) _dl_funcdesc_for(symbol_addr + reloc_value, sym_ref.tpnt->loadaddr.got_value);
				  else
					/* Relocation against an
					   undefined weak symbol:
					   set funcdesc to zero.  */
					reloc_value = 0;

				  *reloc_addr = reloc_value;
				}
				break;
#endif
			default:
				return -1; /*call _dl_exit(1) */
		}
#if defined (__SUPPORT_LD_DEBUG__)
		if (_dl_debug_reloc && _dl_debug_detail && reloc_type != R_RX_NONE)
			_dl_dprintf(_dl_debug_file, "\tpatch: %x ==> %x @ %x", old_val, *reloc_addr, reloc_addr);
	}

#endif

	return goof;
}

static int
_dl_do_lazy_reloc (struct elf_resolve *tpnt, struct r_scope_elem *scope,
		   ELF_RELOC *rpnt, ElfW(Sym) *symtab, char *strtab)
{
	int reloc_type;
	unsigned long *reloc_addr;

	reloc_addr = (unsigned long *) DL_RELOC_ADDR(tpnt->loadaddr, rpnt->r_offset);
	reloc_type = ELF_R_TYPE(rpnt->r_info);

#if defined (__SUPPORT_LD_DEBUG__)
	{
		unsigned long old_val;

		if (reloc_type != R_RX_NONE)
			old_val = *reloc_addr;
#endif
		switch (reloc_type) {
			case R_RX_NONE:
				break;

			case R_RX_JMP_SLOT:
				*reloc_addr = DL_RELOC_ADDR(tpnt->loadaddr, *reloc_addr);
				break;
#ifdef __FDPIC__
			case R_RX_FUNCDESC_VALUE:
				{
					struct funcdesc_value *dst = (struct funcdesc_value *) reloc_addr;

					dst->entry_point = DL_RELOC_ADDR(tpnt->loadaddr, dst->entry_point);
					dst->got_value = tpnt->loadaddr.got_value;
				}
				break;
#endif
			default:
				return -1; /*call _dl_exit(1) */
		}
#if defined (__SUPPORT_LD_DEBUG__)
		if (_dl_debug_reloc && _dl_debug_detail && reloc_type != R_ARM_NONE)
			_dl_dprintf(_dl_debug_file, "\tpatch: %x ==> %x @ %x", old_val, *reloc_addr, reloc_addr);
	}

#endif
	return 0;
}

void _dl_parse_lazy_relocation_information(struct dyn_elf *rpnt,
	unsigned long rel_addr, unsigned long rel_size)
{
	(void)_dl_parse(rpnt->dyn, NULL, rel_addr, rel_size, _dl_do_lazy_reloc);
}

int _dl_parse_relocation_information(struct dyn_elf *rpnt,
	struct r_scope_elem *scope, unsigned long rel_addr, unsigned long rel_size)
{
	return _dl_parse(rpnt->dyn, scope, rel_addr, rel_size, _dl_do_reloc);
}

#ifndef IS_IN_libdl
# include "../../libc/sysdeps/linux/rx/crtreloc.c"
#endif
