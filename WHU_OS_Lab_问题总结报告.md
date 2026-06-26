# WHU OS Lab 问题总结报告（初版）

## 1. 项目概述

本项目从 RISC-V 裸机启动开始，逐步实现了内核输出、页表、中断、进程、系统调用、文件系统，最后扩展到一个可以交互使用的简化 Shell。

最终系统支持：

- QEMU 中启动内核；
- UART 输出与内核 `printf`；
- Sv39 页表；
- 时钟中断与进程调度；
- 用户态系统调用；
- `fork / exec / wait / exit`；
- 文件系统 `open / read / write / close / unlink / mkdir / chdir / fstat`；
- 当前工作目录 `cwd`；
- 用户态 Shell；
- 外部命令 `hello / echo / cat / touch / rm / mkdir / ls / pwd`；
- 重定向 `>`、`>>`、`<`；
- 管道 `|`；
- 后台执行 `&`。

本报告不重复实验手册中的基础流程，重点记录开发过程中实际遇到、实际定位并解决的问题。

---

## 2. 开发方式与版本管理

本项目不是一次性完成所有功能，而是按照实验阶段逐步推进。整体开发过程大致为：

```text
Lab1-Lab2：启动、UART、printf
Lab3-Lab4：页表、中断
Lab5-Lab6：进程、上下文切换、系统调用
Lab7：文件系统
Lab8：Shell、cwd、用户程序、重定向、管道
```

开发过程中采用 Git 分支推进，每个阶段完成后先保证可以编译运行，再继续扩展后续功能。这样做的好处是，当后续 Shell、文件系统或系统调用出现问题时，可以根据最近引入的功能快速定位问题范围。

例如 Lab8 中出现过命令参数丢失、`ls | cat` 后无法继续输入等问题。如果没有前面阶段稳定的进程、系统调用和文件系统基础，就很难判断问题究竟来自 Shell 解析、文件描述符、系统调用，还是用户程序本身。

---

## 3. Makefile 与构建流程中遇到的问题

### 3.1 新文件没有加入 Makefile

**现象：**

新增了内核文件或用户程序后，编译或运行结果没有变化，或者链接时找不到相关符号。

**原因：**

新增源文件没有加入 Makefile 的构建列表。

例如：

- 新增进程相关文件后，需要加入 `kernel/proc/proc.c`、`kernel/proc/swtch.S`；
- 新增系统调用后，需要加入 `kernel/syscall/syscall.c`、`kernel/syscall/sysproc.c`、`kernel/syscall/sysfile.c`；
- 新增管道后，需要加入 `kernel/fs/pipe.c`；
- 新增用户程序 `pwd` 后，需要加入 `USER_PROGS`；
- 修改用户程序后，需要重新生成 `fs.img`，否则 QEMU 里仍然是旧程序。

**解决：**

补齐 Makefile 中的 `SRCS` 和 `USER_PROGS`，并在修改用户程序后重新执行：

```bash
make fs.img
```

必要时执行：

```bash
make clean && make && make fs.img
```

**收获：**

Makefile 不只是编译命令集合，而是整个系统构建链路的描述。内核镜像、initcode、用户程序和文件系统镜像之间存在明确依赖关系。

---

## 4. Lab1-Lab4：启动、页表与中断阶段的问题

### 4.1 启动后完全无输出

**现象：**

QEMU 启动后没有任何输出，无法判断系统是否已经进入内核。

**原因：**

进入 S-Mode 前缺少 PMP 配置。CPU 从 M-Mode 切换到 S-Mode 后，S-Mode 没有访问物理内存的权限，导致系统无法继续正常执行。

**解决：**

在 M-Mode 初始化阶段配置 PMP，允许 S-Mode 访问物理内存，然后再执行 `mret` 切换到 S-Mode。

**收获：**

RISC-V 的启动流程不仅需要设置入口地址和栈，还需要正确处理特权级切换时的权限配置。没有 PMP 配置时，系统可能表现为“完全无输出”。

### 4.2 中断入口和开中断接线不完整

**现象：**

时钟中断不触发，或者系统没有按预期打印时钟中断信息。

**原因：**

中断相关接线不完整，包括：

- `main.c` 中缺少 `trapinithart()`；
- 缺少 `intr_on()`；
- 启动路径中 `entry.S`、`start.c`、`start_main()` 的调用关系需要统一。

**解决：**

统一启动路径：

```text
entry.S -> start() -> mret -> start_main()
```

并在进入内核主流程后设置 `stvec`，再打开 S-Mode 中断。

**收获：**

中断能否触发，不只取决于中断处理函数是否实现，还取决于 `stvec`、中断使能位和启动路径是否正确接线。

---

## 5. Lab5-Lab6：进程与系统调用阶段的问题

### 5.1 第一个用户态进程的 trapframe 同步问题

**现象：**

在实现第一个用户态进程和早期系统调用时，用户态执行 `ecall` 后，内核虽然进行了处理，但用户态并不能稳定拿到内核写回的结果。

例如内核侧已经修改了返回值寄存器，但返回用户态后用户程序看到的寄存器状态仍然不对。

**原因：**

当时对 trapframe 的作用理解还不完整。用户态陷入内核时，用户寄存器需要保存到 trapframe；内核处理完成后，也必须从 trapframe 恢复用户寄存器。

如果内核只是修改了某个临时寄存器，或者没有把修改后的值写回 `p->trapframe`，那么 `userret` 返回用户态时不会带上这个变化。

也就是说，用户态和内核态之间真正传递系统调用参数和返回值的位置不是普通 C 局部变量，而是当前进程的 trapframe。

**解决：**

统一系统调用返回路径：

- 用户态参数和系统调用号从 `trapframe->a0-a7` 读取；
- 内核返回值写入 `trapframe->a0`；
- 返回用户态时由 `userret` 从 trapframe 恢复寄存器。

**收获：**

第一个用户态进程跑起来只是第一步，更关键的是陷入和返回路径必须能正确同步寄存器状态。trapframe 是用户态和内核态之间的寄存器边界。

### 5.2 系统调用返回路径和 `epc`

**现象：**

用户程序执行系统调用后，行为异常，可能重复进入 `ecall` 或返回值不正确。

**原因：**

处理 `ecall` 后，需要让用户态 PC 跳过当前 `ecall` 指令。如果不更新 `epc`，返回用户态后会再次执行同一条 `ecall`。

同时，系统调用返回值必须写回 `trapframe->a0`，用户态才能拿到返回值。

**解决：**

在 syscall 路径中更新 `trapframe->epc`，并将系统调用返回值写回：

```c
p->trapframe->a0 = syscalls[num]();
```

**收获：**

系统调用不是普通函数调用。参数和返回值都通过 trapframe 中保存的寄存器传递。

### 5.3 `exec` 后用户程序参数错误

**现象：**

用户程序拿不到正确参数。例如 `mkdir a` 执行后，用户程序认为 `argc < 2`，输出 Usage。

**原因：**

`exec` 加载新程序后，没有正确设置新程序入口所需的寄存器参数。

RISC-V 调用约定中：

```text
a0 = argc
a1 = argv
```

如果 `exec` 没有正确设置 `trapframe->a0` 和 `trapframe->a1`，新程序的 `main(argc, argv)` 就无法收到参数。

**解决：**

在 `exec` 成功加载 ELF、构造用户栈后设置：

```c
p->trapframe->a0 = argc;
p->trapframe->a1 = argv_user;
```

**收获：**

`exec` 不只是把 ELF 加载进内存，还要准备用户栈和入口参数。否则程序虽然能启动，但参数传递会失败。

### 5.4 `trapframe` 和系统调用参数来源

**现象：**

实现 syscall 时一度不清楚系统调用号和参数应该从哪里读取。

**原因：**

用户态执行 `ecall` 前，系统调用号在 `a7`，参数在 `a0-a5`。进入内核后，这些寄存器必须通过 trapframe 保存下来，syscall 才能读取。

**解决：**

统一系统调用路径：

```text
用户态 ecall
  -> 保存寄存器到 trapframe
  -> syscall() 从 trapframe->a7 取系统调用号
  -> argint / argaddr / argstr 从 trapframe->a0-a5 取参数
  -> 返回值写回 trapframe->a0
  -> userret 恢复寄存器回用户态
```

**收获：**

系统调用的本质是用户态和内核态通过寄存器约定通信，trapframe 是这套通信的保存点。

---

## 6. Lab7：文件系统阶段的问题

### 6.1 相对路径不工作

**现象：**

早期只能较好地支持绝对路径，`touch a`、`cat file`、`mkdir a/file` 等相对路径支持不完整。

**原因：**

路径解析主要从根目录开始，没有当前工作目录的概念。

**解决：**

在进程结构中加入：

```c
struct inode *cwd;
```

然后修改 `namex()`：

- 如果路径以 `/` 开头，从根目录开始解析；
- 否则从当前进程的 `cwd` 开始解析。

同时补充：

- `fork` 时子进程继承 cwd；
- `freeproc` 时释放 cwd；
- 新增 `chdir` 系统调用；
- shell 将 `cd` 实现为内置命令。

**收获：**

相对路径不是用户态字符串拼接问题，而是内核路径解析起点的问题。`cwd` 必须是进程资源的一部分。

### 6.2 `cd ..` 和目录名匹配异常

**现象：**

实现目录后，`cd ..` 失败，目录查找偶尔异常。

**原因：**

目录项中的 `name` 是固定长度字段：

```c
char name[DIRSIZ];
```

短名字如 `"."`、`".."` 后面的字节如果没有清零，可能残留脏数据。使用普通字符串比较时会匹配失败。

**解决：**

写入目录项时按 `DIRSIZ` 清零填充，查找时使用固定长度比较：

```c
memcmp(de.name, name, DIRSIZ)
```

**收获：**

文件系统中的目录项不是普通 C 字符串，而是固定长度磁盘结构。读写和比较都要按照磁盘格式处理。

### 6.3 `ls` 需要 `fstat`

**现象：**

实现 `ls` 时，仅通过目录项只能拿到文件名和 inode 编号，无法显示文件类型和大小。

**原因：**

目录项 `dirent` 只保存：

```text
inum + name
```

文件类型、大小、链接数等信息在 inode 中。

**解决：**

补充 stat 链路：

- `stati()`：从 inode 填充 `struct stat`；
- `filestat()`：从打开文件获取 stat 并复制到用户空间；
- `sys_fstat()`：提供系统调用；
- 用户态 `ls` 对每个目录项执行 `open + fstat`。

**收获：**

目录项和 inode 各自保存的信息不同。`ls` 这种工具需要通过目录项找到 inode，再读取 inode 元信息。

### 6.4 `stati` 类型冲突

**现象：**

编译时出现 `stati` conflicting types 一类错误。

**原因：**

内核和用户态都需要使用 `struct stat`，但结构体定义没有统一放在共享 ABI 头文件中，导致声明时类型不完整或不一致。

**解决：**

抽出共享头文件：

```text
include/fsabi.h
```

将 `struct stat`、`struct dirent`、文件类型常量等放入其中，让内核态和用户态共同包含。

**收获：**

系统调用边界上的结构体属于 ABI，需要用户态和内核态共享一致定义，不能各自随意声明。

---

## 7. Lab8：Shell 与用户程序扩展阶段的问题

### 7.1 Shell 代码直接放入 initcode 导致体积过大

**现象：**

刚开始尝试实现 Shell 时，曾考虑让第一个用户程序直接包含 Shell 逻辑。但随着 Shell 解析、命令分发、用户库等代码加入，initcode 体积迅速变大，超过了最初只映射一页用户代码的设计，导致启动失败或加载异常。

**原因：**

早期 `userinit` 加载的是一个很小的 initcode，设计上更适合作为“第一个用户态引导程序”，而不是直接承载完整 Shell。

Shell 本身需要：

- 输入循环；
- 命令解析；
- 内置命令；
- 外部命令执行；
- 用户库函数；
- 后续还要支持重定向和管道。

这些内容放入 initcode 后会超出早期一页用户空间的限制。

**解决：**

将 initcode 简化为一个很小的 init 程序，只负责：

```c
exec("/sh", argv);
```

真正的 Shell 作为文件系统中的普通 ELF 用户程序 `/sh` 存在，由 `exec` 从文件系统加载。

最终启动链路变为：

```text
kernel userinit
  -> tiny init
  -> exec("/sh")
  -> shell 主循环
```

**收获：**

initcode 应保持足够小，只负责引导第一个真正的用户程序。复杂用户程序应该作为 ELF 文件放入文件系统，由 `exec` 动态加载。这也让后续添加 `echo / cat / ls / mkdir` 等用户程序成为可能。

### 7.2 UART 双路径读取导致命令丢字符

**现象：**

Shell 中输入命令时出现异常：

```text
mkdir a -> mdir
exec failed: /c
exec failed: /cda
```

**原因：**

UART 接收中断 handler 和 `sys_read` 轮询同时读取串口。某些字符被中断路径提前读走并回显，但没有进入 shell 的 line 缓冲区，导致命令缺字。

**解决：**

当前 Shell 使用 `sys_read` 轮询输入，因此关闭 UART 接收中断，避免中断路径和轮询路径同时读 UART。

**收获：**

输入系统只能有一个明确的数据入口。中断输入和轮询输入不能同时消费同一个 UART 数据源。

### 7.3 `cat` 无法配合输入重定向和管道

**现象：**

执行：

```text
cat < out
ls | cat
```

一开始会输出：

```text
Usage: cat file...
```

**原因：**

Shell 已经正确把 fd 0 接到了文件或管道上，但 `cat` 程序在无参数时直接报 Usage，没有从标准输入读取。

**解决：**

修改 `cat`：

- 有文件参数时，读取指定文件；
- 无参数时，从 fd 0 读取。

这样：

```text
cat < out
ls | cat
```

都可以正常工作。

**收获：**

重定向和管道不仅要求 shell 改 fd，还要求用户程序遵守 Unix 风格约定：无参数时从 stdin 读取。

### 7.4 `ls | cat` 后 Shell 无法正常输入

**现象：**

执行：

```text
ls | cat
```

输出结束后，后续输入异常，命令像是被残留的 `cat` 吃掉。

**原因：**

`pipe()` 分配 fd 时最初从 0 开始，可能拿到 fd 0/1。

但本系统中：

```text
fd 0 没有 ofile 时 fallback 到 UART 输入
fd 1 没有 ofile 时 fallback 到 UART 输出
```

如果 pipe 占用了 0/1，再经过 `dup2` 和 `close`，会破坏 stdin/stdout 的语义，使 `cat` 重新从 UART 读，吞掉后续用户输入。

**解决：**

将 pipe 普通 fd 分配改成从 2 开始：

```c
for (int i = 2; i < NOFILE; i++)
```

fd 0/1 只允许通过 `dup2` 主动覆盖，用于实现重定向和管道连接。

**收获：**

文件描述符虽然只是整数，但 0/1 具有标准输入输出约定。设计 fd 分配策略时必须避开这些特殊语义。

### 7.5 重定向和管道统一执行框架

**现象：**

Shell 需要同时支持：

```text
普通命令
>
>>
<
|
&
```

如果为每种情况单独写执行逻辑，代码会很混乱。

**解决：**

Shell 先检查命令中是否存在：

```text
| > < &
```

如果存在，就进入统一的 `cmd_run_pipeline()` 执行框架。

注意：

```text
echo hi > out
```

虽然也走 `cmd_run_pipeline()`，但它只有一个命令：

```text
pl->ncmds = 1
```

因此不会真正创建 pipe，只会执行：

```text
open + dup2(fd, 1) + exec
```

而：

```text
ls | cat
```

有两个命令：

```text
pl->ncmds = 2
```

因此会创建一根 pipe，并把左边 stdout 接到写端，右边 stdin 接到读端。

**收获：**

`>` 并不是 pipe，但可以复用 pipeline 的执行框架。真正是否创建 pipe，取决于命令数量是否大于 1。

### 7.6 `>` 和 `>>` 的错误输出行为

**现象：**

执行：

```text
cat nosuch > out
cat out
```

会看到：

```text
cat: cannot open nosuch
```

错误信息进入了 `out`。

**原因：**

当前系统没有独立 stderr。`uprintf` 默认写 fd 1，而 `>` 已经把 fd 1 重定向到文件。

**处理：**

这是当前实现的简化设计，不是 bug。后续可以通过实现 fd 2、stderr 和 `2>` 重定向进一步完善。

**收获：**

Unix 中 stdout 和 stderr 是不同 fd。当前系统只完整处理了 fd 0/1，因此错误输出也会被普通输出重定向影响。

---

## 8. Lab8 功能完成情况

### 8.1 Shell 启动

当前启动链路为：

```text
kernel userinit
  -> user/init/init.c
  -> exec("/sh")
  -> shell 主循环
```

相比早期 init 直接执行固定用户程序，现在系统进入了交互式 Shell，可以持续读取用户输入并执行不同程序。

### 8.2 当前目录与 prompt

每个进程维护：

```c
struct inode *cwd;
char cwdpath[MAXPATH];
```

其中：

- `cwd` 用于内核路径解析；
- `cwdpath` 用于 `pwd` 和 shell prompt 显示。

示例：

```text
/$ mkdir a
/$ cd a
/a$ pwd
/a
```

### 8.3 重定向

支持：

```text
echo hello > out
echo world >> out
cat < out
```

核心机制：

```text
open 文件
dup2(fd, 0 或 1)
close(fd)
exec 用户程序
```

`exec` 会替换用户程序地址空间，但保留 fd 表，因此新程序继承重定向效果。

### 8.4 管道

支持：

```text
ls | cat
echo hello | cat
ls | cat > out
```

核心机制：

```text
pipe 创建读端和写端
左边命令 dup2(write_end, 1)
右边命令 dup2(read_end, 0)
exec 后程序仍然 read(0) / write(1)
```

管道本质是内核中的临时缓冲区，不是磁盘文件，不会被 `ls` 看到。

---

## 9. 演示命令

可以用以下命令进行功能演示：

```text
ls
mkdir a
cd a
pwd

echo hello > out
cat out

echo world >> out
cat out

cat < out

ls | cat

ls | cat > list
cat list

cat nosuch > err
cat err
```

这些命令覆盖：

- 文件创建；
- 当前目录；
- 输出重定向；
- 追加重定向；
- 输入重定向；
- 管道；
- 管道和重定向组合；
- 当前 stderr 简化设计。

---

## 10. 总结与不足

本项目最终从裸机启动扩展到了一个具备交互能力的小型操作系统。开发过程中最重要的收获不是单个函数的实现，而是理解了各模块之间的连接关系：

```text
trapframe 连接用户态和系统调用
proc 保存进程资源
ofile 统一管理文件描述符
inode 表示文件系统对象
exec 替换用户程序但保留 fd/cwd
dup2 改变 fd 指向
pipe 用临时 buffer 连接两个进程
shell 将这些机制组合成用户可见命令
```

当前系统仍有一些简化：

- 没有独立 stderr；
- 没有 `2>`；
- 没有环境变量和 PATH 搜索；
- 不支持引号、通配符、命令历史；
- 管道等待使用 `yield` 简化实现，没有完整 sleep/wakeup；
- UART 输入采用轮询方式，没有完整终端输入缓冲。

但作为课程设计，本项目已经完成了从启动、进程、系统调用、文件系统到 Shell 的完整闭环，并通过实际调试解决了多个跨模块问题。
