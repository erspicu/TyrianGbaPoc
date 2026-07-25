#include <errno.h>
#include <stddef.h>
#include <stdint.h>

extern unsigned char __eheap_start[];
extern unsigned char __eheap_end[];

void *fake_heap_end;

void *_sbrk(ptrdiff_t increment)
{
    static uintptr_t current;
    const uintptr_t start = (uintptr_t)__eheap_start;
    const uintptr_t limit = (uintptr_t)__eheap_end;
    uintptr_t previous;

    if (current == 0) {
        current = start;
    }
    previous = current;

    if (increment >= 0) {
        const uintptr_t amount = (uintptr_t)increment;
        if (current > limit || amount > limit - current) {
            errno = ENOMEM;
            return (void *)-1;
        }
        current += amount;
    } else {
        const uintptr_t amount = (uintptr_t)(-(increment + 1)) + 1;
        if (current < start || amount > current - start) {
            errno = ENOMEM;
            return (void *)-1;
        }
        current -= amount;
    }

    fake_heap_end = (void *)current;
    return (void *)previous;
}
