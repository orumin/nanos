#include <kernel.h>

//#define TAG_HEAP_DEBUG
#ifdef TAG_HEAP_DEBUG
#define tag_debug(x, ...) do {rprintf("%s: " x, __func__, ##__VA_ARGS__);} while(0)
#else
#define tag_debug(x, ...)
#endif

struct tagheap {
    struct heap h;
    heap mh;
    u64 vtag;
};

static void tag_dealloc(heap h, u64 a, bytes s)
{
    struct tagheap *th = (struct tagheap *)h;
    tag_debug("%s: tag %d, a 0x%lx, s 0x%lx\n", __func__, th->vtag >> VA_TAG_OFFSET, a, s);
    deallocate_u64(th->mh, a & MASK(VA_TAG_OFFSET), s);
}

static u64 tag_alloc(heap h, bytes s)
{
    struct tagheap *th = (struct tagheap *)h;
    void *p = allocate(th->mh, s);
    if (p == INVALID_ADDRESS)
        return INVALID_PHYSICAL;
    u64 a = u64_from_pointer(p);
    assert((a >> VA_TAG_OFFSET) == 0);
    a |= th->vtag;
    tag_debug("%s: tag %d, s 0x%lx, a 0x%lx\n", __func__, th->vtag >> VA_TAG_OFFSET, s, a);
    return a;
}

heap allocate_tagged_region(kernel_heaps kh, u64 tag)
{
    heap h = heap_general(kh);
    struct tagheap *th = allocate(h, sizeof(struct tagheap));
    if (th == INVALID_ADDRESS)
        return INVALID_ADDRESS;
    assert(tag < 256);
    th->mh = h;
    th->vtag = tag << VA_TAG_OFFSET;
    th->h.alloc = tag_alloc;
    th->h.dealloc = tag_dealloc;
    th->h.destroy = 0;
    th->h.pagesize = 32; // XXX
    th->h.allocated = 0;
    th->h.total = 0;
    tag_debug("%s: tag %d, bits 0x%lx, heap %p\n", __func__, tag, th->vtag, th);
    return &th->h;
}

void cpu_init(int cpu)
{
    cpuinfo ci = cpuinfo_from_id(cpu);
    register u64 a = u64_from_pointer(ci);
    asm volatile("mov x18, %0; msr tpidr_el1, %0" ::"r"(a));
}

