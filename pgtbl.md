# PAGE TABLE ANSWER & NOTE
1. For every page table entry in the print_pgtbl output, explain what it logically contains and what its permission bits are. Figure 3.4 in the xv6 book might be helpful, although note that the figure might have a slightly different set of pages than process that's being inspected here. Note that xv6 doesn't place the virtual pages consecutively in physical memory.
- ans: 直接追蹤syscall-pgpte: 
  - 0x5B: 0b0101_1011 > Access, User, eXecutable, Readable, Valid.
    - 推斷:R-X && U==1: 對應user text。
  - 0x17: 0b0001_0111 > User, Writable, Readable, Valid.
    - RW- && U==1: 不能執行但可以讀寫，應該是user stack和heap,或者user data。
  - 0x07: 0b0000_0111 > Writable, Readable, Valid.
    - RW- && U==0: kernel data, page tables。
  - 0xD7: 0b1101_0111 > Dirty, Accessed, User, Writable, Readable, Valid.
    - 0x17: 0b0001_0111 > User, Writable, Readable, Valid.
  - 0xC7: 0b1100_0111 > Dirty, Accessed, Writable, Readable, Valid.
    - RW- && U==0: kernel data, page tables。
  - 0x4B: 0b0100_1011 > Accessed, eXecutable, Readable, Valid.
    - R-X: kernel code。
  - 0x00: 未分配，或保護區域。
  NOTE: 
  - accessed: 代表沒有被用過。
  - dirty: 已經有人修改過資料了。
```
va 0x0 pte 0x21FC885B pa 0x87F22000 perm 0x5B
va 0x1000 pte 0x21FC7C5B pa 0x87F1F000 perm 0x5B
va 0x2000 pte 0x21FC7817 pa 0x87F1E000 perm 0x17
va 0x3000 pte 0x21FC7407 pa 0x87F1D000 perm 0x7
va 0x4000 pte 0x21FC70D7 pa 0x87F1C000 perm 0xD7
va 0x5000 pte 0x0 pa 0x0 perm 0x0
va 0x6000 pte 0x0 pa 0x0 perm 0x0
va 0x7000 pte 0x0 pa 0x0 perm 0x0
va 0x8000 pte 0x0 pa 0x0 perm 0x0
va 0x9000 pte 0x0 pa 0x0 perm 0x0
va 0x3FFFFF6000 pte 0x0 pa 0x0 perm 0x0
va 0x3FFFFF7000 pte 0x0 pa 0x0 perm 0x0
va 0x3FFFFF8000 pte 0x0 pa 0x0 perm 0x0
va 0x3FFFFF9000 pte 0x0 pa 0x0 perm 0x0
va 0x3FFFFFA000 pte 0x0 pa 0x0 perm 0x0
va 0x3FFFFFB000 pte 0x0 pa 0x0 perm 0x0
va 0x3FFFFFC000 pte 0x0 pa 0x0 perm 0x0
va 0x3FFFFFD000 pte 0x0 pa 0x0 perm 0x0
va 0x3FFFFFE000 pte 0x21FD08C7 pa 0x87F42000 perm 0xC7
va 0x3FFFFFF000 pte 0x2000184B pa 0x80006000 perm 0x4B
```

2. Which other xv6 system call(s) could be made faster using this shared page? Explain how.
- ans:只要是user會使用到且不會危害到安全性(kernel info)的應該都可以。ex:struct proc 的 ofile, cwd。