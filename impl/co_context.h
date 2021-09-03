/***********************************************
        File Name: co_context.h
        Author: Abby Cin
        Mail: abbytsing@gmail.com
        Created Time: 10/24/20 2:43 PM
***********************************************/

#ifndef _CONET_CONTEXT_H_
#define _CONET_CONTEXT_H_

namespace conet
{
  class acceptor;
  class connection;
  class signals;

  class context
  {
  public:
    using co_timer = detail::timer_queue;

    context() : stop_{true}, efd_{-1}, revs_{1}, conn_{}, sig_{-1, nullptr}, timer_{}
    {
      conn_.resize(5);
      efd_ = ::epoll_create1(EPOLL_CLOEXEC);
    }
    context(context&& r) noexcept
        : stop_{r.stop_}, efd_{r.efd_}, revs_{std::move(r.revs_)}, conn_{std::move(r.conn_)}, sig_{std::move(r.sig_)},
          timer_{std::move(r.timer_)}
    {
      r.efd_ = -1;
      r.stop_ = true;
    }
    ~context() { cleanup(); }

    context& operator=(const context&) = delete;
    context& operator=(context&& r) = delete;

    int run()
    {
      stop_ = false;
      int res = 0;
      while(!stop_)
      {
        res = run_once();
        if(res < 0) { return res; }
      }
      return 0;
    }

    void stop() { stop_ = true; }

    int run_once()
    {
      int res = ::epoll_wait(efd_, revs_.data(), revs_.size(), timer_.empty() ? -1 : 1);
      if(res < 0) { return res; }
      timer_.tick();
      for(int i = 0; i < res; ++i)
      {
        auto ev = revs_[i].events;
        if((ev & EPOLLIN) || (ev & EPOLLOUT) || (ev & EPOLLPRI) || (ev & EPOLLHUP) || (ev & EPOLLERR))
        {
          if(revs_[i].data.fd == sig_.first)
          {
            sig_.second();
            continue;
          }
          conn_[revs_[i].data.fd].co.resume();
        }
      }
      return 0;
    }

    [[nodiscard]] bool running() const { return !stop_; }

    co_timer& timer() { return timer_; }

  private:
    struct target
    {
      target() : ev{-1}, co{nullptr} {}
      int ev;
      std::coroutine_handle<> co;
    };

    bool stop_;
    int efd_;
    std::vector<epoll_event> revs_;
    std::vector<target> conn_;
    std::pair<int, std::function<void()>> sig_;
    detail::timer_queue timer_;

    friend acceptor;
    friend connection;
    friend signals;

    void cleanup()
    {
      if(efd_ > 0)
      {
        ::close(efd_);
        efd_ = -1;
      }
      conn_.clear();
    }

    int trap(int fd, std::function<void()>&& cb)
    {
      epoll_event ev{};
      ev.data.fd = fd;
      ev.events = EPOLLIN;
      if(auto r = ::epoll_ctl(efd_, EPOLL_CTL_ADD, fd, &ev); r < 0) { return r; }
      sig_.first = fd;
      sig_.second = std::move(cb);
      return 0;
    }

    // NOTE: it's not necessary to remove from conn_, since epoll will automatically remove closed fd from interest set
    // and new peer will use the lowest fd number
    int push(int fd, const std::coroutine_handle<>& co, bool is_read = true)
    {
      if(fd < 0) { return -1; }
      if(conn_.size() <= static_cast<size_t>(fd)) { conn_.resize(conn_.size() * 2); }
      auto& target = conn_[fd];
      int op = is_read ? EPOLLIN : EPOLLOUT;
      if(target.ev == op)
      {
        target.co = co;
        return 0;
      }
      epoll_event ev{};
      ev.data.fd = fd;
      ev.events = op;
      if(auto r = ::epoll_ctl(efd_, target.ev > 0 ? EPOLL_CTL_MOD : EPOLL_CTL_ADD, fd, &ev); r < 0) { return r; }
      if(conn_.size() + 1 == revs_.size()) { revs_.resize(revs_.size() * 2); }
      target.ev = op;
      target.co = co;
      return 0;
    }

    // un-initialize for reusing
    void pop(int fd) { conn_[fd].ev = -1; }
  };
}

#endif //_CONET_CONTEXT_H_
