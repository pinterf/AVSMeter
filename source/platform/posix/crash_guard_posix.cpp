/*
  This file is part of AVSMeter, (c) 2026 pinterf.

  AVSMeter is free software. You can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation, either
  version 3 of the License, or any later version.

  AVSMeter is distributed in the hope that it will be useful
  but WITHOUT ANY WARRANTY and without the implied warranty
  of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with AVSMeter. If not, see <http://www.gnu.org/licenses/>.
*/


#include "platform/crash_guard.h"
#include "platform/posix/console_posix.h"

#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstring>

#include <fcntl.h>
#include <ucontext.h>
#include <unistd.h>
#if defined(__linux__)
#include <sys/syscall.h>
#endif


// A fault can't be turned into an exception reliably (plugin code, no unwind info at the
// faulting instruction), so unlike Windows the process ends: a report on stderr, then the
// signal is raised again with its default action (exit status 128+signal, core dump if enabled).
// The handlers are process-wide, so faults in worker threads (Prefetch) are reported too.
// Only async-signal-safe calls in the handler: no malloc, no stdio, no dladdr.
namespace
{
  const int kSignals[] = { SIGSEGV, SIGBUS, SIGFPE, SIGILL, SIGABRT };


  class Writer
  {
  public:
    void Str(const char *psz)
    {
      while (*psz && m_iLen < (int)sizeof(m_szBuf))
        m_szBuf[m_iLen++] = *psz++;
    }

    void Hex(uintptr_t u)
    {
      char sz[2 + 2 * sizeof(u) + 1];
      int i = (int)sizeof(sz) - 1;
      sz[i] = '\0';
      do
      {
        sz[--i] = "0123456789abcdef"[u & 0xF];
        u >>= 4;
      } while (u != 0);
      sz[--i] = 'x';
      sz[--i] = '0';
      Str(sz + i);
    }

    void Dec(unsigned u)
    {
      char sz[12];
      int i = (int)sizeof(sz) - 1;
      sz[i] = '\0';
      do
      {
        sz[--i] = (char)('0' + u % 10);
        u /= 10;
      } while (u != 0);
      Str(sz + i);
    }

    void Flush()
    {
      const char *p = m_szBuf;
      while (m_iLen > 0)
      {
        ssize_t n = write(STDERR_FILENO, p, (size_t)m_iLen);
        if (n <= 0)
          break;
        p += n;
        m_iLen -= (int)n;
      }
      m_iLen = 0;
    }

  private:
    char m_szBuf[2048];
    int  m_iLen = 0;
  };


  const char *SignalName(int i_signal)
  {
    switch (i_signal)
    {
      case SIGSEGV: return "SIGSEGV";
      case SIGBUS:  return "SIGBUS";
      case SIGFPE:  return "SIGFPE";
      case SIGILL:  return "SIGILL";
      case SIGABRT: return "SIGABRT";
    }
    return "signal";
  }


  const char *SignalReason(int i_signal, int i_code)
  {
    switch (i_signal)
    {
      case SIGSEGV:
        if (i_code == SEGV_MAPERR) return "Access violation (address not mapped)";
        if (i_code == SEGV_ACCERR) return "Access violation (no permission)";
        if (i_code == SI_KERNEL)   return "General protection fault (e.g. misaligned SSE/AVX access)";
        return "Access violation";
      case SIGBUS:
        if (i_code == BUS_ADRALN) return "Misaligned address";
        if (i_code == BUS_ADRERR) return "Nonexistent physical address";
        if (i_code == BUS_OBJERR) return "Object-specific hardware error";
        return "Bus error";
      case SIGFPE:
        if (i_code == FPE_INTDIV) return "Integer divide by zero";
        if (i_code == FPE_INTOVF) return "Integer overflow";
        if (i_code == FPE_FLTDIV) return "Floating-point divide by zero";
        if (i_code == FPE_FLTOVF) return "Floating-point overflow";
        if (i_code == FPE_FLTUND) return "Floating-point underflow";
        if (i_code == FPE_FLTRES) return "Floating-point inexact result";
        if (i_code == FPE_FLTINV) return "Floating-point invalid operation";
        return "Arithmetic exception";
      case SIGILL:
        if (i_code == ILL_ILLOPC || i_code == ILL_ILLOPN) return "Illegal instruction (e.g. instruction set not supported by this CPU)";
        if (i_code == ILL_PRVOPC) return "Privileged instruction";
        return "Illegal instruction";
      case SIGABRT:
        return "Aborted (e.g. heap corruption detected by malloc/free, unhandled C++ exception)";
    }
    return "";
  }


  uintptr_t InstructionAddress(void *p_context)
  {
    const ucontext_t *pUc = (const ucontext_t *)p_context;
    if (!pUc)
      return 0;
#if defined(__x86_64__)
    return (uintptr_t)pUc->uc_mcontext.gregs[REG_RIP];
#elif defined(__i386__)
    return (uintptr_t)pUc->uc_mcontext.gregs[REG_EIP];
#elif defined(__aarch64__)
    return (uintptr_t)pUc->uc_mcontext.pc;
#else
    return 0;
#endif
  }


  uintptr_t ParseHex(const char *&p)
  {
    uintptr_t u = 0;
    for (;; p++)
    {
      char c = *p;
      if (c >= '0' && c <= '9')      u = (u << 4) | (uintptr_t)(c - '0');
      else if (c >= 'a' && c <= 'f') u = (u << 4) | (uintptr_t)(c - 'a' + 10);
      else break;
    }
    return u;
  }


  // Path of the mapping in /proc/self/maps that contains u_address ("start-end perms offset dev inode path"),
  // and the address as a file offset in it (for addr2line).
  bool FindModule(uintptr_t u_address, char *psz_module, size_t ui_size, uintptr_t &u_file_offset)
  {
    int iFd = open("/proc/self/maps", O_RDONLY | O_CLOEXEC);
    if (iFd < 0)
      return false;

    char szBuf[4096];
    char szLine[1024];
    size_t uiLine = 0;
    bool bFound = false;
    ssize_t n;
    while (!bFound && (n = read(iFd, szBuf, sizeof(szBuf))) > 0)
    {
      for (ssize_t i = 0; i < n && !bFound; i++)
      {
        if (szBuf[i] != '\n')
        {
          if (uiLine < sizeof(szLine) - 1)
            szLine[uiLine++] = szBuf[i];
          continue;
        }
        szLine[uiLine] = '\0';
        uiLine = 0;

        const char *p = szLine;
        uintptr_t uStart = ParseHex(p);
        if (*p++ != '-')
          continue;
        uintptr_t uEnd = ParseHex(p);
        if (u_address < uStart || u_address >= uEnd)
          continue;

        bFound = true;
        for (int iField = 0; iField < 4 && *p; iField++) // perms offset dev inode
        {
          while (*p == ' ') p++;
          if (iField == 1)
            u_file_offset = u_address - uStart + ParseHex(p);
          while (*p && *p != ' ') p++;
        }
        while (*p == ' ') p++;
        size_t uiLen = strlen(p);
        if (uiLen >= ui_size)
          uiLen = ui_size - 1;
        memcpy(psz_module, p, uiLen);
        psz_module[uiLen] = '\0';
      }
    }
    close(iFd);
    return bFound && psz_module[0] != '\0';
  }


  void CrashHandler(int i_signal, siginfo_t *p_info, void *p_context)
  {
    int iSavedErrno = errno;
    platform::posix::RestoreConsoleInput();

    uintptr_t uPC = InstructionAddress(p_context);
    char szModule[512];
    uintptr_t uFileOffset = 0;
    bool bModule = (uPC != 0) && FindModule(uPC, szModule, sizeof(szModule), uFileOffset);
    if (!bModule)
      memcpy(szModule, "Cannot determine module", sizeof("Cannot determine module"));

    Writer w;
    w.Str("\n\nFatal error: ");
    w.Str(SignalName(i_signal));
    w.Str(" [");
    w.Str(SignalReason(i_signal, p_info ? p_info->si_code : 0));
    w.Str("]\nModule:   ");
    w.Str(szModule);
    if (uPC != 0)
    {
      w.Str("\nAddress:  ");
      w.Hex(uPC);
      if (bModule && szModule[0] == '/')
      {
        w.Str(" (file offset ");
        w.Hex(uFileOffset);
        w.Str(")");
      }
    }
    if (p_info && (i_signal == SIGSEGV || i_signal == SIGBUS) && p_info->si_code != SI_KERNEL)
    {
      w.Str("\nAccessed: ");
      w.Hex((uintptr_t)p_info->si_addr);
    }
#if defined(__linux__)
    w.Str("\nThread:   ");
    w.Dec((unsigned)syscall(SYS_gettid));
#endif
    w.Str("\n");
    w.Flush();

    // SA_RESETHAND restored the default action: re-raise it (a hardware fault would also
    // recur on return, raise() makes it explicit and covers SIGABRT)
    errno = iSavedErrno;
    raise(i_signal);
  }


  alignas(16) char g_AltStack[64 * 1024];
  std::atomic<bool> g_bInstalled{ false };
}


void platform::TranslateCrashes()
{
  if (g_bInstalled.exchange(true))
    return;

  // Own stack for the handler, so a stack overflow on the main thread can still be reported
  stack_t ss = {};
  ss.ss_sp = g_AltStack;
  ss.ss_size = sizeof(g_AltStack);
  sigaltstack(&ss, nullptr);

  struct sigaction sa = {};
  sa.sa_sigaction = CrashHandler;
  sa.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_RESETHAND;
  sigemptyset(&sa.sa_mask);
  for (int iSignal : kSignals)
    sigaction(iSignal, &sa, nullptr);
}
