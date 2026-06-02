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
XXD     = xxd

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

USER_CFLAGS = -nostdlib -fno-builtin -ffreestanding \
              -fno-asynchronous-unwind-tables \
              -mcmodel=medany -march=rv64gc -mabi=lp64d \
              -I user -I kernel/include
			

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
    kernel/proc/swtch.S \
	kernel/syscall/syscall.c \
	kernel/syscall/sysproc.c \
	kernel/syscall/sysfile.c \
	kernel/sync/spinlock.c \
	kernel/sync/sleeplock.c \
	kernel/fs/bio.c \
	kernel/fs/fs.c \
	kernel/fs/file.c \
	kernel/driver/virtio_disk.c \

KERNEL  = kernel.elf
LDSCRIPT = kernel.ld

USER_INIT   = user/initcode
USER_ELF    = $(USER_INIT).elf
USER_BIN    = $(USER_INIT).bin
INITCODE_H  = kernel/proc/initcode.h
FSIMG       = fs.img
MKFS        = mkfs

# ============================================================
# 构建目标
# ============================================================
all: $(KERNEL)

$(USER_ELF): user/init.c user/ulib.c user/usys.S user/user.h
	$(CC) $(USER_CFLAGS) -Ttext 0 -e main user/init.c user/ulib.c user/usys.S -o $@
	@echo "======================================"
	@echo " 用户程序编译成功：$(USER_ELF)"
	@echo "======================================"

$(USER_BIN): $(USER_ELF)
	$(OBJCOPY) -S -O binary $(USER_ELF) $(USER_BIN)
	@echo "======================================"
	@echo " 用户程序二进制生成成功：$(USER_BIN)"
	@echo "======================================"

$(INITCODE_H): $(USER_BIN)
	$(XXD) -i $(USER_BIN) > $(INITCODE_H)
	@echo "======================================"
	@echo " 初始化代码头文件生成成功：$(INITCODE_H)"
	@echo "======================================"

# 编译 mkfs 工具（主机上运行，非 RISC-V）
$(MKFS): mkfs.c
	gcc -o $(MKFS) mkfs.c
	@echo "mkfs 工具编译成功"

# 生成磁盘镜像
$(FSIMG): $(MKFS)
	./$(MKFS) $(FSIMG)
	@echo "磁盘镜像生成成功：$(FSIMG)"

$(KERNEL): $(INITCODE_H) $(SRCS) $(LDSCRIPT)
	$(CC) $(CFLAGS) -T $(LDSCRIPT) $(SRCS) -o $@
	@echo "======================================"
	@echo " 内核编译成功：$(KERNEL)"
	@echo " 现在运行 'make run' 启动 QEMU"
	@echo "======================================"

# 在 QEMU 中运行内核
run: $(KERNEL) $(FSIMG)
	qemu-system-riscv64 \
	    -machine virt \
	    -bios none \
	    -kernel $(KERNEL) \
	    -nic none \
	    -drive file=$(FSIMG),if=none,format=raw,id=x0 \
	    -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0 \
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
	rm -f $(KERNEL) *.o *.d $(USER_ELF) $(USER_BIN) $(INITCODE_H)

.PHONY: all run debug clean
