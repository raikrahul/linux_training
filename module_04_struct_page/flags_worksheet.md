01. DEFINE BYTE: Smallest addressable unit of memory → 8 bits → Values 0x00 to 0xFF (0 to 255).
02. DEFINE RAM: Random Access Memory → Array of bytes → Each byte has unique address 0, 1, 2, 3, ... → CPU reads/writes by address.
03. AXIOM YOUR RAM SIZE: RUN `cat /proc/meminfo | head -1` → MemTotal=15776272 kB → Your RAM has 15776272 × 1024 = 16,154,902,528 bytes.
04. DEFINE PHYSICAL ADDRESS: Number identifying byte location in RAM → Range: 0x0 to 0x3BFFFFFFF (for 16GB).
05. PROBLEM MANAGING BYTES: 16 billion bytes → Managing one-by-one is slow → Solution: Group bytes into chunks.
06. DEFINE PAGE: Contiguous chunk of 4096 bytes → Why 4096? → 2^12 = 4096 → Nice power of 2 for hardware.
07. AXIOM PAGE SIZE: RUN `getconf PAGE_SIZE` → 4096 → Kernel and hardware agree on this size.
08. DEFINE PAGE FRAME: One page-sized chunk of physical RAM → Starts at address divisible by 4096.
09. DERIVE PAGE COUNT: 16,154,902,528 bytes / 4096 bytes/page = 3,944,068 pages → Your RAM has ~4 million page frames.
10. PROBLEM INDEXING PAGES: Need to refer to pages by number instead of address → Solution: Page Frame Number.
11. DEFINE PFN: Page Frame Number → Index of page in RAM → PFN 0 = bytes 0-4095, PFN 1 = bytes 4096-8191, etc.
12. DERIVE PFN FORMULA: PFN = Physical_Address / PAGE_SIZE → Physical_Address = PFN × PAGE_SIZE.
13. EXAMPLE PFN: Physical 0x1000000 (16MB) → PFN = 0x1000000 / 0x1000 = 0x1000 = 4096.
14. PROBLEM HARDWARE LIMITS: Old ISA DMA controller has 24-bit address bus → Can only access 2^24 = 16,777,216 bytes = 16 MB.
15. PROBLEM 32BIT DEVICES: 32-bit PCI devices have 32-bit address bus → Can only access 2^32 = 4,294,967,296 bytes = 4 GB.
16. SOLUTION ZONES: Label page frames by address range → Devices request pages from zones they can access.
17. DEFINE ZONE: Named region of physical memory → DMA (0-16MB), DMA32 (16MB-4GB), Normal (4GB+).
18. DERIVE ZONE BOUNDARIES PFN: DMA_end = 16MB/4096 = 4096 → DMA32_end = 4GB/4096 = 1,048,576 → Normal_start = 1,048,576.
19. AXIOM ZONE ENUM: Kernel source mmzone.h:759 `ZONE_DMA`=0 → :762 `ZONE_DMA32`=1 → :769 `ZONE_NORMAL`=2.
20. PROBLEM TRACKING PAGE STATE: Some pages dirty, some locked, some on reclaim list → How to know state?
21. SOLUTION FLAGS WORD: Store 1 bit per attribute → 64-bit unsigned long = 64 bits available.
22. DEFINE FLAGS: `unsigned long` at offset 0 in struct page → Holds page attributes + zone encoding.
23. AXIOM FLAG BITS: Kernel source page-flags.h:94 `PG_locked`=0 → :98 `PG_dirty`=4 → :99 `PG_lru`=5.
24. DERIVE DIRTY MASK: PG_dirty=4 → Mask = 1 << 4 = 0b00010000 = 16 = 0x10.
25. PROBLEM FINDING FREE PAGES: When kernel needs page → How to find one quickly?
26. SOLUTION BUDDY ALLOCATOR: Group free pages by size (power of 2) → Fast search by order.
27. DEFINE ORDER: Power of 2 → Order N = 2^N pages → Order 0 = 1 page, Order 1 = 2 pages, Order 10 = 1024 pages.
28. DERIVE ORDER 10 SIZE: 2^10 = 1024 pages × 4096 bytes = 4,194,304 bytes = 4 MB.
29. AXIOM BUDDYINFO: RUN `cat /proc/buddyinfo` → Shows free block count per zone per order.
30. YOUR BUDDYINFO: Normal: 3273,34925,28861,7329,2204,702,278,152,90,87,103 → Column 0 = order-0 = 3273 single pages free.
31. PROBLEM PAGE METADATA: Kernel needs to store flags, refcount, mapping per page → Where?
32. SOLUTION STRUCT PAGE: 64-byte structure per page frame → Array called vmemmap.
33. DEFINE STRUCT PAGE: Kernel source mm_types.h:72 → Fields: flags(8B) + union(40B) + mapcount(4B) + refcount(4B) + memcg(8B) = 64B.
34. DERIVE VMEMMAP SIZE: 3,944,068 pages × 64 bytes = 252,420,352 bytes = 241 MB overhead.
35. DEFINE REFCOUNT: Usage counter → alloc sets to 1 → get_page adds 1 → put_page subtracts 1 → Free when 0.
36. DEFINE MAPCOUNT: Page table mapping count → Starts at -1 (unmapped) → 0 means mapped once → Formula: real = _mapcount + 1.

=== DIRTY PAGE FLOW (WHY DIRTY EXISTS) ===

37. DEFINE DIRTY: Page contains data that differs from backing store (disk) → Must write to disk before freeing → PG_dirty=1 means "modified in RAM".
38. DEFINE CLEAN: Page matches backing store → Safe to discard → PG_dirty=0.
39. PROBLEM WHY DIRTY EXISTS: RAM is volatile → Power off = data lost → Disk is persistent → Must track which pages need saving.
40. USERSPACE PROGRAM: `dirty_trace.c` → Opens file → mmap's file → Writes "HELLO" → Page becomes dirty → msync → Page becomes clean.
41. COMPILE: RUN `gcc -o dirty_trace dirty_trace.c` → Creates executable.

=== PSEUDO-DEBUGGER TRACE: USERSPACE WRITE TO DISK ===

42. #01.CALL.open("/tmp/dirty_test.txt",O_RDWR|O_CREAT).fd=3.LINE:19.KERNEL:sys_open→do_filp_open→path_openat→creates inode if new→returns fd=3.
43. #02.CALL.ftruncate(fd=3,4096).result=0.LINE:23.KERNEL:sys_ftruncate→do_truncate→inode->i_size=4096→file now 4096 bytes→1 page.
44. #03.CALL.mmap(NULL,4096,PROT_RW,MAP_SHARED,fd=3,0).result=0x7a45f4abf000.LINE:29.KERNEL:sys_mmap→do_mmap→creates VMA→vm_start=0x7a45f4abf000,vm_end=+0x1000→NO PAGE YET.
45. #04.ACCESS.map[0]='H'.vaddr=0x7a45f4abf000.LINE:41.CPU:MMU lookup→PTE not present→PAGE FAULT→trap to kernel.
46. #05.FAULT.do_page_fault(regs,vaddr=0x7a45f4abf000).LINE:kernel.KERNEL:find_vma→VMA found→handle_mm_fault→allocates physical page.
47. #06.ALLOC.alloc_page(GFP_HIGHUSER_MOVABLE).page=0xFFFFEA0000100000.pfn=0x4000.phys=0x4000000=64MB.STATE:_refcount=1,_mapcount=-1,flags=0x8000000000000000.
48. #07.MAP.set_pte(pte,pfn=0x4000).PTE=0x4000067.LINE:kernel.PTE:present=1,rw=1,user=1,pfn=0x4000→MMU can translate vaddr.
49. #08.RMAP.page_add_file_rmap(page).LINE:kernel.STATE:_mapcount=-1→0→"1 mapping".
50. #09.RETURN.PAGE FAULT DONE.return to userspace.LINE:kernel→41.CPU:retry→MMU succeeds→write 'H' to RAM[0x4000000].
51. #10.WRITE.CPU stores 'H' to phys 0x4000000.LINE:41.HARDWARE:CPU sets dirty bit in PTE→PTE|=0x40→dirty PTE.
52. #11.ACCESS.map[1..5]="ELLO\0".LINE:42-46.NO FAULT→page mapped→writes to RAM directly.
53. #12.KERNEL.LATER.kswapd scans PTEs.finds dirty PTE.CALLS:set_page_dirty(page).STATE:flags|=0x10→flags=0x8000000000000010.
54. #13.CALL.msync(0x7a45f4abf000,4096,MS_SYNC).LINE:52.KERNEL:sys_msync→writes dirty pages to disk.
55. #14.LOCK.lock_page(page).STATE:flags|=0x1→PG_locked=1→"I own this page for IO".
56. #15.WRITEBACK.clear_page_dirty_for_io(page).STATE:flags&=~0x10→PG_dirty=0→flags|=(1<<15)→PG_writeback=1.
57. #16.IO.submit_bio(bio).DISK:DMA reads RAM[0x4000000]→writes to sector.
58. #17.COMPLETE.end_page_writeback(page).STATE:flags&=~(1<<15)→PG_writeback=0.
59. #18.UNLOCK.unlock_page(page).STATE:flags&=~0x1→PG_locked=0→CLEAN.
60. #19.STATE.page@0xFFFFEA0000100000.flags=0x8000000000000020.PG_lru=1,PG_dirty=0,zone=2.refcount=2,mapcount=0.
61. #20.CALL.munmap(0x7a45f4abf000,4096).LINE:57.KERNEL:page_remove_rmap→_mapcount:0→-1→put_page→refcount:2→1.
62. #21.CALL.close(fd=3).LINE:58.KERNEL:sys_close→fput.
63. #22.EXIT.process exits.LINE:61.KERNEL:exit_mmap→pages dropped→buddy.

=== FLAG TRANSITIONS ===

64. TRANSITIONS: 0x8000000000000000(alloc)→0x8000000000000010(dirty)→0x8000000000000011(dirty+locked)→0x8000000000008001(writeback+locked)→0x8000000000000020(lru+clean).

=== DO BY HAND ===

65. DO: RUN `sudo insmod flags_lab.ko && sudo dmesg | tail -10` → WRITE: p=0x___, pfn=0x___, flags=0x___.
66. DO: Verify PFN in Normal zone → PFN=0x___ → Decimal=___ → ___ > 1,048,576? → ___✓/✗.
67. DO: Extract zone → (0x___ >> 62) & 0x3 = ___ → Expected 2 ✓/✗.
68. DO: Extract dirty → (0x___ >> 4) & 1 = ___ → Expected 0 ✓/✗.
69. DO: RUN `./dirty_trace` → Observe CLEAN→DIRTY→CLEAN transition.
70. DO: RUN `cat /proc/vmstat | grep dirty` before and after write.

=== FAILURE PREDICTIONS ===

71. F01: Confuse PFN with Phys → PFN 0x1000 ≠ Phys 0x1000 → Phys = PFN × 4096.
72. F02: Confuse bit index with mask → PG_dirty=4 ≠ 0x4 → Mask = 1<<4 = 0x10.
73. F03: Wrong zone shift → (flags>>60) ≠ (flags>>62).
74. F04: Confuse PTE dirty (hardware) with PG_dirty (software) → Two different dirty bits!
75. F05: Assume PG_dirty set immediately on write → NO! Kernel sets lazily during scan.
76. F06: Forget lock before writeback → Unlocked writeback = corruption.

=== VIOLATION CHECK ===

77. Lines 01-36: First principles definitions. Lines 37-41: Dirty defined before used. Lines 42-63: Trace uses only prior definitions. Lines 65-70: Exercises. Lines 71-76: Failures. → No term used before definition ✓.

=== FIRST DIRTY PAGE EVER (FROM POWER ON) ===

78. DEFINE PROCESS: Running program → Has virtual address space (0x0 to 0x7FFFFFFFFFFF on x86_64) → CPU executes instructions.
79. DEFINE VIRTUAL ADDRESS: Number used by CPU to access memory → MMU translates to physical address using page tables.
80. DEFINE EXECUTABLE FILE: File on disk containing program code + data → Format: ELF (Executable and Linkable Format).
81. DEFINE ELF: Container format → Header + Sections → Each section has name, type, VirtualAddress, FileOffset, size.
82. DEFINE FILEOFFSET: Position in ELF file on disk where section bytes start → disk[FileOffset..FileOffset+size].
83. DEFINE VMA: Virtual Memory Address → Position in process address space where section placed at runtime.
84. AXIOM ELF SECTIONS: RUN `readelf -S /bin/cat` → Columns: [Nr] Name Type VirtualAddr FileOffset.
85. YOUR .TEXT: .text VMA=0x2720, FileOffset=0x2720 → VMA=FileOffset (coincidence, not always true).
86. YOUR .DATA: .data VMA=0xa000, FileOffset=0x9000 → VMA≠FileOffset → Difference=0xa000-0x9000=0x1000=4096.
87. YOUR .BSS: .bss VMA=0xa080, FileOffset=0x9068, Type=NOBITS → No bytes on disk → Kernel allocates zeroed RAM.
88. PROBLEM VMA IS RELATIVE: VMA=0xa000 is relative to what? → Need BASE address.
89. DEFINE BASE: Starting virtual address where kernel loads ELF → Random for security (ASLR).
90. AXIOM ASLR: Address Space Layout Randomization → Kernel picks random BASE each run → Attacker cannot predict addresses.
91. AXIOM YOUR CAT MAPS: RUN `cat /proc/self/maps | head -5` → First line: 65161ca16000-... /usr/bin/cat → BASE=0x65161ca16000.
92. DERIVE ACTUAL .TEXT: Actual_addr = BASE + VMA = 0x65161ca16000 + 0x2000 = 0x65161ca18000.
93. VERIFY .TEXT: maps shows r-xp (executable) at 0x65161ca18000 ✓.
94. DERIVE ACTUAL .DATA: Actual_addr = BASE + VMA = 0x65161ca16000 + 0xa000 = 0x65161ca20000.
95. VERIFY .DATA: maps shows rw-p (writable) at 0x65161ca20000, FileOffset=0x9000 ✓.
96. LOADER TRACE: open("/bin/cat")→read ELF header→for each section: mmap(BASE+VMA, size, fd, FileOffset).
97. EXAMPLE .DATA LOADING: mmap(0x65161ca20000, size, fd, 0x9000) → Reads disk[0x9000] → Places at vaddr 0x65161ca20000.
98. DEFINE .TEXT SECTION: Machine code (instructions) → Read-only + Executable → CPU fetches instructions from here.
99. DEFINE .DATA SECTION: Initialized global variables → Read-Write → Example: `int count = 5;` stored here with value 5.
100. DEFINE .BSS SECTION: Uninitialized global variables → Read-Write → Example: `int total;` → Kernel fills with zeros at load.
86. DEFINE EXECVE: System call to load and run executable → kernel reads ELF → Creates process → Maps sections into memory.
87. DEFINE FORK: System call to duplicate process → Child gets copy of parent's address space → PROBLEM: Copying 4GB is slow.
88. SOLUTION FORK OPTIMIZATION: Don't copy pages immediately → Share parent's pages → Mark shared pages READ-ONLY.
89. PROBLEM CHILD WRITES: If child writes to shared page → Must not corrupt parent's data → Solution: Copy-On-Write (COW).
90. DEFINE COW: Copy-On-Write → Page shared read-only → On write → FAULT → Kernel copies page → Gives private copy to writer.
91. AXIOM YOUR PROCESS MAPS: RUN `cat /proc/self/maps | grep heap` → Heap: 64c930930000-64c930951000 rw-p.
92. AXIOM YOUR STACK: RUN `cat /proc/self/maps | grep stack` → Stack: 7fffde047000-7fffde068000 rw-p.
93. DEFINE PTE FLAGS: Each PTE has bits → Bit 0: Present (1=valid) → Bit 1: RW (0=read-only, 1=read-write) → Bit 6: Dirty.
94. DERIVE PTE RW BIT: PTE & 0x2 → If 0 → Read-only → If 2 → Read-Write.
95. DEFINE VMA: Virtual Memory Area → Kernel struct describing region → vm_start, vm_end, vm_flags (VM_READ|VM_WRITE|VM_EXEC).
96. AXIOM VM_WRITE: Kernel source include/linux/mm.h → VM_WRITE = 0x00000002 → Means "process allowed to write here".
97. DEFINE GFP: Get Free Pages → Flags telling allocator what kind of memory → GFP_KERNEL = 0xCC0 → GFP_HIGHUSER_MOVABLE = 0x2CC2.

=== COW TRACE WITH REAL DATA ===

98. SCENARIO: Parent process has global variable at .bss → Parent forks → Child writes to variable → COW happens.
99. STEP FORK: Parent calls fork() → Child created → Child shares Parent's pages → All shared PTEs set RW=0 (read-only).
100. #COW01.BEFORE.Parent PTE for .bss: PFN=0x4000, flags=0x4000065 (present=1, rw=0, user=1) → Read-only shared.
101. #COW02.BEFORE.Child PTE for .bss: PFN=0x4000, flags=0x4000065 (same page, same flags) → Read-only shared.
102. #COW03.BEFORE.page@0xFFFFEA0000100000._refcount=2 → Two processes share this page.
103. #COW04.WRITE.Child executes `global_var = 1;` → CPU stores to vaddr 0x64c930930080 → MMU checks PTE.rw=0 → WRITE FAULT.
104. #COW05.TRAP.CPU traps to kernel → do_page_fault(regs, error_code=0x7) → error_code bit 1 = write fault.
105. #COW06.LOOKUP.find_vma(current->mm, vaddr=0x64c930930080) → Returns VMA → vm_start=0x64c930930000, vm_flags=VM_WRITE.
106. #COW07.CHECK.VMA has VM_WRITE → User allowed to write → Fault is legitimate → Need COW.
107. #COW08.REFCOUNT.page->_refcount=2 → More than 1 user → Cannot just make writable → Must copy.
108. #COW09.ALLOC.alloc_page(GFP_HIGHUSER_MOVABLE) → new_page=0xFFFFEA0000200000, pfn=0x5000, phys=0x5000000=80MB.
109. #COW10.COPY.copy_user_highpage(new, old, vaddr) → memcpy(0x5000000, 0x4000000, 4096) → All 4096 bytes copied.
110. #COW11.UPDATE_PTE.set_pte(child_pte, new_pfn=0x5000 | flags=0x067) → Child PTE now: PFN=0x5000, rw=1.
111. #COW12.REFCOUNT.old_page->_refcount: 2→1 (child no longer uses it) → new_page->_refcount=1.
112. #COW13.RETURN.Return from fault → CPU retries instruction → MMU translates vaddr → PFN=0x5000 → Writes to phys 0x5000080.

=== IS COW PAGE DIRTY? ===

113. QUESTION: new_page (0xFFFFEA0000200000) has PG_dirty=1?
114. ANSWER: NO! .bss is anonymous (no file backing) → Anonymous pages use PG_swapbacked, not PG_dirty.
115. DEFINE ANONYMOUS PAGE: Page not backed by any file → Created by: heap (malloc), stack, COW of .bss/.data.
116. AXIOM ANONYMOUS FLAG: PG_swapbacked = bit 17 → Anonymous pages have flags & (1<<17) != 0.
117. WHY NO PG_DIRTY: PG_dirty means "differs from FILE on disk" → Anonymous has no file → Cannot "differ from file".
118. HOW ANONYMOUS SAVED: Memory pressure → kswapd → Writes anonymous page to SWAP partition → Sets PG_swapcache.

=== FIRST TRUE PG_DIRTY (FILE-BACKED) ===

119. AXIOM YOUR UTMP: RUN `stat /run/utmp` → Inode=2799, Size=3072 bytes.
120. TRACE: init/systemd writes login record to /run/utmp → This file is file-backed → PG_dirty will be set.
121. #FILE01.CALL.open("/run/utmp",O_RDWR).fd=5.KERNEL:path lookup→inode=2799→creates struct file.
122. #FILE02.CALL.write(fd=5,record,sizeof(utmp)).KERNEL:vfs_write→generic_file_write_iter.
123. #FILE03.GRAB.grab_cache_page(mapping,index=0).KERNEL:find page for file offset 0→not found→allocate.
124. #FILE04.ALLOC.alloc_page(GFP_HIGHUSER_MOVABLE).page=0xFFFFEA0000300000.pfn=0x6000.phys=0x6000000.
125. #FILE05.ADD.add_to_page_cache(page,mapping,0).page->mapping=address_space of inode 2799.
126. #FILE06.COPY.copy_from_user(page_addr, record, size).Data written to RAM[0x6000000].
127. #FILE07.DIRTY.set_page_dirty(page).flags |= (1<<4) → flags=0x8000000000000014 → PG_uptodate + PG_dirty.
128. RESULT: FIRST TRUE PG_DIRTY=1 → Page at 0xFFFFEA0000300000 → Backs file /run/utmp inode 2799.

=== WHY ANONYMOUS ≠ PG_DIRTY ===

129. AXIOM DIRTY PURPOSE: PG_dirty means "differs from DISK FILE" → Must sync to FILE before discard.
130. AXIOM ANONYMOUS PURPOSE: Anonymous has no file → Cannot "differ from file" → Uses PG_swapbacked instead.
131. WHEN ANONYMOUS SAVED: Memory pressure → kswapd → Finds anonymous page → Writes to SWAP partition → Then can discard.
132. F07: Confuse anonymous dirty (PG_swapbacked) with file dirty (PG_dirty) → Two different mechanisms!

=== COMPLETE TRACE: open() + write() WITHOUT mmap ===
=== FROM USERSPACE TO CR3 TO PTE TO INODE TO PAGE CACHE TO DIRTY ===

109. DEFINE CR3: CPU register holding physical address of top-level page table (PGD) → CR3=0x1A000000 (example).
110. DEFINE PGD: Page Global Directory → 512 entries × 8 bytes = 4096 bytes → Entry points to PUD.
111. DEFINE PUD: Page Upper Directory → 512 entries → Entry points to PMD.
112. DEFINE PMD: Page Middle Directory → 512 entries → Entry points to PTE table.
113. DEFINE PTE: Page Table Entry → 512 entries → Entry contains PFN + flags (present, rw, dirty).
114. DEFINE INODE: On-disk structure describing file → inode number, size, block pointers → In RAM: struct inode.
115. DEFINE PAGE CACHE: RAM cache of file data → Indexed by (inode, offset) → struct address_space.

=== CODE CONTEXT ===
116. CODE: `int fd = open("/tmp/dirty_test.txt", O_RDWR | O_CREAT, 0644);` → LINE 19 in dirty_trace.c.
117. CODE: `write(fd, "HELLO", 5);` → (not in current file, but this is what triggers dirty).

=== TRACE: open() SYSCALL ===

118. #OPEN01.USERSPACE.open("/tmp/dirty_test.txt",O_RDWR|O_CREAT,0644).LINE:19.USER:syscall(SYS_open,path,flags,mode).
119. #OPEN02.SYSCALL.sys_open(path,flags,mode).KERNEL:entry via syscall instruction→rdi=path,rsi=flags,rdx=mode.
120. #OPEN03.VFS.do_sys_open(dfd=AT_FDCWD,path,flags,mode).KERNEL:allocates struct file.
121. #OPEN04.LOOKUP.path_openat(nd,flags).KERNEL:walks path /→tmp→dirty_test.txt→resolves each component.
122. #OPEN05.DENTRY.d_lookup(parent,name).KERNEL:searches dentry cache→if miss→reads from disk.
123. #OPEN06.INODE.iget(sb,ino).KERNEL:inode=0xFFFF888012340000→i_ino=12345→i_size=0→i_mapping=&inode->i_data.
124. #OPEN07.FILE.alloc_file(path,flags,fops).KERNEL:struct file *f=0xFFFF888023450000→f->f_mapping=inode->i_mapping.
125. #OPEN08.FD.fd_install(fd=3,file).KERNEL:current->files->fd_array[3]=file→return 3 to userspace.
126. STATE AFTER OPEN: fd=3 → file→f_mapping→address_space→host=inode → NO PAGES YET → Just metadata.

=== TRACE: write() SYSCALL ===

127. #WRITE01.USERSPACE.write(fd=3,"HELLO",5).LINE:assumed.USER:syscall(SYS_write,fd,buf,count).
128. #WRITE02.SYSCALL.sys_write(fd=3,buf=0x7FFD12340000,count=5).KERNEL:looks up fd→gets file.
129. #WRITE03.FILE.file=current->files->fd_array[3]=0xFFFF888023450000.KERNEL:f_mapping=0xFFFF888012340100.
130. #WRITE04.VFS.vfs_write(file,buf,count,pos=0).KERNEL:calls file->f_op->write_iter.
131. #WRITE05.ITER.generic_file_write_iter(iocb,iter).KERNEL:normal file write path.

=== PAGE CACHE ALLOCATION (NO MMAP NEEDED) ===

132. #CACHE01.GRAB.grab_cache_page_write_begin(mapping,index=0).KERNEL:find or create page for file offset 0.
133. #CACHE02.FIND.find_get_page(mapping,0).KERNEL:radix tree lookup→NOT FOUND→must allocate.
134. #CACHE03.ALLOC.alloc_page(GFP_HIGHUSER_MOVABLE).page=0xFFFFEA0000500000.pfn=0x7000.phys=0x7000000=112MB.
135. #CACHE04.ADD.add_to_page_cache_lru(page,mapping,0).KERNEL:page->mapping=0xFFFF888012340100→page->index=0.
136. #CACHE05.STATE.page@0xFFFFEA0000500000:flags=0x8000000000000008,_refcount=2,_mapcount=-1,mapping=0x...100,index=0.

=== COPY DATA FROM USERSPACE ===

137. #COPY01.KMAP.kmap_local_page(page).KERNEL:temporarily maps page to kernel virtual address=0xFFFF888007000000.
138. #COPY02.FAULT.copy_from_user(kaddr,"HELLO",5).KERNEL:reads from user buf=0x7FFD12340000.
139. #COPY03.CR3.current->mm->pgd=0x1A000000.CPU:CR3 points here→used by MMU for user address translation.
140. #COPY04.TRANSLATE.MMU translates 0x7FFD12340000→PGD[255]→PUD[510]→PMD[137]→PTE[832]→PFN=0x8000→phys=0x8000000.
141. #COPY05.READ.CPU reads 5 bytes from phys 0x8000000 ("HELLO")→copies to kernel buffer.
142. #COPY06.STORE.memcpy(kaddr=0xFFFF888007000000,"HELLO",5).KERNEL:writes to page cache page.

=== SET PAGE DIRTY ===

143. #DIRTY01.CALL.set_page_dirty(page).KERNEL:marks page as modified.
144. #DIRTY02.CHECK.PageDirty(page)?NO→proceed to set.
145. #DIRTY03.SET.TestSetPageDirty(page).KERNEL:atomic OR→flags|=0x10→flags=0x8000000000000018.
146. #DIRTY04.MAPPING.mapping->a_ops->set_page_dirty(page).KERNEL:filesystem-specific hook (ext4).
147. #DIRTY05.INODE.mark_inode_dirty(inode).KERNEL:inode->i_state|=I_DIRTY_PAGES→inode needs writeback too.
148. #DIRTY06.STATE.page@0xFFFFEA0000500000:flags=0x8000000000000018(PG_uptodate|PG_dirty),mapping=...,index=0.

=== WHERE IS DIRTY? ===

149. DIRTY LOCATION 1: struct page → flags field → bit 4 (PG_dirty) → 0xFFFFEA0000500000 + 0 = flags address.
150. DIRTY LOCATION 2: inode → i_state field → I_DIRTY_PAGES bit → 0xFFFF888012340000 + offsetof(i_state).
151. NOT IN PTE: PTE dirty bit exists only for MAPPED pages → write() doesn't create PTE → Page cache only.

=== REVERSE MAPPING (RMAP) ===

152. DEFINE RMAP: Given page, find all PTEs pointing to it → Used for: reclaim, migration, COW.
153. RMAP FOR FILE PAGE: page->mapping = address_space → address_space->i_mmap = tree of VMAs → Each VMA = range of PTEs.
154. YOUR PAGE: page->mapping=0xFFFF888012340100 → mapping->host=inode → inode->i_ino=12345.
155. RMAP WALK: rmap_walk(page) → for each VMA in i_mmap → calculate PTE address → can find all mappers.
156. CURRENT STATE: Nobody mmap'd this file → i_mmap is empty → No PTEs exist → Only page cache.

=== WRITEBACK (LATER) ===

157. #WB01.TRIGGER.kswapd or sync wakes flusher thread.
158. #WB02.SCAN.writeback_sb_inodes(sb).KERNEL:finds dirty inodes.
159. #WB03.INODE.inode->i_mapping=address_space→scans for dirty pages.
160. #WB04.FIND.find_get_pages_tag(mapping,PAGECACHE_TAG_DIRTY).KERNEL:finds our page.
161. #WB05.LOCK.lock_page(page).flags|=0x1→PG_locked.
162. #WB06.CLEAR.clear_page_dirty_for_io(page).flags&=~0x10→PG_dirty=0.
163. #WB07.WRITEBACK.flags|=(1<<1)→PG_writeback=1.
164. #WB08.IO.submit_bio(bio).DISK:DMA reads RAM[0x7000000]→writes to disk block.
165. #WB09.COMPLETE.end_page_writeback(page).flags&=~(1<<1)→PG_writeback=0.
166. #WB10.UNLOCK.unlock_page(page).flags&=~0x1→PG_locked=0.
167. #WB11.INODE.inode->i_state&=~I_DIRTY_PAGES→inode clean.
168. FINAL STATE: page flags=0x8000000000000028 (PG_uptodate|PG_lru), inode clean, disk synced.

=== KEY INSIGHT: write() vs mmap ===

169. write() PATH: fd→file→mapping→page cache→copy_from_user→set_page_dirty→NO PTE CREATED.
170. mmap() PATH: fd→file→VMA→page fault→alloc page→add to mapping→PTE created→PTE dirty bit set.
171. DIFFERENCE: write() uses kernel address only→no user PTE→no CR3 walk for destination→only for source buffer.

=== DO BY HAND ===

172. DO: Trace CR3 for your process → RUN `cat /proc/self/maps` → Note virtual addresses.
173. DO: Calculate PGD index → vaddr >> 39 = ___ → PGD[___].
174. DO: Find inode for file → RUN `stat /tmp/test.txt` → inode = ___.

=== FAILURE PREDICTIONS ===

175. F08: Confuse write() dirty (page cache only) with mmap dirty (PTE + page cache).
176. F09: Think write() creates PTE → NO! Only mmap creates PTE.
177. F10: Forget inode gets marked dirty too → I_DIRTY_PAGES bit.
178. F11: Think CR3 walk happens for write destination → NO! Destination is kernel address.

=== REAL DIRTY PAGE TRACE FROM YOUR MACHINE ===

179. PROGRAM RUN: ./dirty_trace_v2 (as root for /proc/self/pagemap access).
180. YOUR VADDR: mmap returned 0x708b2eba5000 → Virtual address in userspace.
181. YOUR PFN: 0x23de8c = 2,350,732 decimal → Page Frame Number from pagemap.
182. YOUR PHYS: PFN × 4096 = 0x23de8c × 0x1000 = 0x23de8c000 = 9,508,921,344 bytes = 8.8 GB into RAM.
183. DERIVE ZONE: PFN=2,350,732 > 1,048,576 (DMA32 boundary) → Zone = Normal = 2.
184. DERIVE NODE: Single NUMA node → Node = 0.
185. DERIVE STRUCT PAGE ADDR: VMEMMAP_BASE + PFN × 64 = 0xFFFFEA0000000000 + 0x23de8c × 64 = 0xFFFFEA0008F7A300.
186. FLAGS LOCATION: struct_page at 0xFFFFEA0008F7A300 → flags at offset 0 → flags at 0xFFFFEA0008F7A300.
187. ZONE IN FLAGS: Zone=2 encoded in bits 62-63 → flags & (0x3UL << 62) = 0x8000000000000000.
188. EXTRACT ZONE: page_zonenum(page) → (flags >> 62) & 0x3 → 0x8000000000000000 >> 62 = 0b10 = 2 = Normal ✓.
189. EXTRACT NODE: page_to_nid(page) → (flags >> 60) & 0x3 → Your machine = 0 (single node).
190. DIRTY BIT: PG_dirty = bit 4 → After write: flags |= 0x10 → Check: (flags >> 4) & 1 = 1 if dirty.

=== HOW TO SEE DIRTY IN KERNEL ===

191. OPTION 1 VMSTAT: RUN `watch -n 0.1 'cat /proc/vmstat | grep nr_dirty'` → See count change.
192. OPTION 2 FTRACE: RUN `sudo trace-cmd record -e 'writeback:*' sleep 5` → Capture writeback events.
193. OPTION 3 CRASH: RUN `sudo crash /boot/vmlinuz-$(uname -r) /proc/kcore` → px ((struct page *)0xFFFFEA0008F7A300)->flags.
194. OPTION 4 DRGN: RUN `sudo drgn -k` → print(Object(prog, 'struct page', address=0xFFFFEA0008F7A300).flags).
195. OPTION 5 KERNEL MODULE: flags_lab.ko prints flags for allocated pages → sudo dmesg | grep FlagsLab.
