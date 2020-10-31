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

  template<typename F, typename... Args>
  concept rocket = requires(F&& f, Args&&... args)
  {
    {
      std::forward<F>(f)(std::forward<Args>(args)...)
    }
    ->std::same_as<task>;
  };

  class context
  {
  public:
    using co_timer = detail::timer_queue;

    context() : efd_{-1}, stop_{true}, revs_{1}, conn_{}, sig_{-1, nullptr}, timer_{}
    {
      efd_ = ::epoll_create1(EPOLL_CLOEXEC);
    }
    context(context&& r) noexcept
        : efd_{r.efd_}, stop_{r.stop_}, revs_{std::move(r.revs_)}, conn_{std::move(r.conn_)}, sig_{std::move(r.sig_)},
          timer_{std::move(r.timer_)}
    {
      r.efd_ = -1;
      r.stop_ = true;
    }
    ~context() { cleanup(); }

    int run()
    {
      stop_ = false;
      while(!stop_)
      {
        errno = 0;
        int res = ::epoll_wait(efd_, revs_.data(), revs_.size(), timer_.empty() ? -1 : 1);
        if(res < 0) { return res; }
        timer_.tick();
        while(!suspended_.empty())
        {
          suspended_.back().destroy();
          suspended_.pop_back();
        }
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
            auto t = conn_.find(revs_[i].data.fd);
            if(t != conn_.end()) { t->second.co.resume(); }
          }
        }
      }
      return 0;
    }

    void stop() { stop_ = true; }

    template<typename F, typename... Args>
    requires rocket<F, Args...> void launch(F&& f, Args&&... args)
    {
      auto t = std::forward<F>(f)(std::forward<Args>(args)...);
      t.set_context(this);
    }

    [[nodiscard]] bool running() const { return !stop_; }

    co_timer& timer() { return timer_; }

  private:
    struct target
    {
      target() : ev{-1}, co{nullptr} {}
      int ev;
      simple_co_handle_t co;
    };

    int efd_;
    bool stop_;
    std::vector<epoll_event> revs_;
    std::map<int, target> conn_;
    std::pair<int, std::function<void()>> sig_;
    detail::timer_queue timer_;
    std::vector<simple_co_handle_t> suspended_;

    friend acceptor;
    friend connection;
    friend signals;
    friend task;

    void cleanup()
    {
      if(efd_ > 0)
      {
        ::close(efd_);
        efd_ = -1;
      }
      for(auto& [_, v]: conn_)
      {
        if(v.ev != -1) { v.co.destroy(); }
      }
      conn_.clear();
      while(!suspended_.empty())
      {
        suspended_.back().destroy();
        suspended_.pop_back();
      }
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
    int push(int fd, simple_co_handle_t& co, bool is_read = true)
    {
      co.promise().set_context(this); // promise maybe differ to task::set_context
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
    void pop(int fd)
    {
      auto iter = conn_.find(fd);
      if(iter != conn_.end()) { iter->second.ev = -1; }
    }
    void recycle(simple_co_handle_t& co) { suspended_.push_back(co); }
  };

  std::suspend_always task::promise_type::final_suspend()
  {
    if(ctx_) { ctx_->recycle(co_); }
    return {};
  }
}

#endif //_CONET_CONTEXT_H_
