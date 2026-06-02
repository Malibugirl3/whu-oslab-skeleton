#include "user.h"

int main(void) {
  int fd, r;
  char buf[64];

  uprintf("Lab7: File System Test\n");

  /* 测试1: 创建并写入文件 */
  fd = open("/hello.txt", O_CREAT | O_WRONLY);
  if (fd < 0) {
    uprintf("FAIL: open/create\n");
    exit(1);
  }
  r = write(fd, "Hello, File System!\n", 20);
  if (r != 20) {
    uprintf("FAIL: write returned %d\n", r);
    exit(1);
  }
  close(fd);
  uprintf("Test 1 passed: wrote 20 bytes\n");

  /* 测试2: 重新打开并读取 */
  fd = open("/hello.txt", O_RDONLY);
  if (fd < 0) {
    uprintf("FAIL: reopen\n");
    exit(1);
  }
  r = read(fd, buf, 64);
  if (r != 20) {
    uprintf("FAIL: read returned %d\n", r);
    exit(1);
  }
  buf[r] = 0;
  uprintf("Test 2 passed: read '%s'\n", buf);
  close(fd);

  /* 测试3: 删除文件 */
  if (unlink("/hello.txt") < 0) {
    uprintf("FAIL: unlink\n");
    exit(1);
  }
  fd = open("/hello.txt", O_RDONLY);
  if (fd >= 0) {
    uprintf("FAIL: file should be deleted\n");
    exit(1);
  }
  uprintf("Test 3 passed: file deleted\n");

  uprintf("All tests passed!\n");

  for (;;) ;  /* init 永不退出 */

  return 0;
}
