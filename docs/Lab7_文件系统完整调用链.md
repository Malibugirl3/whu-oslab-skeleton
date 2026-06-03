# Lab7 文件系统：从 0 到 1 的完整调用链

> 从按下 `make run` 到文件读写完成，OS 启动 → init 程序 → 文件系统调用的完整旅程。

---

## 🗺️ 总览地图

```
你按下的命令: make run
     │
     ▼
┌──────────────────────────────────────────────────────┐
│ 第0站: QEMU 启动 + 硬件初始化                         │
│ 第1站: 内核启动 (main.c)                             │
│ 第2站: 文件系统初始化 (fsinit)                        │
│ 第3站: 第一个用户进程 init 诞生                       │
│ 第4站: init 调用 open("/hello.txt", O_CREAT|O_WRONLY) │
│ 第5站: init 调用 write(fd, "Hello...", 20)            │
│ 第6站: init 调用 read(fd, buf, 64)                    │
│ 第7站: init 调用 unlink("/hello.txt")                 │
└──────────────────────────────────────────────────────┘
```

---

## 第0站：QEMU 启动 — "一台虚拟的 RISC-V 电脑"

```
$ make run
```

Makefile 里实际执行的命令是：

```bash
qemu-system-riscv64 \
  -machine virt                 \  # "给我一台虚拟的 RISC-V 机器"
  -kernel kernel/kernel         \  # "把 kernel 文件加载到内存 0x80000000"
  -drive file=fs.img,...        \  # "插上一块虚拟硬盘，内容是 fs.img"
  -device virtio-blk-device,... \  # "硬盘通过 virtio 协议通信"
  -m 128M                          # "给 128MB 内存"
```

**关键比喻**：
- `kernel/kernel` 就像是 **Windows 的 bootmgr + ntoskrnl**，是一块可以直接执行的 ELF 二进制
- `fs.img` 就像是 **C 盘的镜像文件**，里面存着文件系统的所有数据
- QEMU 把这台"虚拟电脑"的内存、硬盘、CPU 都模拟好，然后把 kernel 放到内存里，CPU 从 `0x80000000` 开始执行

**此时内存布局**：

```
0x80000000 ┌──────────────┐
           │  kernel 代码  │  ← QEMU 把 kernel 文件加载到这里
           │  kernel 数据  │
           │  kernel 栈    │
           │  ...          │
           │  空闲内存      │
0x88000000 └──────────────┘

硬盘 (virtio, 地址 0x10001000):
┌──────────────────────────┐
│ fs.img 的内容             │
│  block 0: boot block     │
│  block 1: superblock     │  ← 文件系统的"户口本"
│  block 2: bitmap         │  ← 每个 bit 表示一个数据块是否被占用
│  block 3~N: inode area   │  ← 所有文件的"身份证"
│  block N+1~...: data     │  ← 文件的实际内容
└──────────────────────────┘
```

---

## 第1站：内核启动 — `kernel/boot/main.c`

```c
void main() {
    console_init();       // 1. 初始化串口（能打印了）
    kinit();              // 2. 初始化物理内存分配器
    kvminit();            // 3. 建立页表（含 virtio MMIO 映射）
    procinit();           // 4. 初始化进程表
    trap_init();          // 5. 设置中断/异常处理
    virtio_disk_init();   // 6. 🆕 初始化硬盘驱动
    fsinit(ROOTDEV);      // 7. 🆕 初始化文件系统
    user_init();          // 8. 创建第一个用户进程 init
    scheduler();          // 9. 开始调度，永不停歇
}
```

**用盖房子的比喻**：

| 步骤 | 做了什么 | 比喻 |
|------|---------|------|
| `console_init` | 接通显示器 | 拉电线 |
| `kinit` | 内存管理器就绪 | 准备好建材仓库 |
| `kvminit` | 页表建立（含 virtio MMIO） | 画好建筑图纸 |
| `procinit` | 进程表初始化 | 准备好工位 |
| `trap_init` | 中断向量就绪 | 安装门铃系统 |
| `virtio_disk_init` | 硬盘驱动就绪 | **接通硬盘电源** |
| `fsinit` | 文件系统就绪 | **打开文件柜** |
| `user_init` | 创建 init 进程 | **派第一个工人上岗** |
| `scheduler` | 无限循环调度 | 工头开始分派工作 |

---

### 第1.5站：`virtio_disk_init()` — 让内核能跟硬盘"说话"

这一步在 `kernel/driver/virtio_disk.c`。

**比喻**：硬盘是一个只懂 virtio 协议的外国人，`virtio_disk_init` 相当于安排一个翻译官。

```
virtio_disk_init():
  1. 重置 virtio 设备
  2. 设置 QUEUE（一个环形缓冲区，用于和硬盘交换请求/响应）
  3. 告诉硬盘 "我准备好了"（DRIVER_OK）

之后，任何代码想读写硬盘，只需：
  把请求放进 virtio 队列 → 硬盘处理 → 从队列取结果

这就是 virtio_disk_rw(buf, 1) 做的事：
  "请把第 N 号数据块的内容读到 buf 里"（bread 内部调用这个）
  "请把 buf 的内容写到第 N 号数据块"（bwrite 内部调用这个）
```

---

## 第2站：`fsinit(ROOTDEV)` — 文件系统的"开机自检"

```c
void fsinit(int dev) {
    // 1. 读 block 1（superblock）
    readsb(dev, &sb);

    // 2. 验证魔法数
    if (sb.magic != FSMAGIC)
        panic("invalid file system");

    // 3. 初始化 inode 缓存
    for (int i = 0; i < NINODE; i++)
        icache.inode[i].valid = 0;
}
```

**superblock 是什么？它是文件系统的"户口本"**：

```
superblock 里存着:
  sb.size       = 整个磁盘有多少个 block       (比如 1000)
  sb.nblocks    = 数据区有多少个 block          (比如 950)
  sb.ninodes    = 最多有多少个 inode            (比如 200)
  sb.inodestart = inode 区域从第几个 block 开始  (比如 2)
  sb.bmapstart  = bitmap 从第几个 block 开始     (比如 3)
  sb.magic      = 魔数 0x10203040（验证这是一个合法的文件系统）
```

**`fs.img` 是谁造的？** 是 `mkfs.c` 这个独立工具：

```
mkfs.c 做的事情：
  1. 创建一个空文件 fs.img
  2. 在 block 0 写 boot block（占位）
  3. 在 block 1 写 superblock
  4. 在 block 2 写 bitmap（标记哪些块已被占用）
  5. 在 block 3+ 写 inode 区域
  6. 创建根目录 inode（inum=1），写入 "." 和 ".."
  7. 把 fs.img 写到磁盘上
```

---

## 第3站：`user_init()` — 第一个用户进程的诞生

```c
void user_init(void) {
    struct proc *p;
    p = allocproc();         // 分配一个进程结构体

    // 把 init 程序的机器码加载到进程的地址空间
    uvminit(p->pagetable, initcode, sizeof(initcode));

    p->sz = PGSIZE;          // 进程占用 1 页内存

    // 设置 trapframe，让进程"看起来像"刚从用户态发起了一次 ecall
    p->trapframe->epc = 0;   // 从地址 0 开始执行
    p->trapframe->sp = PGSIZE; // 栈在页的顶端

    p->state = RUNNABLE;     // 标记为"可以运行了"
}
```

**initcode 是什么？** 它是 `user/init.c` 编译并链接成的一段**位置无关的二进制**（通过 `user/initcode.S` → `initcode`）。

**比喻**：`user_init` 就像 HR 给新员工办入职：
- 分配工位（allocproc）
- 发工作手册（加载 initcode 到内存）
- 告诉员工你的工位在哪、从哪开始干活（epc=0, sp=PGSIZE）
- 标记为"可以上班了"（RUNNABLE）

然后 `scheduler()` 发现这个进程是 RUNNABLE，就切换到它。进程开始从 `epc=0` 执行——也就是 `initcode` 的第一条指令。

---

### 第3.5站：initcode — 从内核态"跳"到用户态

```
initcode（汇编）做的事情：
  1. 把 init 程序的实际入口地址放到寄存器
  2. 执行 ecall → 进入内核 → syscall() 分发 → SYS_exec
  3. SYS_exec：把 /init 程序从磁盘加载到内存，替换当前进程
```

但在这个 Lab 里，initcode **直接就是 init 程序本身**（简化处理），所以它直接执行 `init.c` 里的 `main()`。

---

## 🔥 第4站：`open("/hello.txt", O_CREAT | O_WRONLY)` — 完整的调用链

这是最复杂的一站，切成 **7 段**，像剥洋葱一样一层层剥开。

### 段1：用户态 → 内核态（usys.S + trap）

```c
// user/init.c 第10行
fd = open("/hello.txt", O_CREAT | O_WRONLY);
```

**第一步：编译器把 `open()` 变成什么？**

`open()` 在 `user/usys.S` 里是一个汇编桩：

```asm
open:
    li a7, 15           # a7 = SYS_open 的系统调用号
    ecall               # 触发异常，陷入内核
    ret                 # 返回（此时 a0 = 内核写的返回值）
```

**第二步：`ecall` 之后发生了什么？**

```
CPU 执行 ecall → 硬件自动:
  1. 把当前 PC 保存到 sepc（记住从哪来的）
  2. 把 CPU 模式从 User 切换到 Supervisor（内核态）
  3. 跳到 stvec 指向的地址（trampoline → usertrap → syscall()）
```

`syscall()` 在 `kernel/syscall/syscall.c`：

```c
void syscall() {
    int num = p->trapframe->a7;      // 拿到系统调用号 15
    p->trapframe->a0 = syscalls[num](); // 调用 syscalls[15] = sys_open
}
```

**关键点**：`a7` 寄存器里存的是系统调用号（15 = SYS_open），`a0` 存路径字符串的地址，`a1` 存 flags。

---

### 段2：`sys_open()` — 参数的"翻译"

```c
uint64 sys_open(void) {
    char path[128];
    int fd, flags;

    argint(1, &flags);             // 从用户的 a1 寄存器拿 flags
    argstr(0, path, sizeof(path)); // 从用户的 a0 寄存器拿路径字符串
    // path 现在 = "/hello.txt", flags = O_CREAT | O_WRONLY = 3
```

**`argstr` 干了什么？**

用户的字符串在用户地址空间，内核不能直接用。`argstr` 调用 `copyin()` 把字符串从用户内存复制到内核的 `path` 数组：

```
用户内存                       内核内存
┌──────────┐    copyin()     ┌──────────┐
│ /hello   │ ──────────────→ │ /hello   │
│ .txt\0   │                 │ .txt\0   │
└──────────┘                 └──────────┘
   用户态地址                    内核栈上的数组
```

---

### 段3：`nameiparent()` — 路径解析

```c
if ((dp = nameiparent(path, name)) == 0)
    return -1;
// 传入: path = "/hello.txt"
// 传出: dp = 根目录的 inode, name = "hello.txt"
```

**`nameiparent` 做了什么？**

```
nameiparent("/hello.txt", name):
  │
  ├─ 循环调用 skipelem() 解析路径
  │
  │  第1轮: skipelem("/hello.txt") → "hello.txt" + 剩余=""
  │          namei("hello.txt") → 根目录下找 "hello.txt"
  │          这是最后一个元素，所以返回它的父目录（根目录）
  │          把 "hello.txt" 复制到 name
  │
  └─ 返回: dp = iget(dev, 1)  ← inum=1 是根目录
```

**`skipelem` 的逻辑**（用 "/hello.txt" 为例）：

```
输入: "/hello.txt"
  → 跳过开头的 '/'
  → 找到下一个 '/' 或 '\0' 作为结束位置
  → 把 "hello.txt" 复制到 name 输出缓冲区
  → 返回剩余路径 ""

每次调用吃掉路径的第一段:
  "/a/b/c" → name="a",  返回 "b/c"
  "a/b/c"  → name="a",  返回 "b/c"
  "b/c"    → name="b",  返回 "c"
  "c"      → name="c",  返回 ""
  ""       → 返回 0（结束了）
```

**`namei` 的逻辑**：

```
namei("/hello.txt"):
  │
  ├─ namei 从根目录 inode 开始 (iget(dev, 1))
  │
  ├─ 循环:
  │   第1轮: skipelem("/hello.txt") → "hello.txt"
  │          ilock(ip) 锁住根目录
  │          dirlookup(ip, "hello.txt", 0) → 查找
  │          iunlock(ip)
  │          iput(ip) 释放根目录
  │          ip = 查到的 inode (或 0 表示没找到)
  │
  └─ 返回最终的 ip（如果中间某步没找到就返回 0）
```

**`dirlookup` 做了什么？**（这是理解文件系统的关键！）

```c
dirlookup(dp, "hello.txt", &off):
  // dp 是根目录的 inode
  // 目录的本质也是一个文件，它的"数据"就是一条条 dirent

  从 off=0 开始，每 sizeof(dirent)=16 字节读一条:

  off=0:  读 16 字节 → de = { inum:1, name:"." }      → 跳过
  off=16: 读 16 字节 → de = { inum:1, name:".." }     → 跳过
  off=32: 读 16 字节 → de = { inum:2, name:"hello.txt" } → 匹配！
            if (poff) *poff = 32;  ← 记录偏移，后面 unlink 要用
            return iget(dev, 2);    ← 返回 inum=2 的 inode
```

---

### 段4：创建新文件（O_CREAT 路径）

因为文件还不存在，`dirlookup` 返回 0，进入 `else` 分支：

```c
// 创建新文件
ip = ialloc(dp->dev, T_FILE);   // 在磁盘上分配一个新 inode
ilock(ip);                       // 锁住它
ip->nlink = 1;                   // 🆕 新文件有一个名字指向它
dirlink(dp, name, ip->inum);     // 在父目录里写入一条 dirent
```

**`ialloc` 做了什么？**

```
ialloc(dev, T_FILE):
  │
  └─ 扫描磁盘上的 dinode 区域:
       for inum = 1..sb.ninodes:
         读 dinode[inum]
         if (dip->type == 0):          ← 找到空闲 inode
           memset(dip, 0, sizeof(*dip)) ← 清零
           dip->type = T_FILE          ← 标记为"文件类型"
           bwrite(bp)                  ← 写回磁盘
           return iget(dev, inum)      ← 返回这个 inode 的内存缓存
```

**`dirlink` 做了什么？**

```
dirlink(dp, "hello.txt", inum=2):
  // dp 是根目录 inode
  // 目标：在根目录的数据里写一条 { inum:2, name:"hello.txt" }

  遍历根目录的每条 dirent:
    off=0:  de.inum=1, de.name="."     → 已占用
    off=16: de.inum=1, de.name=".."    → 已占用
    off=32: de.inum=0                  → 🎉 空槽！重复利用

  构造新 dirent:
    de.inum = 2
    strncpy(de.name, "hello.txt", DIRSIZ)

  writei(dp, 0, &de, off=32, sizeof(de)):
    把这条新记录写到根目录文件的第 32 字节处
```

---

### 段5：`filealloc()` — 分配文件描述符结构

```c
f = filealloc();
f->type = FD_INODE;   // 这是一个"inode 类型"的文件
f->ip = ip;           // 指向 inode 2 (hello.txt)
f->off = 0;           // 文件读写位置从 0 开始
f->readable = !(flags & O_WRONLY);  // flags=O_WRONLY → readable=0
f->writable = 1;                     // writable=1
```

**`filealloc` 做了什么？**

```
filealloc():
  ftable 是一个全局数组，有 NFILE=100 个槽位
  扫描 ftable.file[0..99]:
    找到第一个 type==FD_NONE 的槽位
    初始化 ref=1, 返回指针
```

**重要概念：`struct file` 为什么存在？**

不同的进程可以打开同一个文件，但它们的读写位置（off）不同。`struct file` 就是用来记录"谁以什么方式在读写"：

```
进程A                    ftable                     inode
┌────────┐              ┌──────────┐              ┌──────────┐
│ ofile[0]├────────────→│ file[3]  │              │ inode[2] │
│ ofile[1]│              │  off=0   ├─────────────→│ type=T_FILE│
│ ofile[2]│              │  readable│              │ size=20   │
└────────┘              │  writable│              │ nlink=1   │
                         └──────────┘              └──────────┘
进程B
┌────────┐              ┌──────────┐
│ ofile[0]├────────────→│ file[7]  │
│ ofile[1]│              │  off=100 ├──→ 同一个 inode[2]
└────────┘              │  readable│
                         └──────────┘
```

`struct file` 是 Layer 4，"打开的文件"的抽象。

---

### 段6：分配 fd（文件描述符）

```c
for (fd = 0; fd < NOFILE; fd++) {
    if (myproc()->ofile[fd] == 0) {
        myproc()->ofile[fd] = f;
        break;
    }
}
```

**三层结构的关系**：

```
fd (文件描述符)     struct file           struct inode
   数字            (全局打开文件表)         (文件系统 inode 缓存)
   ────            ────────────────         ──────────────────
     0  ─────────→ ftable.file[3] ──────→ icache.inode[x]
     1  (stdout 保留给 UART)                  │
     2                                          ├─ dev (设备号)
     3                                          ├─ inum (inode 编号)
     ...                                       ├─ type (T_FILE/T_DIR)
    15                                         ├─ size (文件大小字节)
                                                ├─ nlink (链接数)
                                                ├─ addrs[0..12] (物理块地址)
                                                └─ ...
```

**为什么需要三层而不是两层？**

| 层 | 回答了什么问题 |
|----|-------------|
| fd（进程级别） | "这个进程的第几个文件？" |
| struct file（全局） | "读写到哪了？有什么权限？有几个进程在用？" |
| struct inode（全局） | "这个文件在磁盘上的哪个位置？多大？" |

---

### 段7：返回 fd 给用户

```c
iunlock(ip);    // 解锁 inode
return fd;      // 比如返回 0
```

`syscall()` 把这个返回值写入 `p->trapframe->a0`，然后 `usertrap` 返回时恢复寄存器。`open()` 的汇编桩拿到 `a0=0`，返回给 `init.c` 的 `main()`。

```
main() 中:
  fd = open("/hello.txt", O_CREAT | O_WRONLY);
  // fd = 0，现在 fd=0 指向 hello.txt 文件
```

---

## ✍️ 第5站：`write(fd, "Hello, File System!\n", 20)`

### 调用链

```
write(0, "Hello, File System!\n", 20)
  │
  ├─ usys.S: li a7, SYS_write; ecall
  │
  ├─ syscall() → sys_write()
  │
  ├─ sys_write():
  │    fd=0, addr=字符串在用户空间的地址, n=20
  │    f = myproc()->ofile[0]     ← 拿到我们刚才创建的 file 结构
  │    filewrite(f, addr, 20)     ← 调用 Layer 4
  │
  └─ filewrite():
       ilock(f->ip)               ← 锁 inode
       writei(f->ip, 1, addr, f->off=0, n=20)  ← Layer 3
       f->off += 20               ← off 变成 20
       iupdate(f->ip)             ← 把新 size 写回磁盘
       iunlock(f->ip)
```

### `writei` 内部做了什么？

```
writei(ip, user_src=1, src=用户地址, off=0, n=20):
  │
  │ 用户地址用 copyin，内核地址直接用 memmove
  │
  ├─ 第1步: bmap(ip, 0) → 将"文件的第0个逻辑块"映射到"磁盘的物理块号"
  │    比如逻辑块 0 → 物理块 50
  │
  │    如果是第一次写，bmap 内部会调用 balloc 分配新块
  │    balloc: 扫描 bitmap，找到第一个空闲块 → 标记为占用 → 返回块号
  │
  ├─ 第2步: bread(dev, 50) → 读出物理块 50 的内容到缓冲区
  │
  ├─ 第3步: copyin(用户页表, bp->data + 0, src, 20)
  │    把 "Hello, File System!\n" 从用户空间复制到缓冲区
  │
  ├─ 第4步: bwrite(bp) → 把缓冲区写回磁盘块 50
  │
  └─ 第5步: ip->size = 20（文件现在有 20 字节了）
```

**磁盘上的数据流**：

```
磁盘 fs.img:
  block 1: superblock     ← 不变
  block 2: bitmap         ← 第50位从0变成1（标记block 50被占用）
  block 3: dinode[1]      ← 根目录，size 增加了
  block 4: dinode[2]      ← hello.txt's dinode, size=20, addrs[0]=50
  ...
  block 50: "Hello, File System!\n" ← 🔥 数据写在这里！
```

---

## 📖 第6站：`read(fd, buf, 64)`

跟 write 是对称的：

```
read(0, buf, 64)
  │
  ├─ sys_read() → fileread(f, buf_addr, 64)
  │
  └─ fileread():
       ilock(f->ip)
       readi(f->ip, 1, buf_addr, f->off=0, n=64)
       f->off += 20  (实际读了20字节)
       iunlock(f->ip)
```

`readi` 和 `writei` 的逻辑完全对称，只是方向相反：
- `writei`: copyin（用户→内核缓冲区）→ bwrite（缓冲区→磁盘）
- `readi`: bread（磁盘→缓冲区）→ copyout（内核缓冲区→用户）

---

## 🗑️ 第7站：`unlink("/hello.txt")`

```
unlink("/hello.txt")
  │
  ├─ sys_unlink():
  │
  ├─ nameiparent("/hello.txt", name) → dp=root, name="hello.txt"
  │
  ├─ ilock(root)
  ├─ dirlookup(root, "hello.txt", &off) → ip=inode[2], off=32
  │
  ├─ 检查: ip 不是目录 ✓
  │
  ├─ memset(&de, 0, sizeof(de));  de.inum = 0;
  ├─ writei(root, 0, &de, off=32, sizeof(de))
  │    └─ 把根目录第 32 字节处的 dirent 写成全零
  │       这相当于"划掉"了 hello.txt 的目录项
  │
  ├─ iput(root)     ← 释放根目录
  │
  ├─ ip->nlink--     ← inode[2].nlink 从 1 变成 0
  │
  ├─ iunlock(ip)
  └─ iput(ip)       ← ref 从 1 变成 0
       │
       └─ 因为 ref==0 且 nlink==0:
            itrunc(ip):
              bfree(block 50)  ← bitmap 第50位清零
              ip->type = 0     ← dinode[2] 标记为空闲
              iupdate(ip)      ← 写回磁盘
```

**删除后的磁盘状态**：

```
block 1: superblock     ← 不变
block 2: bitmap         ← 第50位从1变回0（block 50 现在空闲了）
block 3: dinode[1]      ← 根目录，dirent[2] 的 inum=0（空槽）
block 4: dinode[2]      ← type=0（空闲了，可以被新文件复用）
block 50: "Hello, File System!\n" ← 数据还在！但 bitmap 说它空闲了
```

**关键理解**：删除文件只是删了"户口"和"房产证"，房子本身（数据块）可能还在，只是被标记为"可拆迁"了。

---

## 🎯 总结：各层职责一句话

| 层 | 文件 | 一句话职责 |
|----|------|-----------|
| **用户态** | `init.c` | "我要打开、读、写、删文件" |
| **系统调用桩** | `usys.S` | "把函数调用翻译成 ecall 指令" |
| **Layer 5** | `sysfile.c` | "把用户态参数翻译成内核调用" |
| **Layer 4** | `file.c` | "管理打开的文件：谁在读、读到哪里了" |
| **Layer 3** | `fs.c` | "inode 管理 + 路径解析 + 目录操作 + 数据读写" |
| **Layer 2** | `fs.c` (balloc/bfree) | "磁盘空间管理：分配释放数据块" |
| **Layer 1** | `bio.c` | "缓冲缓存：读过的磁盘块暂存内存" |
| **Layer 0** | `virtio_disk.c` | "跟硬盘硬件对话：读/写物理扇区" |

**整个调用链就像一个接力赛**：

```
init.c  →  usys.S  →  sysfile.c  →  file.c  →  fs.c  →  bio.c  →  virtio_disk.c  →  硬盘
(用户)    (ecall)    (参数翻译)    (fd管理)   (inode)   (缓存)     (硬件驱动)       (数据)
```

---

## 📋 关键数据结构

### superblock（超级块）— 文件系统的户口本

```c
struct superblock {
    uint magic;       // 魔数 0x10203040，验证文件系统合法性
    uint size;        // 磁盘总块数
    uint nblocks;     // 数据块总数
    uint ninodes;     // 最大 inode 数
    uint nlog;        // 日志区块数
    uint logstart;    // 日志区起始块号
    uint inodestart;  // inode 区起始块号
    uint bmapstart;   // 位图区起始块号
};
```

### dinode（磁盘 inode）— 文件在磁盘上的身份证

```c
struct dinode {
    short type;                // 0=空闲, 1=普通文件, 2=目录, 3=设备
    short major, minor;        // 设备号（type==3 时有效）
    short nlink;               // 硬链接计数
    uint size;                 // 文件大小（字节）
    uint addrs[NDIRECT+1];     // 数据块地址表（12个直接+1个间接）
};
```

### inode（内存 inode）— 磁盘 inode 的内存缓存

```c
struct inode {
    uint dev;           // 设备号
    uint inum;          // inode 编号
    int ref;            // 内存引用计数
    int valid;          // 数据是否已从磁盘加载
    short type;         // 文件类型
    short major, minor, nlink;
    uint size;          // 文件大小
    uint addrs[NDIRECT+1]; // 数据块地址
};
```

### struct file（打开的文件实例）

```c
struct file {
    int type;           // FD_NONE=0, FD_INODE=1
    int ref;            // 引用计数
    char readable;      // 是否可读
    char writable;      // 是否可写
    struct inode *ip;   // 对应的 inode
    uint off;           // 当前读写偏移
};
```

### struct dirent（目录项）

```c
struct dirent {
    ushort inum;        // inode 编号（0=空槽）
    char name[DIRSIZ];  // 文件名（最多14字符）
};  // 共16字节
```

---

## 🔑 关键概念

### ref vs nlink

| 变量 | 含义 | 存储位置 | 谁改变它 |
|------|------|---------|---------|
| `ref` | 内存引用计数：有几个进程/代码正在使用这个 inode | 内存 inode | iget(+1), iput(-1) |
| `nlink` | 磁盘链接计数：磁盘上有几个 dirent 指向这个 inode | 磁盘 dinode | 创建时=1, unlink(-1) |

### valid 的含义

| valid | 含义 |
|-------|------|
| 0 | inode 槽位占用中，但磁盘数据还没加载（`iget` 返回后、`ilock` 调用前） |
| 1 | 磁盘数据已加载到内存 inode，可以安全使用其字段 |

### iget 不读磁盘，ilock 才读磁盘

```
iget(dev, inum):  只找缓存槽，valid 可能为 0，不触发磁盘 I/O
ilock(ip):        若 valid==0，从磁盘读取 dinode，设置 valid=1
```

---

## 📚 思考题 Q&A

### Q1: 关于 LRU 不变式

**问题**：`brelse` 把刚释放的块移到链表头部，`bget` 从链表尾部往前找淘汰候选。这个设计保证了"最近使用的块最后被淘汰"。请用一个具体的例子（假设缓冲池共3个槽位，依次操作块 A→B→C→A→D）手动模拟链表状态的变化，验证这个性质。

**答**：

初始状态（哨兵头 head 空链表）：
```
head ↔ head（空双向循环链表）
```

**步骤1: bget(A)** — 缓冲池空，分配空槽，从磁盘读：
```
链表: head ↔ A ↔ head
```

**步骤2: bget(B)** — 分配新槽：
```
链表: head ↔ B ↔ A ↔ head
```

**步骤3: bget(C)** — 填充缓冲池：
```
链表: head ↔ C ↔ B ↔ A ↔ head
```

**步骤4: bget(A)** — A 已在缓存中命中！`brelse` 还未调用过，链表不变。但因为 refcnt>0，它不会被淘汰。等 `brelse(A)` 时，A 被移到头部：
```
链表: head ↔ A ↔ C ↔ B ↔ head
```
A 虽然后进来，但因为它刚被用过，所以在头部（安全区域）。

**步骤5: bget(D)** — 缓冲池满（3 个都在用），需要淘汰。从尾部往前（即 B 方向）找 refcnt==0 的块：
```
链表: head ↔ A ↔ C ↔ B ↔ head
               ↑       ↑
            安全区   淘汰区（最久没用）
```
B 在尾部且 refcnt==0，被选为淘汰候选。B 的 dev/blockno 被更新为 D，valid=0。然后需要重新从磁盘读。

**验证 LRU 性质**：A 因为被重新访问而被移到头部，所以在 D 需要淘汰时 A 被保留了，而 B（最先访问、最久没再用）被淘汰了。✅

---

### Q2: 关于 iget 的"不读磁盘"设计

**问题**：iget 只分配缓存槽（valid=0），ilock 才读磁盘（设置 valid=1）。为什么要分成两步？如果 iget 直接读磁盘，会有什么问题？（提示：考虑两个进程同时 iget 同一个 inode 的情况）

**答**：

如果 `iget` 直接读磁盘，会导致两个严重问题：

1. **性能灾难**：每次获取 inode 引用都触发磁盘 I/O，即使这个 inode 的数据已经在内存缓存中。例如 `namei("/a/b/c")` 路径解析过程中，每一步都要 `iget` 目录 inode，如果每次都读磁盘，路径解析会非常慢。

2. **缓存一致性破坏**：假设进程 A 和进程 B 同时打开同一个文件 `/etc/passwd`：
   - 进程 A 调用 `iget(dev, 7)` — 如果 iget 直接读磁盘，它创建一个新的缓存条目，从磁盘读取 dinode
   - 进程 B 调用 `iget(dev, 7)` — 因为 valid 已经被设为 1，进程 B 在 icache 中找到已存在的条目，ref++ 后返回
   - 但现在有两个问题：进程 B 可能获取到进程 A 还没有完全初始化的缓存条目（竞态条件）；或者如果 iget 每次都覆盖，那么进程 A 对 inode 的修改可能被进程 B 的 iget 覆盖掉

**正确设计的分工**：
- `iget`：纯缓存操作，只在内存中找/分配槽位，保证同一 inode 在内存中只有一份拷贝
- `ilock`：负责按需加载磁盘数据，只在数据确实需要时才读磁盘

---

### Q3: 关于"目录是文件"

**问题**：`dirlookup` 内部调用了 `readi`。请描述当你 `dirlookup(root_inode, "etc", 0)` 时，数据是如何从磁盘流到比较逻辑的——要求说出调用链中每个函数的作用。

**答**：

```
dirlookup(root_inode, "etc", 0):
  │
  ├─ for (off = 0; off < dp->size; off += 16):   // 遍历每条 dirent（每条16字节）
  │
  │   ├─ readi(root_inode, 0, &de, off, 16):     // 从目录 inode 读取一条 dirent
  │   │    │
  │   │    ├─ bmap(dp, off/BSIZE):               // 逻辑块号→物理块号
  │   │    │   └─ 查 dp->addrs[bn] 得到物理块号
  │   │    │       └─ 若未分配则 balloc 分配
  │   │    │
  │   │    ├─ bread(dev, blockno):               // 从磁盘读物理块到缓冲区
  │   │    │   └─ bget: 找/分配缓存槽
  │   │    │       └─ virtio_disk_rw(b, 0):      // Layer 0: 实际读磁盘
  │   │    │
  │   │    └─ memmove(&de, bp->data + off%BSIZE, 16): // 从缓冲区复制到 de
  │   │        └─ brelse(bp): 释放缓冲区引用
  │   │
  │   ├─ if (de.inum == 0): continue;  // 空槽，跳过
  │   │
  │   └─ if (strncmp(de.name, "etc", DIRSIZ) == 0):  // 名字匹配！
  │          return iget(dp->dev, de.inum);  // 返回 etc 的 inode
  │
  └─ return 0;  // 遍历完没找到
```

每个函数的作用：

| 函数 | 作用 |
|------|------|
| `readi` | 从 inode 读取指定偏移的字节数据（不知道也不关心数据是目录还是普通文件） |
| `bmap` | 把"文件的第 N 个逻辑块"翻译成"磁盘的第 M 号物理块" |
| `balloc` | 在 bitmap 中找一个空闲 bit，标记为占用，返回块号 |
| `bread` | 通过缓冲层读一个磁盘块（如果已在缓存中则直接返回） |
| `bget` | 在缓冲池中查找或分配一个缓存槽 |
| `virtio_disk_rw` | 真正的硬件交互：向 virtio 设备发送读写请求 |
| `memmove` | 从缓冲区复制数据到目标地址 |

---

### Q4: 关于文件描述符的两层设计

**问题**：为什么不直接让文件描述符 fd（整数）直接对应 inode 编号，而是要通过 `struct file` 作为中间层？举一个如果省略 `struct file`（fd 直接对应 inode）会出问题的场景。

**答**：

**核心原因**：inode 是文件的**静态标识**（这个文件在磁盘上的哪里），而 fd 代表的是**一次打开操作的动态状态**（读到哪里了、以什么权限打开的）。

如果没有 `struct file` 层，以下场景会出问题：

**场景1：两个进程独立读同一个文件**

进程 A 和进程 B 都打开 `/etc/config`：
- 进程 A 读了 100 字节，停在第 100 字节
- 进程 B 从第 0 字节开始读

如果没有 `struct file` 层，偏移量存在哪里？如果存在 inode 里，进程 A 读到第 100 字节后，进程 B 也会从第 100 字节开始读——这不是进程 B 想要的！

有了 `struct file` 层：
- 进程 A: fd=3 → file[3].off=100
- 进程 B: fd=4 → file[4].off=0
- 都有 file[].ip 指向同一个 inode — 共享数据，独立偏移

**场景2：fork() 后父子进程共享偏移**

```c
int fd = open("/log.txt", O_WRONLY);
if (fork() == 0) {
    write(fd, "child\n", 6);   // 子进程写
} else {
    write(fd, "parent\n", 7);  // 父进程写
}
```

期望输出：`child\nparent\n` 或 `parent\nchild\n`（先后顺序取决于调度，但内容互不覆盖）。

有 `struct file` 层：父子进程的 fd 指向**同一个** `struct file`（fork 时 filedup 只 ref++），它们共享同一个 off。所以无论谁先写，`off` 都会正确递增，不会互相覆盖。

没有 `struct file` 层：inode 没有 off 字段，两个进程都需要自己记录偏移，或者其他复杂的机制——而且 fork 后父子无法共享偏移。

---

### Q5: 关于持久性

**问题**：在 writei 中，你调用了 bwrite(bp) 将修改写回磁盘。如果省略这个 bwrite（只修改内存中的 buf->data，不写磁盘），系统会立刻崩溃吗？数据会丢失吗？在什么情况下可以不立刻 bwrite，而是等到 brelse 的时候再写？（提示：想想 xv6 的日志机制）

**答**：

**系统会立刻崩溃吗？**
不会。系统会继续正常运行——因为数据已经存在于内存的缓冲区（buf->data）中，CPU 和内核代码仍然能访问。但数据**只存在内存中**，没有持久化到磁盘。

**数据会丢失吗？**
会的。如果发生了以下情况：
- 系统断电
- QEMU 被强制关闭（没有正常关机流程）
- 内核 panic

这些情况下，内存中的数据丢失，磁盘上的文件内容不会更新，写入操作的效果完全丢失。

**什么情况下可以不立刻 bwrite？**

在 xv6 的日志机制下，采用 **write-ahead logging (WAL)** 策略：

1. 写入操作先把数据写到日志区（log area）
2. 写一个 commit 记录到日志
3. 然后在 `end_op` 时批量将日志中的数据安装（install）到实际数据块
4. 最后清除日志

在这种设计下，`writei` 中的 `bwrite` 不是必须的——你可以依赖日志层的 `log_write` 来保证持久性。日志机制保证了**原子性**：即使系统在写入过程中崩溃，重启后可以根据日志恢复（redo）或丢弃（如果没有 commit）。

简化版（无日志）：必须在 `writei` 中立即 `bwrite`，否则数据只存在于内存缓冲区中，一断电就没了。

---

## 📊 磁盘布局

```
磁盘块编号: 0      1       2~31   32~44   45      46~999
          ┌───────┬───────┬──────┬───────┬───────┬───────────┐
          │ Boot  │ Super │ 日志  │Inode  │ 位图  │ 数据区     │
          │ Block │ Block │ (Log)│ 区    │ 块    │ (Data)     │
          └───────┴───────┴──────┴───────┴───────┴───────────┘
```

## 最大文件大小计算

```
最大文件大小 = (直接块 + 一级间接块) × 块大小
            = (12 + BSIZE/sizeof(uint)) × BSIZE
            = (12 + 256) × 1024
            = 268 KB
```
