/*
 * Various assembly language/system dependent hacks that are required
 * so that we can minimize the amount of platform specific code.
 * Copyright (C) 2000-2004 by Erik Andersen <andersen@codepoet.org>
 */

#ifndef _ARCH_DL_SYSDEP
#define _ARCH_DL_SYSDEP

/* Define this if the system uses RELOCA.  */
#define ELF_USES_RELOCA
#include <elf.h>

#ifdef __FDPIC__
/* Need bootstrap relocations */
#define ARCH_NEEDS_BOOTSTRAP_RELOCS

#define DL_CHECK_LIB_TYPE(epnt, piclib, _dl_progname, libname) \
do \
{ \
  (piclib) = 2; \
} \
while (0)
#endif /* __FDPIC__ */

/* Here we define the magic numbers that this dynamic loader should accept */
#define MAGIC1 EM_RX
#undef  MAGIC2

/* Used for error messages */
#define ELF_TARGET "RX"

struct elf_resolve;
unsigned long _dl_linux_resolver(struct elf_resolve * tpnt, int reloc_entry);

/* ELF_RTYPE_CLASS_PLT iff TYPE describes relocation of a PLT entry or
   TLS variable, so undefined references should not be allowed to
   define the value.

   ELF_RTYPE_CLASS_NOCOPY iff TYPE should not be allowed to resolve to one
   of the main executable's symbols, as for a COPY reloc.  */

#define elf_machine_type_class(type)					\
  ((((type) == R_RX_JMP_SLOT || (type) == R_RX_FUNCDESC_VALUE ||	\
     (type) == R_RX_FUNCDESC || (type) == R_RX_ABS32)			\
    * ELF_RTYPE_CLASS_PLT)						\
   | (((type) == R_RX_COPY) * ELF_RTYPE_CLASS_COPY))

/* Return the link-time address of _DYNAMIC.  Conveniently, this is the
   first element of the GOT.  We used to use the PIC register to do this
   without a constant pool reference, but GCC 4.2 will use a pseudo-register
   for the PIC base, so it may not be in r10.  */
static __always_inline Elf32_Addr __attribute__ ((unused))
elf_machine_dynamic (void)
{
  Elf32_Addr dynamic;
  __asm__ ("mvfc pc,%0\n"
	   "add #_GLOBAL_OFFSET_TABLE_,%0"
	   : "=r" (dynamic));

  return dynamic;
}

extern char __dl_start[] __asm__("_dl_start");

#ifdef __FDPIC__
/* We must force strings used early in the bootstrap into the data
   segment.  */
#undef SEND_EARLY_STDERR
#define SEND_EARLY_STDERR(S) \
  do { /* FIXME: implement */; } while (0)

#include "../fdpic/dl-sysdep.h"
#endif /* __FDPIC__ */

/* Return the run-time load address of the shared object.  */
static __always_inline Elf32_Addr __attribute__ ((unused))
elf_machine_load_address (void)
{
	return 0;
}

static __always_inline void
elf_machine_relative (DL_LOADADDR_TYPE load_off, const Elf32_Addr rel_addr,
		      Elf32_Word relative_count)
{
    Elf32_Rela *rpnt = (void *) rel_addr;

    do {
        unsigned long *reloc_addr = (unsigned long *) DL_RELOC_ADDR(load_off, rpnt->r_offset);

        *reloc_addr = DL_RELOC_ADDR(load_off, rpnt->r_addend);
        rpnt++;
    } while(--relative_count);
}
#endif /* !_ARCH_DL_SYSDEP */
