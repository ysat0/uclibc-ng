#include <features.h>

#include <sys/syscall.h>
#define _ERRNO_H
#include <bits/errno.h>

	.text
	.align 4
	.globl	_vfork
	.hidden	_vfork
	.type	_vfork,@function

_vfork:
	mov.l	[r0+],r5
	mov	#0x4111,r1
	mov	#0, r2
	mov.l	#__NR_clone,r15
	int	#0x08
	cmp	#-4096,r1
	bc	1f	/* errno < 0 && errno >=4096 */
	jmp	r5
1:	mov.l	r5,[-r0]
	bra	__syscall_error

weak_alias(_vfork,vfork)
libc_hidden_weak(vfork)
	.end
