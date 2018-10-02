/*
 * Copyright (C) 2017 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "core/memory_tracker/cc/memory_protections.h"

#include <functional>
#include <ucontext.h>
#include <fcntl.h>
#include <sstream>

namespace gapii {
namespace track_memory {

class PosixMemoryTracker {
  public:
    PosixMemoryTracker(std::function<bool(void*,PageProtections)> segfault_function):
      signal_handler_registered_(false),
      orig_action_{0},
      handle_segfault_(segfault_function) {
    }
  bool IsInstalled() const {
    struct sigaction orig_action = {0};
    sigaction(SIGSEGV, nullptr, &orig_action);
    return orig_action.sa_sigaction == &SegfaultHandlerFunction;
  }

protected:
  // A static wrapper of HandleSegfault() as sigaction() asks for a static
  // function.
  static void SegfaultHandlerFunction(int sig, siginfo_t *info, void *unused);

  // EnableMemoryTrackerImpl calls sigaction() to register the new segfault
  // handler to the thread (and affects all the child threads), stores the
  // original segfault handler. This method sets the static pointer:
  // unique_tracker to |this| pointer. The signal handler will not be set again
  // if the signal handler has already been set by the same memory tracker
  // instance before, all following calls to this function will just return
  // true.
  bool inline EnableMemoryTrackerImpl();

  // DisableMemoryTrackerImpl recovers the original segfault signal
  // handler. Returns true if the handler is recovered successfully,
  // othwerwise returns false.
  bool inline DisableMemoryTrackerImpl();

  struct trackable_memory{
    void* primary_address;
    void* secondary_address;
  };

  inline trackable_memory AllocateTrackableMemory(size_t length);

  inline void FreeTrackableMemory(void* ptr, void* secondary, size_t length);
  private:
    bool signal_handler_registered_;  // A flag to indicate whether the signal
                                      // handler has been registered
    struct sigaction orig_action_;    // The original signal action for SIGSEGV
    std::function<bool(void*, PageProtections)> handle_segfault_; // The function to call on a segfault
};

typedef MemoryTrackerImpl<PosixMemoryTracker> MemoryTracker;
extern PosixMemoryTracker *unique_tracker;

void inline PosixMemoryTracker::SegfaultHandlerFunction(int sig,
                                                        siginfo_t *info,
                                                        void *unused) {
  if (unique_tracker) {
    PageProtections fault_type = PageProtections::kRead | PageProtections::kWrite;
    ucontext_t* ctx = (ucontext_t*)(unused);

    #if defined(__x86_64) || defined(__i386)
      #if defined(__x86_64)
        const size_t reg_err = 19;
      #else
        const size_t reg_err = 13;
      #endif

      const size_t fault_write = 1<<1;
      if (ctx->uc_mcontext.gregs[reg_err] & fault_write) {
        fault_type = PageProtections::kWrite;
      } else {
        fault_type = PageProtections::kRead;
      }
    #elif defined(__ARM_ARCH_7A__)
      const uint32_t fsr_write = 1 << 11;
      if (ctx->uc_mcontext.error_code & fsr_write) {
        fault_type = PageProtections::kWrite;
      } else {
        fault_type = PageProtections::kRead;
      }
    #elif defined(__aarch64__)
      size_t offs = 0;
      uint64_t esr_el1_write = (1 << 6)
      while (offs < (4096 - sizeof(esr_context*)) {
        _aarch64_ctx* aactx = static_cast<_aarch64_ctx*>(&ctx->uc_mcontext.__reserved[offs]);
        if (aactx->magic == ESR_MAGIC) {
          esr_context* esr = static_cast<esr_context*>(aactx);
          if (esr->esr & esr_el1_write) {
            fault_type = PageProtections::kWrite;
          } else {
            fault_type = PageProtections::kRead;
          }
          break;
        }
        if (aactx->size < 4096) {
            offs += aactx->size;
        } else {
            break;
        }
      }
    #endif

    if (!unique_tracker->handle_segfault_(info->si_addr, fault_type)) {
      #ifndef NDEBUG
        raise(SIGTRAP);
      #endif // NDEBUG
        (*unique_tracker->orig_action_.sa_sigaction)(sig, info, unused);
    }
  }
}

bool inline PosixMemoryTracker::EnableMemoryTrackerImpl() {
  if (IsInstalled()) {
    return true;
  }

  unique_tracker = this;
  struct sigaction sa {
    0
  };
  sa.sa_flags = SA_SIGINFO;
  sigemptyset(&sa.sa_mask);
  sa.sa_sigaction = &SegfaultHandlerFunction;
  return sigaction(SIGSEGV, &sa, &orig_action_) != 1;
}

bool inline PosixMemoryTracker::DisableMemoryTrackerImpl() {
  if (IsInstalled()) {
    return sigaction(SIGSEGV, &orig_action_, nullptr) != 1;
  }
  return true;
}

PosixMemoryTracker::trackable_memory PosixMemoryTracker::AllocateTrackableMemory(size_t length) {
  std::stringstream filename;
  filename << "/gapii_smem_file";
  filename << getpid();
  int fd = shm_open(filename.str().c_str(), O_RDWR | O_CREAT | O_EXCL, 0600);
  if (fd == -1) {
    return {};
  }
  shm_unlink(filename.str().c_str());
  if (ftruncate(fd, length) != 0) {
    return {};
  }

  void* mem1 = mmap(NULL, length, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
  void* mem2 = mmap(NULL, length, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
  close(fd);
  return {mem1, mem2};
}

void PosixMemoryTracker::FreeTrackableMemory(void* primary, void* secondary, size_t length) {
  munmap(primary, length);
  munmap(secondary, length);
}

} // namespace track_memory
} // namespace gapii
