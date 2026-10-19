/* inetrosco.c - rosco support for INET.
 *
 * get_bp() is the frame pointer for stacktrace() (inet/stacktrace.c), in
 * assembly elsewhere (lib/m68000/misc/get_bp.s).  stacktrace() keeps it in
 * an unsigned int, which is 16 bits here, so it could not walk the stack
 * anyway: 0 makes it print nothing.  It is only called on a panic.
 */

unsigned get_bp()
{
  return 0;
}
