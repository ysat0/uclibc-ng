/* Copyright (C) 2022 Yoshinori Sato

uClibc is free software; you can redistribute it and/or modify it
under the terms of the GNU Lesser General Public License as
published by the Free Software Foundation; either version 2.1 of the
License, or (at your option) any later version.

uClibc is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with uClibc; see the file COPYING.LIB.  If not, see
<http://www.gnu.org/licenses/>.  */

#include <features.h>
	.text
	.hidden _dl_linux_resolve
	.globl _dl_linux_resolve
	.type _dl_linux_resolve,%function

/*
 *    _dl_linux_resolve() FDPIC version receives the following parameters from
 *    lazy PLT entry:
 *    R14: GOT address for the resolver GOT
 *    R5: funcdesc_value_reloc_offset(foo)
 *    R13: GOT address for the caller GOT
 *    _dl_linux_resolver() will return a function descriptor address in R1.
 */
_dl_linux_resolve:
	pushm	r1-r4
	mov.l	8[r13], r1
	mov	r5, r2
	mov.l	r14, r13
	bsr	_dl_linux_resolver
	mov.l	4[r1],r13
	mov.l	[r1],r14
	popm	r1-r4
	jmp	r14

	.size _dl_linux_resolve, .-_dl_linux_resolve
