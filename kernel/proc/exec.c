#include "defs.h"
#include "file.h"
#include "memlayout.h"
#include "param.h"
#include "proc.h"
#include "riscv.h"
#include "types.h"
#include "userabi.h"

#define ELF_MAGIC 0x464C457FU  /* "\x7FELF" little-endian */
#define ELF_PROG_LOAD 1

#define ELF_PROG_FLAG_EXEC  1
#define ELF_PROG_FLAG_WRITE 2
#define ELF_PROG_FLAG_READ  4

struct elfhdr {
  uint magic;
  uchar elf[12];
  ushort type;
  ushort machine;
  uint version;
  uint64 entry;
  uint64 phoff;
  uint64 shoff;
  uint flags;
  ushort ehsize;
  ushort phentsize;
  ushort phnum;
  ushort shentsize;
  ushort shnum;
  ushort shstrndx;
};

struct proghdr {
  uint type;
  uint flags;
  uint64 off;
  uint64 vaddr;
  uint64 paddr;
  uint64 filesz;
  uint64 memsz;
  uint64 align;
};

static int
flags2perm(int flags)
{
  int perm = PTE_U;

  if (flags & ELF_PROG_FLAG_READ)
    perm |= PTE_R;
  if (flags & ELF_PROG_FLAG_WRITE)
    perm |= PTE_W | PTE_R;
  if (flags & ELF_PROG_FLAG_EXEC)
    perm |= PTE_X;

  return perm;
}

static int
loadseg(pagetable_t pagetable, struct inode *ip, struct proghdr *ph)
{
  uint64 va;

  for (va = ph->vaddr; va < ph->vaddr + ph->memsz; va += PGSIZE) {
    char *mem = kalloc();
    if (mem == 0)
      return -1;

    memset(mem, 0, PGSIZE);

    if (va < ph->vaddr + ph->filesz) {
      uint64 fileoff = ph->off + (va - ph->vaddr);
      uint n = ph->filesz - (va - ph->vaddr);
      if (n > PGSIZE)
        n = PGSIZE;

      if (readi(ip, 0, (uint64)mem, fileoff, n) != n) {
        kfree(mem);
        return -1;
      }
    }

    if (mappages(pagetable, (uint64)mem, va, PGSIZE,
                 flags2perm(ph->flags)) != 0) {
      kfree(mem);
      return -1;
    }
  }

  return 0;
}

/* exec — xv6 风格 ELF 加载器的第一版
 *
 * 已支持：
 *   - 读取并校验 ELF header。
 *   - 遍历 program header。
 *   - 加载 PT_LOAD 段到 ELF 指定虚拟地址。
 *   - 根据 ELF flags 设置用户页权限。
 *   - 设置 epc = elf.entry。
 *   - 构造 argc/argv 用户栈。
 *
 * 暂不支持：
 *   - guard page。
 */
int
exec(char *path, uint64 argv)
{
  struct inode *ip;
  struct elfhdr elf;
  struct proghdr ph;
  pagetable_t pagetable = 0;
  pagetable_t oldpagetable;
  uint64 oldsz;
  uint64 sz = 0;
  uint64 stackva = 0;
  struct proc *p = myproc();
  char argbuf[MAXARG][MAXARGLEN];
  uint64 uargv[MAXARG];
  int argc = 0;

  if ((ip = namei(path)) == 0)
    return -1;

  ilock(ip);
  if (ip->type != T_FILE)
    goto bad;

  if (readi(ip, 0, (uint64)&elf, 0, sizeof(elf)) != sizeof(elf))
    goto bad;
  if (elf.magic != ELF_MAGIC)
    goto bad;
  if (elf.phentsize != sizeof(ph))  // 程序头表项大小
    goto bad;

  pagetable = uvmcreate();
  if (pagetable == 0)
    goto bad;

  for (int i = 0; i < elf.phnum; i++) {
    uint64 phoff = elf.phoff + i * sizeof(ph);

    if (readi(ip, 0, (uint64)&ph, phoff, sizeof(ph)) != sizeof(ph))
      goto bad;
    if (ph.type != ELF_PROG_LOAD)
      continue;
    if (ph.memsz < ph.filesz)
      goto bad;
    if (ph.vaddr + ph.memsz < ph.vaddr)
      goto bad;
    if ((ph.vaddr % PGSIZE) != 0)
      goto bad;

    uint64 end = PGROUNDUP(ph.vaddr + ph.memsz);
    if (end > sz)
      sz = end;

    if (loadseg(pagetable, ip, &ph) != 0)
      goto bad;
  }

  stackva = PGROUNDUP(sz);
  char *stack = kalloc();
  if (stack == 0)
    goto bad;
  memset(stack, 0, PGSIZE);
  if (mappages(pagetable, (uint64)stack, stackva, PGSIZE,
               PTE_R | PTE_W | PTE_U) != 0) {
    kfree(stack);
    goto bad;
  }

  for (;;) {
    uint64 uarg;

    if (argc >= MAXARG)
      goto bad;
    if (copyin(p->pagetable, (char *)&uarg,
               argv + argc * sizeof(uint64), sizeof(uint64)) < 0)
      goto bad;
    if (uarg == 0)
      break;
    if (copyinstr(p->pagetable, argbuf[argc], uarg, MAXARGLEN) < 0)
      goto bad;
    argc++;
  }

  uint64 sp = stackva + PGSIZE;

  for (int i = argc - 1; i >= 0; i--) {
    uint64 len = strlen(argbuf[i]) + 1;

    sp -= len;
    sp &= ~0xFUL;

    if (sp < stackva)
      goto bad;
    if (copyout(pagetable, sp, argbuf[i], len) < 0)
      goto bad;

    uargv[i] = sp;
  }
  uargv[argc] = 0;

  sp -= (argc + 1) * sizeof(uint64);
  sp &= ~0xFUL;
  if (sp < stackva)
    goto bad;

  if (copyout(pagetable, sp, (char *)uargv,
              (argc + 1) * sizeof(uint64)) < 0)
    goto bad;

  uint64 argv_user = sp;

  iunlock(ip);
  iput(ip);

  oldpagetable = p->pagetable;
  oldsz = p->sz;

  p->pagetable = pagetable;
  p->sz = stackva + PGSIZE;
  p->trapframe->epc = elf.entry;
  p->trapframe->sp = sp;
  // p->trapframe->a0 = argc;
  p->trapframe->a1 = argv_user;
  safestrcpy(p->name, path, sizeof(p->name));

  uvmfree(oldpagetable, oldsz);
  return argc;

bad:
  if (pagetable) {
    uint64 freesz = stackva ? stackva + PGSIZE : sz;
    uvmfree(pagetable, freesz);
  }
  iunlock(ip);
  iput(ip);
  return -1;
}
