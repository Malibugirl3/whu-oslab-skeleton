# Makefile — 构建系统（Lab1 完成后即可使用）
#
# 使用方法：
#   make          # 编译内核
#   make run      # 编译并在 QEMU 中启动内核
#   make debug    # 启动 QEMU 并等待 GDB 连接（端口 1234）
#   make clean    # 清除所有编译产物
#
# 注意：随着实验推进，你需要把新增的 .c 和 .S 文件加入 SRCS 列表。

# ============================================================
# 工具链配置（无需修改）
# ============================================================
CROSS   = riscv64-unknown-elf-
CC      = $(CROSS)gcc
LD      = $(CROSS)ld
OBJDUMP = $(CROSS)objdump
OBJCOPY = $(CROSS)objcopy

# ============================================================
# 编译标志
#   -nostdlib     : 不链接任何 C 标准库（我们在裸机环境中！）
#   -fno-builtin  : 禁用编译器内置函数（如 memcpy），我们自己实现
#   -mcmodel=medany: 使用"中等任意"代码模型，支持大范围地址访问
#   -march=rv64gc : 目标架构为 64位 RISC-V，包含整数、乘除、原子、压缩指令集
#   -mabi=lp64d   : ABI：long和指针为64位，浮点使用硬件寄存器
#   -g            : 保留调试信息（GDB需要）
#   -Wall         : 开启所有警告（推荐保留，帮助发现潜在错误）
# ============================================================
CFLAGS = -nostdlib -fno-builtin -mcmodel=medany \
         -march=rv64gc -mabi=lp64d \
         -g -Wall -ffreestanding \
         -I kernel/include

# ============================================================
# TODO [Lab1-任务4]：
#   随着实验进行，将新增的源文件路径添加到 SRCS 列表。
#
#   Lab1 完成后，SRCS 应包含（去掉下面的注释符号 #）：
#     kernel/boot/entry.S
#     kernel/driver/uart.c
#     kernel/boot/main.c
#
#   Lab2 完成后，追加：
#     kernel/driver/console.c
#
#   Lab3 完成后，追加：
#     kernel/mm/kalloc.c
#     kernel/mm/vm.c
#
#   Lab4 完成后，追加：
#     kernel/boot/start.c
#     kernel/trap/kernelvec.S
#     kernel/trap/trap.c
#
#   Lab5 完成后，追加：
#     kernel/proc/proc.c
#     kernel/proc/swtch.S
#
#   Lab6 完成后，追加：
#     kernel/syscall/syscall.c
#     kernel/syscall/sysproc.c
#
#   Lab7 完成后，追加：
#     kernel/fs/bio.c
#     kernel/fs/fs.c
# ============================================================
SRCS = \
    kernel/boot/entry.S \
    kernel/driver/uart.c \
    kernel/boot/main.c \
    kernel/driver/console.c \
    kernel/mm/kalloc.c \
    kernel/mm/vm.c \
    kernel/boot/start.c \
    kernel/trap/kernelvec.S \
    kernel/trap/trap.c \
    kernel/lib/string.c \
    kernel/proc/proc.c \
    kernel/proc/swtch.S 

KERNEL  = kernel.elf
LDSCRIPT = kernel.ld

# ============================================================
# 构建目标
# ============================================================
all: $(KERNEL)

$(KERNEL): $(SRCS) $(LDSCRIPT)
	$(CC) $(CFLAGS) -T $(LDSCRIPT) $(SRCS) -o $@
	@echo "======================================"
	@echo " 内核编译成功：$(KERNEL)"
	@echo " 现在运行 'make run' 启动 QEMU"
	@echo "======================================"

# 在 QEMU 中运行内核
run: $(KERNEL)
	qemu-system-riscv64 \
	    -machine virt \
	    -bios none \
	    -kernel $(KERNEL) \
	    -nographic
	# 退出 QEMU：按 Ctrl+A，然后按 X

# 启动 QEMU 并暂停，等待 GDB 连接（调试模式）
debug: $(KERNEL)
	killall -q qemu-system-riscv64 || true
	qemu-system-riscv64 \
	    -machine virt \
	    -bios none \
	    -kernel $(KERNEL) \
	    -nographic \
	    -s -S
	@echo ""
	@echo "QEMU 已暂停，等待 GDB 连接..."
	@echo "在新终端中运行："
	@echo "  gdb-multiarch $(KERNEL)"
	@echo "  (gdb) target remote :1234"
	@echo "  (gdb) break _entry"
	@echo "  (gdb) continue"
	@echo ""

# 清除编译产物
clean:
	rm -f $(KERNEL) *.o *.d

.PHONY: all run debug clean
