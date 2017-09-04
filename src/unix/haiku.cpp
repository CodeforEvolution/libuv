/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
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

extern "C" {
#include "uv.h"
#include "internal.h"
}

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/resource.h>
#include <time.h>
#include <stdlib.h>
#include <fcntl.h>

#include <Directory.h>
#include <Entry.h>
#include <image.h>
#include <OS.h>
#include <Path.h>
#include <String.h>

#include <private/shared/cpu_type.h>

static void* args_mem = NULL;
static char** process_argv = NULL;
static int process_argc = 0;
static char* process_title_ptr = NULL;

int uv_exepath(char* buffer, size_t* size) {
  const char* str;
  image_info info;
  int32 cookie = 0;
  size_t strSize;
    
  if (buffer == NULL || size == NULL || *size == 0)
    return -EINVAL;

  while (get_next_image_info(0, &cookie, &info) == B_OK) {
    if (info.type == B_APP_IMAGE) {
      break;
    }
  }

  BEntry entry(info.name, true);
  BPath path;
  status_t rc = entry.GetPath(&path);  /* (path) now has binary's path. */
  if (rc != B_OK)
    return -errno;
  rc = path.GetParent(&path); /* chop filename, keep directory. */
  if (rc != B_OK)
    return -errno;
  str = path.Path();

  strSize -= 1;
  *size -= 1;
	
  strSize = sizeof str;
  if (*size > strSize)
    *size = strSize;

  memcpy(buffer, str, *size);
  buffer[*size] = '\0';

  return 0;
}

uint64_t uv_get_free_memory(void) {
  uint64 free_memory;

  system_info info;
  get_system_info(&info);

  free_memory = (info.free_memory) * B_PAGE_SIZE;

  return (uint64_t)free_memory;
}


uint64_t uv_get_total_memory(void) {
  uint64 total_memory;

  system_info info;
  get_system_info(&info);

  total_memory = (info.max_pages + info.ignored_pages) * B_PAGE_SIZE;

  return (uint64_t)total_memory;
}


void uv_loadavg(double avg[3]) {
  //Does not exist on Haiku...
  avg[0] = avg[1] = avg[2] = 0;
}

int uv_resident_set_memory(size_t* rss) {
  area_info areaInfo;
  int32 cookie = 0;
  team_info teamInfo;
  thread_info threadInfo;
	
  get_thread_info(find_thread(NULL), &threadInfo);
  get_team_info(threadInfo.team, &teamInfo);
	
  while (get_next_area_info(teamInfo.team, &cookie, &areaInfo) == B_OK)
  {
    *rss += areaInfo.ram_size;
  }

  return 0;
}


int uv_uptime(double* uptime) {
  *uptime = (double)system_time() / 1000000; 

  return 0;
}


int uv_cpu_info(uv_cpu_info_t** cpu_infos, int* count) {
  BString cpuStr;  
  cpu_topology_node_info* topology = NULL;
  int numcpus;
  system_info sysInfo;
  uint32 infoCount;
  uint32 topologyNodeCount = 0;
  uv_cpu_info_t* cpu_info;
	
  get_system_info(&sysInfo);

  numcpus = (int)sysInfo.cpu_count;

  *cpu_infos = new uv_cpu_info_t[numcpus * sizeof(**cpu_infos)];
  if (!(*cpu_infos))
    return -ENOMEM;

  *count = numcpus;

  get_cpu_topology_info(NULL, &topologyNodeCount);
  if (topologyNodeCount != 0)
    topology = new cpu_topology_node_info[topologyNodeCount];
  get_cpu_topology_info(topology, &topologyNodeCount);

  enum cpu_platform platform = B_CPU_UNKNOWN;
  enum cpu_vendor cpuVendor = B_CPU_VENDOR_UNKNOWN;
  uint32 cpuModel = 0;
  for (int i = 0; i < topologyNodeCount; i++) {
    switch (topology[i].type) {
      case B_TOPOLOGY_ROOT:
        platform = topology[i].data.root.platform;
        break;
      case B_TOPOLOGY_PACKAGE:
        cpuVendor = topology[i].data.package.vendor;
		break;
      case B_TOPOLOGY_CORE:
        cpuModel = topology[i].data.core.model;
        break;

      default:
        break;
    }
  }

  delete[] topology;

  cpuStr << get_cpu_vendor_string(cpuVendor)
    << " " << get_cpu_model_string(platform, cpuVendor, cpuModel);

  for (int i = 0; i < numcpus; i++) {
    cpu_info = &(*cpu_infos)[i];

  //Can't be implemented in Haiku
  cpu_info->cpu_times.user = 0;
  cpu_info->cpu_times.nice = 0;
  cpu_info->cpu_times.sys = 0;
  cpu_info->cpu_times.idle = 0;
  cpu_info->cpu_times.irq = 0;

  cpu_info->model = uv__strdup(cpuStr.String());
  cpu_info->speed = (int)get_rounded_cpu_speed();
  }

  return 0;
}


void uv_free_cpu_info(uv_cpu_info_t* cpu_infos, int count) {
  int i;

  for (i = 0; i < count; i++) {
    delete cpu_infos[i].model;
  }

  delete[] cpu_infos;
}
