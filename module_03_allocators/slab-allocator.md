---
title: "Slab Allocator"
difficulty: Intermediate
order: 11
---

# Slab Allocator

Slab allocates fixed-size objects efficiently, avoiding buddy allocator overhead.

## Why Slab?

```
64-byte object using buddy:
  Order-0 = 4096 bytes
  Waste = 4096 - 64 = 4032 bytes = 98.4%!

64-byte object using slab:
  One page holds 64 objects
  Waste = 4096 - (64 × 64) = 0 bytes = 0%
```

## Slab Structure

```
┌─────────────────────────────────────────────┐
│              SLAB CACHE                      │
│  ┌───────────────────────────────────────┐  │
│  │            SLAB (4KB page)            │  │
│  │ ┌────┬────┬────┬────┬─────┬────┐     │  │
│  │ │Obj0│Obj1│Obj2│Obj3│ ... │Obj63│     │  │
│  │ │64B │64B │64B │64B │     │64B  │     │  │
│  │ └────┴────┴────┴────┴─────┴────┘     │  │
│  └───────────────────────────────────────┘  │
└─────────────────────────────────────────────┘
```

## API

```c
// Create cache
struct kmem_cache *cache = kmem_cache_create(
    "my_objects", 64, 0, 0, NULL);

// Allocate
void *obj = kmem_cache_alloc(cache, GFP_KERNEL);

// Free
kmem_cache_free(cache, obj);

// Destroy
kmem_cache_destroy(cache);
```

## Performance

| Operation | Slab | Buddy |
|-----------|------|-------|
| Allocate | ~30 cycles | ~300 cycles |
| Free | ~30 cycles | ~300 cycles |
| Speedup | 10× | baseline |
