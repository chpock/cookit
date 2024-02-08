#ifndef PANIC_H
#define PANIC_H

#ifdef __GNUC__
EXTERN void
panic _ANSI_ARGS_((char *message, ...)
		  __attribute__ ((__noreturn__ ,
				  __format__(printf,1,2) )) );
#else
EXTERN void panic _ANSI_ARGS_((char *message, ...));
#endif

#endif
