#include "defs.h"
#include "param.h"
#include "proc.h"
#include "types.h"
#include "file.h"

static void pipeclose(struct pipe *pi, int writable) {
  acquire(&pi->lock);
  if (writable)
    pi->writeopen = 0;
  else
    pi->readopen = 0;
  if (pi->readopen == 0 && pi->writeopen == 0) {
    release(&pi->lock);
    kfree((void *)pi);
    return;
  }
  release(&pi->lock);
}

static int pipealloc(struct pipe **pip) {
  struct pipe *pi;

  pi = (struct pipe *)kalloc();
  if (pi == 0)
    return -1;
  initlock(&pi->lock, "pipe");
  pi->readopen = 1;
  pi->writeopen = 1;
  pi->nwrite = 0;
  pi->nread = 0;
  *pip = pi;
  return 0;
}

int piperead(struct file *f, uint64 addr, int n) {
  struct pipe *pi = f->pipe;
  int i = 0;
  struct proc *p = myproc();

  acquire(&pi->lock);
  while (i == 0 && pi->writeopen && pi->nread == pi->nwrite) {
    release(&pi->lock);
    if (p->killed)
      return -1;
    yield();
    acquire(&pi->lock);
  }
  while (i < n && pi->nread != pi->nwrite) {
    char ch = pi->data[pi->nread++ % PIPESIZE];
    if (copyout(p->pagetable, addr + i, &ch, 1) < 0) {
      release(&pi->lock);
      return -1;
    }
    i++;
  }
  release(&pi->lock);
  return i;
}

int pipewrite(struct file *f, uint64 addr, int n) {
  struct pipe *pi = f->pipe;
  int i = 0;
  struct proc *p = myproc();

  acquire(&pi->lock);
  while (i < n) {
    if (pi->readopen == 0) {
      release(&pi->lock);
      return -1;
    }
    if (pi->nwrite == pi->nread + PIPESIZE) {
      release(&pi->lock);
      if (p->killed)
        return -1;
      yield();
      acquire(&pi->lock);
      continue;
    }
    char ch;
    if (copyin(p->pagetable, &ch, addr + i, 1) < 0) {
      release(&pi->lock);
      return -1;
    }
    pi->data[pi->nwrite++ % PIPESIZE] = ch;
    i++;
  }
  release(&pi->lock);
  return i;
}

void pipefileclose(struct file *f) {
  struct pipe *pi = f->pipe;

  if (pi == 0)
    return;
  pipeclose(pi, f->writable);
  f->pipe = 0;
}

int createpipe(int fd[2]) {
  struct file *rf, *wf;
  struct pipe *pi;

  if (pipealloc(&pi) < 0)
    return -1;
  rf = filealloc();
  wf = filealloc();
  if (rf == 0 || wf == 0) {
    if (wf)
      fileclose(wf);
    if (rf)
      fileclose(rf);
    kfree((void *)pi);
    return -1;
  }
  rf->type = FD_PIPE;
  rf->pipe = pi;
  rf->readable = 1;
  rf->writable = 0;
  wf->type = FD_PIPE;
  wf->pipe = pi;
  wf->readable = 0;
  wf->writable = 1;

  fd[0] = -1;
  fd[1] = -1;
  for (int i = 2; i < NOFILE; i++) {
    if (myproc()->ofile[i] == 0) {
      myproc()->ofile[i] = rf;
      fd[0] = i;
      break;
    }
  }
  for (int i = 2; i < NOFILE; i++) {
    if (myproc()->ofile[i] == 0) {
      myproc()->ofile[i] = wf;
      fd[1] = i;
      break;
    }
  }
  if (fd[0] < 0 || fd[1] < 0) {
    if (fd[0] >= 0) {
      myproc()->ofile[fd[0]] = 0;
      fileclose(rf);
    } else {
      fileclose(rf);
    }
    if (fd[1] >= 0) {
      myproc()->ofile[fd[1]] = 0;
      fileclose(wf);
    } else {
      fileclose(wf);
    }
    return -1;
  }
  return 0;
}
