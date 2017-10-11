/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
 * Additional changes copyright © 2011-2012 Research In Motion Limited.
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "uv.h"
#include "internal.h"

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/procfs.h>
#include <sys/neutrino.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/syspage.h>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <errno.h>
#include <poll.h>

#include <unistd.h>
#include <time.h>

/* GetInterfaceAddresses */
#include <arpa/inet.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <net/if.h>

#define HAVE_IFADDRS_H 1
#ifdef __UCLIBC__
# if __UCLIBC_MAJOR__ < 0 || __UCLIBC_MINOR__ < 9 || __UCLIBC_SUBLEVEL__ < 32
#  undef HAVE_IFADDRS_H
# endif
#endif
#ifdef HAVE_IFADDRS_H
# include <ifaddrs.h>
#endif

#undef NANOSEC
#define NANOSEC ((uint64_t) 1e9)


static char *process_title;


uint64_t uv__hrtime() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (((uint64_t) ts.tv_sec) * NANOSEC + ts.tv_nsec);
}


void uv_loadavg(double avg[3]) {
  /* QNX does not implement load averages. Maybe fill in with something? */
}


struct dinfo_s {
  procfs_debuginfo info;
  char pathbuffer[_POSIX_PATH_MAX];
};


int uv_exepath(char* buffer, size_t* size) {
  if (!buffer || !size) {
    return -1;
  }

  int proc_fd, status;
  struct dinfo_s dinfo;
  char buf[_POSIX_PATH_MAX + 1];

  sprintf(buf, "/proc/%d/as", getpid());
  if ((proc_fd = open(buf, O_RDONLY)) == -1) {
    close(proc_fd);
    *size = 0;
    return -1;
  }

  memset(&dinfo, 0, sizeof(dinfo));
  status = devctl(proc_fd, DCMD_PROC_MAPDEBUG_BASE, &dinfo, sizeof(dinfo), 0);
  if (status != EOK) {
    close(proc_fd);
    *size = 0;
    return -1;
  }
  close(proc_fd);
  strncpy(buffer, dinfo.info.path, *size);
  *size = strlen(buffer);
  return 0;
}


uint64_t uv_get_free_memory(void) {
  struct stat statbuf; 
  paddr_t freemem; 
  stat("/proc", &statbuf); 
  freemem = (paddr_t)statbuf.st_size; 
  return (uint64_t) freemem;
}


uint64_t uv_get_total_memory(void) {
  char *str = SYSPAGE_ENTRY(strings)->data; 
  struct asinfo_entry *as = SYSPAGE_ENTRY(asinfo); 
  off64_t total = 0; 
  unsigned num; 

  for (num = _syspage_ptr->asinfo.entry_size / sizeof(*as); num > 0; --num) { 
    if(strcmp(&str[as->name], "ram") == 0) { 
      total += as->end - as->start + 1; 
    } 
    ++as; 
  } 

  return (uint64_t) total;
}


char** uv_setup_args(int argc, char** argv) {
  process_title = argc ? strdup(argv[0]) : NULL;
  return argv;
}


uv_err_t uv_set_process_title(const char* title) {
  /* TODO implement me */
  return uv__new_artificial_error(UV_ENOSYS);
}


uv_err_t uv_get_process_title(char* buffer, size_t size) {
  if (process_title) {
    strncpy(buffer, process_title, size);
  } else {
    if (size > 0) {
      buffer[0] = '\0';
    }
  }

  return uv_ok_;
}


uv_err_t uv_resident_set_memory(size_t* rss) {
#if 0 // TODO QNX
  struct rusage usage;
  if (!getrusage(RUSAGE_SELF, &usage)) {}
    return uv__new_sys_error(errno);
  }
  *rss = (size_t) usage.ru_maxrss * getpagesize();
#endif
  return uv_ok_;
}


uv_err_t uv_uptime(double* uptime) {
  time_t now;
  now = time(NULL);
  *uptime = (double)(now - SYSPAGE_ENTRY(qtime)->boot_time);
  return uv_ok_;
}


uv_err_t uv_cpu_info(uv_cpu_info_t** cpu_infos, int* count) {
  // TODO QNX - implement
  int num_cpus, i;
  struct cpuinfo_entry *cpu_entries, *cpu;
  uv_cpu_info_t* cpu_info;

  num_cpus = 0; // probably not true

  *cpu_infos = (uv_cpu_info_t*)malloc(1 * sizeof(uv_cpu_info_t));
  if (!(*cpu_infos)) {
    return uv__new_artificial_error(UV_ENOMEM);
  }

  *count = num_cpus;

  for (i = 0; i < num_cpus; i++) {
    cpu = cpu_entries + i;
    cpu_info = cpu_infos + i;
    // FIXME: Need to get this CPU time info
    cpu_info->cpu_times.user = 0;
    cpu_info->cpu_times.nice = 0;
    cpu_info->cpu_times.sys = 0;
    cpu_info->cpu_times.idle = 0;
    cpu_info->cpu_times.irq = 0;

    cpu_info->model = 0;
    cpu_info->speed = 0;
  }

  return uv_ok_;
}


void uv_free_cpu_info(uv_cpu_info_t* cpu_infos, int count) {
  int i;

  for (i = 0; i < count; i++) {
    free(cpu_infos[i].model);
  }

  free(cpu_infos);
}


uv_err_t uv_interface_addresses(uv_interface_address_t** addresses,
  int* count) {
#ifndef HAVE_IFADDRS_H
  return uv__new_artificial_error(UV_ENOSYS);
#else
  struct ifaddrs *addrs, *ent;
  struct sockaddr_in *in4;
  struct sockaddr_in6 *in6;
  char ip[INET6_ADDRSTRLEN];
  uv_interface_address_t* address;

  if (getifaddrs(&addrs) != 0) {
    return uv__new_sys_error(errno);
  }

  *count = 0;

  /* Count the number of interfaces */
  for (ent = addrs; ent != NULL; ent = ent->ifa_next) {
    if (!(ent->ifa_flags & IFF_UP && ent->ifa_flags & IFF_RUNNING) ||
        (ent->ifa_addr == NULL)) {
      continue;
    }

    (*count)++;
  }

  *addresses = (uv_interface_address_t*)
    malloc(*count * sizeof(uv_interface_address_t));
  if (!(*addresses)) {
    return uv__new_artificial_error(UV_ENOMEM);
  }

  address = *addresses;

  for (ent = addrs; ent != NULL; ent = ent->ifa_next) {
    bzero(&ip, sizeof (ip));
    if (!(ent->ifa_flags & IFF_UP && ent->ifa_flags & IFF_RUNNING)) {
      continue;
    }

    if (ent->ifa_addr == NULL) {
      continue;
    }

    address->name = strdup(ent->ifa_name);

    if (ent->ifa_addr->sa_family == AF_INET6) {
      address->address.address6 = *((struct sockaddr_in6 *)ent->ifa_addr);
    } else {
      address->address.address4 = *((struct sockaddr_in *)ent->ifa_addr);
    }

    address->is_internal = ent->ifa_flags & IFF_LOOPBACK ? 1 : 0;

    address++;
  }

  freeifaddrs(addrs);

  return uv_ok_;
#endif
}


void uv_free_interface_addresses(uv_interface_address_t* addresses,
  int count) {
  int i;

  for (i = 0; i < count; i++) {
    free(addresses[i].name);
  }

  free(addresses);
}

int uv__platform_loop_init(uv_loop_t* loop, int default_loop) {
  return 0;
}

void uv__platform_loop_delete(uv_loop_t* loop) {
}

void uv__io_poll(uv_loop_t* loop, int timeout) {
  ngx_queue_t* q;
  uv__io_t* w;
  int i = 0;
  int numFds = loop->nfds;
  int retFds = -1;
  uint64_t base;
  uint64_t diff;

  if (numFds == 0) {
    assert(ngx_queue_empty(&loop->watcher_queue));
    return;
  }

  assert(timeout >= -1);

  struct pollfd fds[numFds];

  ngx_queue_foreach(q, &loop->watcher_queue) {
    w = ngx_queue_data(q, uv__io_t, watcher_queue);
    assert(w->pevents != 0);
    assert(w->fd >= 0);
    assert(w->fd < (int) loop->nwatchers);

    fds[i].fd = w->fd;
    fds[i].events = w->pevents;
    i++;
  }

  base = loop->time;

  for (;;) {
    retFds = poll(fds, numFds, timeout);

    /* Update loop->time unconditionally. It's tempting to skip the update when
    * timeout == 0 (i.e. non-blocking poll) but there is no guarantee that the
    * operating system didn't reschedule our process while in the syscall.
    */
    SAVE_ERRNO(uv__update_time(loop));

    if (retFds == 0) {
      assert(timeout != -1);
      return;
    }

    if (retFds == -1) {
      if (errno != EINTR)
        abort();

      if (timeout == -1)
        continue;

      if (timeout == 0)
        return;

      /* Interrupted by a signal. Update timeout and poll again. */
      assert(timeout > 0);

      diff = loop->time - base;
      if (diff >= (uint64_t) timeout)
        return;

      timeout -= diff;
      continue;
    }

    for (i = 0; i < numFds; i++) {
      if (fds[i].revents & (POLLERR | POLLHUP | POLLIN | POLLOUT)) {
        w = loop->watchers[fds[i].fd];
        if (w) {
          w->cb(loop, w, fds[i].revents);
        }
      }
    }

    break;
  }
}

int uv_fs_event_init(uv_loop_t* loop,
                     uv_fs_event_t* handle,
                     const char* path,
                     uv_fs_event_cb cb,
                     int flags) {
// TODO QNX
  return 0;
}

void uv__fs_event_close(uv_fs_event_t* handle) {
// TODO QNX
}
