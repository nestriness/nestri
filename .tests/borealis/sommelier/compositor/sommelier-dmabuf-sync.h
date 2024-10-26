// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_SOMMELIER_COMPOSITOR_SOMMELIER_DMABUF_SYNC_H_
#define VM_TOOLS_SOMMELIER_COMPOSITOR_SOMMELIER_DMABUF_SYNC_H_

#include <linux/dma-buf.h>

// TODO(b/189505947): DMA_BUF_IOCTL_EXPORT_SYNC_FILE might not exist, in
// linux-headers yet. It has been upstreamed in v6.0+.
#ifndef DMA_BUF_IOCTL_EXPORT_SYNC_FILE

struct dma_buf_export_sync_file {
  __u32 flags;
  __s32 fd;
};

#define DMA_BUF_IOCTL_EXPORT_SYNC_FILE \
  _IOWR(DMA_BUF_BASE, 2, struct dma_buf_export_sync_file)
#endif  // DMA_BUF_IOCTL_EXPORT_SYNC_FILE

// return 0 for success or errno
int sl_dmabuf_get_read_sync_file(int dmabuf_fd, int& sync_file_fd);
bool sl_dmabuf_sync_is_virtgpu(int sync_file_fd);
void sl_dmabuf_sync_wait(int sync_file_fd);
void sl_dmabuf_sync(struct sl_context* ctx, struct sl_sync_point* sync_point);

#endif  // VM_TOOLS_SOMMELIER_COMPOSITOR_SOMMELIER_DMABUF_SYNC_H_
