/* nedalloc, an alternative malloc implementation for multiple threads without
lock contention based on dlmalloc v2.8.3. (C) 2005-2009 Niall Douglas

Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/

#ifndef NEDMALLOC_H
#define NEDMALLOC_H


/* See malloc.c.h for what each function does.

REPLACE_SYSTEM_ALLOCATOR causes nedalloc's functions to be called malloc,
free etc. instead of nedmalloc, nedfree etc. You may or may not want this.

NO_NED_NAMESPACE prevents the functions from being defined in the nedalloc
namespace when in C++ (uses the global namespace instead).

NEDMALLOCEXTSPEC can be defined to be __declspec(dllexport) or
__attribute__ ((visibility("default"))) or whatever you like. It defaults
to extern unless NEDMALLOC_DLL_EXPORTS is set as it would be when building
nedmalloc.dll.

USE_LOCKS can be 2 if you want to define your own MLOCK_T, INITIAL_LOCK,
ACQUIRE_LOCK, RELEASE_LOCK, TRY_LOCK, IS_LOCKED and NULL_LOCK_INITIALIZER.

USE_MAGIC_HEADERS causes nedalloc to allocate an extra three sizeof(size_t)
to each block. nedpfree() and nedprealloc() can then automagically know when
to free a system allocated block. Enabling this typically adds 20-50% to
application memory usage.

USE_ALLOCATOR can be one of these settings:
  0: System allocator (nedmalloc now simply acts as a threadcache).
     WARNING: Intended for DEBUG USE ONLY - not all functions work correctly.
  1: dlmalloc

*/

#include <stddef.h>   /* for size_t */

#ifndef NEDMALLOCEXTSPEC
 #ifdef NEDMALLOC_DLL_EXPORTS
  #define NEDMALLOCEXTSPEC extern __declspec(dllexport)
 #else
  #define NEDMALLOCEXTSPEC extern
 #endif
#endif

#if defined(_MSC_VER) && _MSC_VER>=1400
 #define NEDMALLOCPTRATTR __declspec(restrict)
#endif
#ifdef __GNUC__
 #define NEDMALLOCPTRATTR __attribute__ ((malloc))
#endif
#ifndef NEDMALLOCPTRATTR
 #define NEDMALLOCPTRATTR
#endif

#ifndef USE_MAGIC_HEADERS
 #define USE_MAGIC_HEADERS 0
#endif

#ifndef USE_ALLOCATOR
 #define USE_ALLOCATOR 1 /* dlmalloc */
#endif

#if !USE_ALLOCATOR && !USE_MAGIC_HEADERS
#error If you are using the system allocator then you MUST use magic headers
#endif

#ifdef REPLACE_SYSTEM_ALLOCATOR
 #if USE_ALLOCATOR==0
  #error Cannot combine using the system allocator with replacing the system allocator
 #endif
 #ifndef WIN32	/* We have a dedidicated patcher for Windows */
  #define nedmalloc               malloc
  #define nedcalloc               calloc
  #define nedrealloc              realloc
  #define nedfree                 free
  #define nedmemalign             memalign
  #define nedmallinfo             mallinfo
  #define nedmallopt              mallopt
  #define nedmalloc_trim          malloc_trim
  #define nedmalloc_stats         malloc_stats
  #define nedmalloc_footprint     malloc_footprint
  #define nedindependent_calloc   independent_calloc
  #define nedindependent_comalloc independent_comalloc
  #ifdef _MSC_VER
   #define nedblksize              _msize
  #endif
 #endif
#endif


#ifndef NO_MALLINFO
 #define NO_MALLINFO 0
#endif

#if !NO_MALLINFO
#if defined(__cplusplus)
extern "C" {
#endif
struct mallinfo;
#if defined(__cplusplus)
}
#endif
#endif

#if defined(__cplusplus)
 #if !defined(NO_NED_NAMESPACE)
namespace nedalloc {
 #else
extern "C" {
 #endif
 #define THROWSPEC throw()
#else
 #define THROWSPEC
#endif

/* These are the global functions */

/* Gets the usable size of an allocated block. Note this will always be bigger than what was
asked for due to rounding etc. Tries to return zero if this is not a nedmalloc block (though
one could see a segfault up to 6.25% of the time). On Win32 SEH is used to guarantee that a
segfault never happens.
*/
NEDMALLOCEXTSPEC size_t nedblksize(void *mem) THROWSPEC;

NEDMALLOCEXTSPEC void nedsetvalue(void *v) THROWSPEC;

NEDMALLOCEXTSPEC NEDMALLOCPTRATTR void * nedmalloc(size_t size) THROWSPEC;
NEDMALLOCEXTSPEC NEDMALLOCPTRATTR void * nedcalloc(size_t no, size_t size) THROWSPEC;
NEDMALLOCEXTSPEC NEDMALLOCPTRATTR void * nedrealloc(void *mem, size_t size) THROWSPEC;
NEDMALLOCEXTSPEC void   nedfree(void *mem) THROWSPEC;
NEDMALLOCEXTSPEC NEDMALLOCPTRATTR void * nedmemalign(size_t alignment, size_t bytes) THROWSPEC;
#if !NO_MALLINFO
NEDMALLOCEXTSPEC struct mallinfo nedmallinfo(void) THROWSPEC;
#endif
NEDMALLOCEXTSPEC int    nedmallopt(int parno, int value) THROWSPEC;
NEDMALLOCEXTSPEC void*  nedmalloc_internals(size_t *granularity, size_t *magic) THROWSPEC;
NEDMALLOCEXTSPEC int    nedmalloc_trim(size_t pad) THROWSPEC;
NEDMALLOCEXTSPEC void   nedmalloc_stats(void) THROWSPEC;
NEDMALLOCEXTSPEC size_t nedmalloc_footprint(void) THROWSPEC;
NEDMALLOCEXTSPEC NEDMALLOCPTRATTR void **nedindependent_calloc(size_t elemsno, size_t elemsize, void **chunks) THROWSPEC;
NEDMALLOCEXTSPEC NEDMALLOCPTRATTR void **nedindependent_comalloc(size_t elems, size_t *sizes, void **chunks) THROWSPEC;

/* Destroys the system memory pool used by the functions above.
Useful for when you have nedmalloc in a DLL you're about to unload.
If you call ANY nedmalloc functions after calling this you will
get a fatal exception!
*/
NEDMALLOCEXTSPEC void neddestroysyspool() THROWSPEC;

/* These are the pool functions */
struct nedpool_t;
typedef struct nedpool_t nedpool;

/* Creates a memory pool for use with the nedp* functions below.
Capacity is how much to allocate immediately (if you know you'll be allocating a lot
of memory very soon) which you can leave at zero. Threads specifies how many threads
will *normally* be accessing the pool concurrently. Setting this to zero means it
extends on demand, but be careful of this as it can rapidly consume system resources
where bursts of concurrent threads use a pool at once.
*/
NEDMALLOCEXTSPEC NEDMALLOCPTRATTR nedpool *nedcreatepool(size_t capacity, int threads) THROWSPEC;

/* Destroys a memory pool previously created by nedcreatepool().
*/
NEDMALLOCEXTSPEC void neddestroypool(nedpool *p) THROWSPEC;

/* Sets a value to be associated with a pool. You can retrieve this value by passing
any memory block allocated from that pool.
*/
NEDMALLOCEXTSPEC void nedpsetvalue(nedpool *p, void *v) THROWSPEC;
/* Gets a previously set value using nedpsetvalue() or zero if memory is unknown.
Optionally can also retrieve pool.
*/
NEDMALLOCEXTSPEC void *nedgetvalue(nedpool **p, void *mem) THROWSPEC;

/* Trims the thread cache for the calling thread, returning any existing cache
data to the central pool. Remember to ALWAYS call with zero if you used the
system pool. Setting disable to non-zero replicates neddisablethreadcache().
*/
NEDMALLOCEXTSPEC void nedtrimthreadcache(nedpool *p, int disable) THROWSPEC;

/* Disables the thread cache for the calling thread, returning any existing cache
data to the central pool. Remember to ALWAYS call with zero if you used the
system pool.
*/
NEDMALLOCEXTSPEC void neddisablethreadcache(nedpool *p) THROWSPEC;

NEDMALLOCEXTSPEC NEDMALLOCPTRATTR void * nedpmalloc(nedpool *p, size_t size) THROWSPEC;
NEDMALLOCEXTSPEC NEDMALLOCPTRATTR void * nedpcalloc(nedpool *p, size_t no, size_t size) THROWSPEC;
NEDMALLOCEXTSPEC NEDMALLOCPTRATTR void * nedprealloc(nedpool *p, void *mem, size_t size) THROWSPEC;
NEDMALLOCEXTSPEC void   nedpfree(nedpool *p, void *mem) THROWSPEC;
NEDMALLOCEXTSPEC NEDMALLOCPTRATTR void * nedpmemalign(nedpool *p, size_t alignment, size_t bytes) THROWSPEC;
#if !NO_MALLINFO
NEDMALLOCEXTSPEC struct mallinfo nedpmallinfo(nedpool *p) THROWSPEC;
#endif
NEDMALLOCEXTSPEC int    nedpmallopt(nedpool *p, int parno, int value) THROWSPEC;
NEDMALLOCEXTSPEC int    nedpmalloc_trim(nedpool *p, size_t pad) THROWSPEC;
NEDMALLOCEXTSPEC void   nedpmalloc_stats(nedpool *p) THROWSPEC;
NEDMALLOCEXTSPEC size_t nedpmalloc_footprint(nedpool *p) THROWSPEC;
NEDMALLOCEXTSPEC NEDMALLOCPTRATTR void **nedpindependent_calloc(nedpool *p, size_t elemsno, size_t elemsize, void **chunks) THROWSPEC;
NEDMALLOCEXTSPEC NEDMALLOCPTRATTR void **nedpindependent_comalloc(nedpool *p, size_t elems, size_t *sizes, void **chunks) THROWSPEC;

#if defined(__cplusplus)
}
#endif

#endif
