#include <stdio.h>
#include <stdlib.h>

#define noinline __attribute__ ((noinline))
#define noinstrument __attribute__ ((hcos_noinstrument))

struct foo {
  int i;
  int j;
};

struct kmem_cache {
  int size;
};

static struct foo allocation1;
static struct foo allocation2;
static struct foo allocation3;

#define FAIL(...) do {				\
    fprintf(stderr, "FAIL: " __VA_ARGS__);	\
    exit(1);					\
  } while (1)

struct expected_report {
  enum alloc_sem {
    MALLOC,
    KMEM,
    MMAP,
    MUNMAP,
  } semantics;

  void *addr;
  unsigned int size;
  int lineno;
};

#define MALLOC_CALL(ADDR, SIZE, LINENO)		\
  {MALLOC, ADDR, SIZE, LINENO}

#define KMEM_ALLOC_CALL(ADDR, SIZE, LINENO)	\
  {KMEM, ADDR, SIZE, LINENO}

#define MMAP_CALL(ADDR, LINENO)			\
  {MMAP, ADDR, 0, LINENO}

#define MUNMAP_CALL(ADDR, LINENO)		\
  {MUNMAP, ADDR, 0, LINENO}

static struct expected_report expected[] = {
  MALLOC_CALL(&allocation1, sizeof(struct foo), 111),
  MMAP_CALL(&allocation2, 114),
  MUNMAP_CALL(&allocation2, 117),
  MALLOC_CALL(&allocation1, sizeof(struct foo), 121),
  KMEM_ALLOC_CALL(&allocation3, sizeof(struct foo), 124),
};

static int num_reports = 0;
static int expected_size = sizeof(expected) / sizeof(struct expected_report);

noinstrument void check_report(enum alloc_sem semantics, void *addr, unsigned int size, int lineno)
{
  if (num_reports >= expected_size)
    FAIL("Too many hook executions.\n");

  if (semantics != expected[num_reports].semantics)
    FAIL("Wrong type of hook.\n");

  if (addr != expected[num_reports].addr)
    FAIL("Expected address %p but got address %p.\n", expected[num_reports].addr, addr);

  if (size != expected[num_reports].size)
    FAIL("Expected line number %u but got %u\n", expected[num_reports].size, size);

  if (lineno != expected[num_reports].lineno)
    FAIL("Expected line number %d but got %d\n", expected[num_reports].lineno, lineno);

  num_reports++;
}

noinline void *kmalloc(unsigned int size)
{
  /* Without a printf, GCC might inline this despite noinline. */
  printf("In kmalloc()\n\n");
  return &allocation1;
}

noinline void *kmem_cache_alloc(struct kmem_cache *cache)
{
  /* Without a printf, GCC might inline this despite noinline. */
  printf("In kmem_cache()\n");
  return &allocation3;
}

noinline void *kmap()
{
  /* Without a printf, GCC might inline this despite noinline. */
  printf("In kmap()\n");
  return &allocation2;
}

noinline void kunmap(void *addr)
{
  printf("kunmap: %p\n", addr);
}

int main()
{
  struct kmem_cache cache;

  struct foo *foo = kmalloc(sizeof(struct foo));
  printf("%p\n", foo);

  foo = kmap();
  printf("%p\n", foo);

  kunmap(foo);
  printf("%p\n", foo);

  /* Test the case when a function gets called without storing its return value. */
  kmalloc(sizeof(struct foo));

  cache.size = sizeof(struct foo);
  foo = kmem_cache_alloc(&cache);
  printf("%p\n", foo);

  if (num_reports != expected_size)
    FAIL("Not enough hook executions (only %d of %d expected).\n", num_reports, expected_size);

  return 0;
}

void  __kmalloc_hook(void *addr, unsigned int size, const char *file, int lineno)
{
  printf("Inside kmalloc hook:\n  Addr: %p\n  Size: %d\n  At: %s:%d\n", addr, size, file, lineno);
  check_report(MALLOC, addr, size, lineno);
}

void  __kmem_alloc_hook(void *addr, struct kmem_cache *cache, const char *file, int lineno)
{
  int size = cache->size;
  printf("Inside kmalloc hook:\n  Addr: %p\n  Size: %d\n  At: %s:%d\n", addr, size, file, lineno);
  check_report(KMEM, addr, size, lineno);
}

void  __kmap_hook(void *addr, unsigned int size, const char *file, int lineno)
{
  printf("Inside kmap hook:\n  Addr: %p\n  At: %s:%d\n", addr, file, lineno);
  check_report(MMAP, addr, size, lineno);
}

void  __kunmap_hook(void *addr, unsigned int size, const char *file, int lineno)
{
  printf("Inside kunmap hook:\n  Addr: %p\n  At: %s:%d\n", addr, file, lineno);
  check_report(MUNMAP, addr, size, lineno);
}
