/* virtio_disk.c — virtio 块设备驱动（Layer 0）
 *
 * 与 QEMU 的 virtio-blk 设备通过 legacy MMIO 接口通信。
 * 参考 xv6-riscv/kernel/virtio_disk.c
 */

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "buf.h"

#define R(r) ((volatile uint32 *)(VIRTIO0 + (r)))

/* virtio MMIO 控制寄存器偏移 */
#define VIRTIO_MMIO_MAGIC_VALUE         0x000
#define VIRTIO_MMIO_VERSION             0x004
#define VIRTIO_MMIO_DEVICE_ID           0x008
#define VIRTIO_MMIO_VENDOR_ID           0x00c
#define VIRTIO_MMIO_DEVICE_FEATURES     0x010
#define VIRTIO_MMIO_DRIVER_FEATURES     0x020
#define VIRTIO_MMIO_GUEST_PAGE_SIZE     0x028
#define VIRTIO_MMIO_QUEUE_SEL           0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX       0x034
#define VIRTIO_MMIO_QUEUE_NUM           0x038
#define VIRTIO_MMIO_QUEUE_ALIGN         0x03c
#define VIRTIO_MMIO_QUEUE_PFN           0x040
#define VIRTIO_MMIO_QUEUE_READY         0x044
#define VIRTIO_MMIO_QUEUE_NOTIFY        0x050
#define VIRTIO_MMIO_INTERRUPT_STATUS    0x060
#define VIRTIO_MMIO_INTERRUPT_ACK       0x064
#define VIRTIO_MMIO_STATUS              0x070

/* 状态位 */
#define VIRTIO_CONFIG_S_ACKNOWLEDGE  1
#define VIRTIO_CONFIG_S_DRIVER       2
#define VIRTIO_CONFIG_S_DRIVER_OK    4
#define VIRTIO_CONFIG_S_FEATURES_OK  8

/* 特性位 */
#define VIRTIO_BLK_F_RO              5
#define VIRTIO_BLK_F_SCSI            7
#define VIRTIO_BLK_F_CONFIG_WCE      11
#define VIRTIO_BLK_F_MQ              12
#define VIRTIO_F_ANY_LAYOUT          27
#define VIRTIO_RING_F_INDIRECT_DESC  28
#define VIRTIO_RING_F_EVENT_IDX      29

#define NUM 8

/* virtqueue 描述符 */
struct virtq_desc {
    uint64 addr;
    uint32 len;
    uint16 flags;
    uint16 next;
};
#define VRING_DESC_F_NEXT  1
#define VRING_DESC_F_WRITE 2

struct virtq_avail {
    uint16 flags;
    uint16 idx;
    uint16 ring[NUM];
    uint16 unused;
};

struct virtq_used_elem {
    uint32 id;
    uint32 len;
};

struct virtq_used {
    uint16 flags;
    uint16 idx;
    struct virtq_used_elem ring[NUM];
};

#define VIRTIO_BLK_T_IN  0
#define VIRTIO_BLK_T_OUT 1

struct virtio_blk_req {
    uint32 type;
    uint32 reserved;
    uint64 sector;
};

/* 磁盘全局状态 — 必须页对齐 */
/* 磁盘结构体必须页对齐（virtio legacy 接口要求 QUEUE_PFN 指向的地址页对齐）*/
static char disk_pages[2 * PGSIZE] __attribute__((aligned(PGSIZE)));

static struct disk {
    struct virtq_desc *desc;
    struct virtq_avail *avail;
    struct virtq_used *used;

    char free[NUM];
    uint16 used_idx;

    struct {
        struct buf *b;
        char status;
    } info[NUM];

    struct virtio_blk_req ops[NUM];
    struct spinlock vdisk_lock;
} disk;

static int alloc_desc(void) {
    for (int i = 0; i < NUM; i++) {
        if (disk.free[i]) {
            disk.free[i] = 0;
            return i;
        }
    }
    return -1;
}

static void free_desc(int i) {
    if (disk.free[i])
        panic("free_desc");
    disk.desc[i].addr = 0;
    disk.desc[i].len = 0;
    disk.desc[i].flags = 0;
    disk.desc[i].next = 0;
    disk.free[i] = 1;
}

static void free_chain(int i) {
    while (1) {
        int flag = disk.desc[i].flags;
        int nxt = disk.desc[i].next;
        free_desc(i);
        if (flag & VRING_DESC_F_NEXT)
            i = nxt;
        else
            break;
    }
}

static int alloc3_desc(int *idx) {
    for (int i = 0; i < 3; i++) {
        idx[i] = alloc_desc();
        if (idx[i] < 0) {
            for (int j = 0; j < i; j++)
                free_desc(idx[j]);
            return -1;
        }
    }
    return 0;
}

void virtio_disk_init(void) {
    uint32 status = 0;

    /* 验证设备 */
    if (*R(VIRTIO_MMIO_MAGIC_VALUE) != 0x74726976 ||
        *R(VIRTIO_MMIO_VERSION) != 1 ||
        *R(VIRTIO_MMIO_DEVICE_ID) != 2 ||
        *R(VIRTIO_MMIO_VENDOR_ID) != 0x554d4551) {
        panic("could not find virtio disk");
    }

    /* Acknowledge + Driver */
    status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;
    *R(VIRTIO_MMIO_STATUS) = status;
    status |= VIRTIO_CONFIG_S_DRIVER;
    *R(VIRTIO_MMIO_STATUS) = status;

    /* 协商特性 */
    uint64 features = *R(VIRTIO_MMIO_DEVICE_FEATURES);
    features &= ~(1 << VIRTIO_BLK_F_RO);
    features &= ~(1 << VIRTIO_BLK_F_SCSI);
    features &= ~(1 << VIRTIO_BLK_F_CONFIG_WCE);
    features &= ~(1 << VIRTIO_BLK_F_MQ);
    features &= ~(1 << VIRTIO_F_ANY_LAYOUT);
    features &= ~(1 << VIRTIO_RING_F_EVENT_IDX);
    features &= ~(1 << VIRTIO_RING_F_INDIRECT_DESC);
    *R(VIRTIO_MMIO_DRIVER_FEATURES) = features;

    /* FEATURES_OK */
    status |= VIRTIO_CONFIG_S_FEATURES_OK;
    *R(VIRTIO_MMIO_STATUS) = status;

    /* DRIVER_OK */
    status |= VIRTIO_CONFIG_S_DRIVER_OK;
    *R(VIRTIO_MMIO_STATUS) = status;

    /* 页大小 */
    *R(VIRTIO_MMIO_GUEST_PAGE_SIZE) = PGSIZE;

    /* 初始化队列 0 */
    *R(VIRTIO_MMIO_QUEUE_SEL) = 0;
    uint32 max = *R(VIRTIO_MMIO_QUEUE_NUM_MAX);
    if (max == 0)
        panic("virtio disk has no queue 0");
    if (max < NUM)
        panic("virtio disk max queue too short");
    *R(VIRTIO_MMIO_QUEUE_NUM) = NUM;
    memset(disk_pages, 0, sizeof(disk_pages));
    *R(VIRTIO_MMIO_QUEUE_PFN) = ((uint64)disk_pages) >> PGSHIFT;

    /* desc = pages; avail = desc + NUM*sizeof(desc); used = pages + PGSIZE */
    disk.desc = (struct virtq_desc *)disk_pages;
    disk.avail = (struct virtq_avail *)(disk_pages + NUM * sizeof(struct virtq_desc));
    disk.used = (struct virtq_used *)(disk_pages + PGSIZE);

    for (int i = 0; i < NUM; i++)
        disk.free[i] = 1;

    printf("virtio disk: initialized\n");
}

void virtio_disk_rw(struct buf *b, int write) {
    uint64 sector = b->blockno * (BSIZE / 512);

    acquire(&disk.vdisk_lock);

    int idx[3];
    while (alloc3_desc(idx) < 0)
        ;

    struct virtio_blk_req *buf0 = &disk.ops[idx[0]];
    if (write)
        buf0->type = VIRTIO_BLK_T_OUT;
    else
        buf0->type = VIRTIO_BLK_T_IN;
    buf0->reserved = 0;
    buf0->sector = sector;

    disk.desc[idx[0]].addr = (uint64)buf0;
    disk.desc[idx[0]].len = sizeof(struct virtio_blk_req);
    disk.desc[idx[0]].flags = VRING_DESC_F_NEXT;
    disk.desc[idx[0]].next = idx[1];

    disk.desc[idx[1]].addr = (uint64)b->data;
    disk.desc[idx[1]].len = BSIZE;
    if (write)
        disk.desc[idx[1]].flags = 0;
    else
        disk.desc[idx[1]].flags = VRING_DESC_F_WRITE;
    disk.desc[idx[1]].flags |= VRING_DESC_F_NEXT;
    disk.desc[idx[1]].next = idx[2];

    disk.info[idx[0]].status = 0xff;
    disk.desc[idx[2]].addr = (uint64)&disk.info[idx[0]].status;
    disk.desc[idx[2]].len = 1;
    disk.desc[idx[2]].flags = VRING_DESC_F_WRITE;
    disk.desc[idx[2]].next = 0;

    b->disk = 1;
    disk.info[idx[0]].b = b;

    disk.avail->ring[disk.avail->idx % NUM] = idx[0];
    __sync_synchronize();
    disk.avail->idx += 1;
    __sync_synchronize();

    *R(VIRTIO_MMIO_QUEUE_NOTIFY) = 0;

    /* 轮询等待设备完成 */
    while (disk.used_idx == disk.used->idx)
    __sync_synchronize();
    int id = disk.used->ring[disk.used_idx % NUM].id;
    if (disk.info[id].status != 0)
        panic("virtio_disk_rw: io error");
    disk.used_idx++;

    b->disk = 0;
    free_chain(idx[0]);
    release(&disk.vdisk_lock);
}
