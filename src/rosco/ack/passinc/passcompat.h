/* Forced into every compile of the ACK passes as Minix programs
 * (mkpass.py): declarations of the functions in passlib/ that the Minix
 * 2.0.4 C library lacks.  Without a declaration gcc assumes int, which is
 * 16 bits with -mshort and truncates the pointer strdup() returns.
 */
#ifndef _ROSCO_PASSCOMPAT_H
#define _ROSCO_PASSCOMPAT_H

char *strdup(const char *_s);

#endif /* _ROSCO_PASSCOMPAT_H */
