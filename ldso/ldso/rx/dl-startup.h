/*
 * Architecture specific code used by dl-startup.c
 * Copyright (C) 2022 Yoshinori Sato
 *
 * Licensed under the LGPL v2.1, see the file COPYING.LIB in this tarball.
 */

#include <features.h>

#if defined(__FDPIC__)
__asm__(
	"	.text\n"
	"	.globl  _start\n"
	"	.type   _start,%function\n"
	"	.align 2\n"
	"_start:\n"
	/* We compute the parameters for __self_reloc:
	   - r1 is a pointer to the loadmap (either from r12 or r11 if rtld is
	   lauched in standalone mode)
	   - r2 is a pointer to the start of .rofixup section
	   - r3 is a pointer to the last word of .rofixup section

	   __self_reloc will fix indirect addresses in .rofixup
	   section and will return the relocated GOT value.
	*/
	"1:		      \n"
	"	mvfc	pc, r4\n"
	"	add	#__ROFIXUP_LIST__ - 1b, r4, r2\n"
	"	add	#__ROFIXUP_END__ - 1b, r4, r3\n"
	"	mov	r11, r1\n"
	"	tst	r12, r12\n"
	"	beq	2f\n"
	"	mov	r12, r1\n"
	"2:		       \n"
	"	pushm	r11-r14\n"
	"	bsr	__self_reloc\n"
	"	popm	r11-r14\n"
	/* We compute the parameters for dl_start(). See DL_START()
	   macro below.  The address of the user entry point is
	   returned in dl_main_funcdesc (on stack).  */
	"	mov	r11, r2\n"
	"	mov	r12, r3\n"
	"	mov	r13, r4\n"
	"	mov	r1, r13\n"
	"	add	#8, r0, r5\n"
	"	add	#-24, r0\n"
	"	mov.l	r5, 4[r0]\n"
	"	add	#-16, r5\n"
	"	mov.l	r5, [r0]\n"
	"	bsr	_dl_start\n"
	/* Now compute parameters for entry point according to FDPIC ABI.  */
	"	add	#_dl_fini@GOTOFFFUNCDESC, r13, r14\n"
	"	mov	20[r0], r13\n"
	"	mov	16[r0], r5\n"
	"	add 	#24, r0\n"
	"	jmp	r5\n"
	".loopforever:\n"
	"	bra	.loopforever\n"
	"	.size	_start,.-_start\n"
	"	.previous\n"
);
#endif

/* Get a pointer to the argv array.  On many platforms this can be just
 * the address of the first argument, on other platforms we need to
 * do something a little more subtle here.  */
#define GET_ARGV(ARGVP, ARGS) ARGVP = (((unsigned long *)ARGS) + 1)

/* Handle relocation of the symbols in the dynamic loader. */
static /*__always_inline*/
void PERFORM_BOOTSTRAP_RELOC(ELF_RELOC *rpnt, unsigned long *reloc_addr,
	unsigned long symbol_addr, DL_LOADADDR_TYPE load_addr, Elf32_Sym *symtab)
{
	switch (ELF_R_TYPE(rpnt->r_info)) {
		case R_RX_NONE:
			break;
		case R_RX_GLOB_DAT:
		case R_RX_JMP_SLOT:
			if (symtab)
				*reloc_addr = symbol_addr;
			else
				*reloc_addr = DL_RELOC_ADDR(load_addr, rpnt->r_addend);
			break;
		case R_RX_RELATIVE:
			*reloc_addr = DL_RELOC_ADDR(load_addr, rpnt->r_addend);
			break;
#ifdef __FDPIC__
		case R_RX_FUNCDESC_VALUE:
			{
				struct funcdesc_value *dst = (struct funcdesc_value *) reloc_addr;

				dst->entry_point += symbol_addr;
				dst->got_value = load_addr.got_value;
			}
			break;
#endif
		default:
			SEND_STDERR("Unsupported relocation type\n");
			_dl_exit(1);
	}
}
#ifdef __FDPIC__
#undef DL_START
#define DL_START(X)   \
static void  __attribute__ ((used)) \
_dl_start (Elf32_Addr dl_boot_got_pointer, \
          struct elf32_fdpic_loadmap *dl_boot_progmap, \
          struct elf32_fdpic_loadmap *dl_boot_ldsomap, \
          Elf32_Dyn *dl_boot_ldso_dyn_pointer, \
          struct funcdesc_value *dl_main_funcdesc, \
          X)

/*
 * Transfer control to the user's application, once the dynamic loader
 * is done.  We return the address of the function's entry point to
 * _dl_boot, see boot1_arch.h.
 */
#define START()	do {							\
  struct elf_resolve *exec_mod = _dl_loaded_modules;			\
  dl_main_funcdesc->entry_point = _dl_elf_main;				\
  while (exec_mod->libtype != elf_executable)				\
    exec_mod = exec_mod->next;						\
  dl_main_funcdesc->got_value = exec_mod->loadaddr.got_value;		\
  return;								\
} while (0)

/* We use __aeabi_idiv0 in _dl_find_hash, so we need to have the raise
   symbol.  */
int raise(int sig)
{
  _dl_exit(1);
}
#endif /* __FDPIC__ */
