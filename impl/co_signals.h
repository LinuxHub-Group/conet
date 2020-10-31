/***********************************************
        File Name: co_signals.h
        Author: Abby Cin
        Mail: abbytsing@gmail.com
        Created Time: 10/26/20 10:18 PM
***********************************************/

#ifndef _CONET_SIGNALS_H_
#define _CONET_SIGNALS_H_

namespace conet
{
  template<typename F>
  concept co_signal_handler_cp = requires(F f, int x)
  {
    {f(x)};
  };

  class signals
  {
    static void ignore_handler(int) {}

  public:
    explicit signals(context& ctx) : fd_{-1}, set_{}, ctx_{ctx}, co_{nullptr} { ::sigemptyset(&set_); }
    ~signals()
    {
      if(fd_ > 0) { ::close(fd_); }
    }

    int trap(int num)
    {
      if(::sigaddset(&set_, num) < 0) { return -1; }
      if(::sigprocmask(SIG_BLOCK, &set_, nullptr) < 0) { return -1; }
      fd_ = ::signalfd(fd_, &set_, SFD_NONBLOCK);
      return fd_;
    }

    int ignore(int num)
    {
      sigset_t se{};
      ::sigaddset(&se, num);
      struct sigaction sa
      {
      };
      sa.sa_handler = ignore_handler;
      sa.sa_mask = se;
      sa.sa_flags = 0;

      return ::sigaction(num, &sa, nullptr);
    }

    void wait(co_signal_handler_cp auto cb)
    {
      ctx_.trap(fd_, [this, cb = std::move(cb)] { this->handle_signal(cb, ctx_, fd_); });
    }

  private:
    int fd_;
    sigset_t set_;
    context& ctx_;
    simple_co_handle_t co_;

    void handle_signal(co_signal_handler_cp auto&& cb, context& ctx, int& fd)
    {
      signalfd_siginfo trapped{};
      for(;;)
      {
        if(auto n = ::read(fd, &trapped, sizeof(trapped)); n != sizeof(trapped))
        {
          if(!ctx.running()) { break; }
          // something went wrong
          continue;
        }
        cb(trapped.ssi_signo);
        if(!ctx.running()) { break; }
      }
      //::sigprocmask(SIG_UNBLOCK, &set_, nullptr);
      ::close(fd);
      fd = -1;
    }
  };
}

#endif //_CONET_SIGNALS_H_
