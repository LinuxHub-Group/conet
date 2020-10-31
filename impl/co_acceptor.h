/***********************************************
        File Name: co_acceptor.h
        Author: Abby Cin
        Mail: abbytsing@gmail.com
        Created Time: 10/22/20 8:05 PM
***********************************************/

#ifndef _CONET_ACCEPTOR_H_
#define _CONET_ACCEPTOR_H_

namespace conet
{
  class acceptor
  {
  public:
    acceptor(context& ctx, resolver r) : fd_{r.fd_}, co_{nullptr}, ctx_{ctx} {}
    ~acceptor() = default;

    [[nodiscard]] int listen() const { return ::listen(fd_, SOMAXCONN); }

    auto accept() { return awaiter{this, ctx_, fd_}; }

  private:
    int fd_;
    simple_co_handle_t co_;
    context& ctx_;

    struct awaiter
    {
      int fd_;
      acceptor* this_;
      context& ctx_;
      awaiter(acceptor* me, context& ctx, int fd) : fd_{fd}, this_{me}, ctx_{ctx} {}

      static constexpr bool await_ready() { return false; }
      void await_suspend(simple_co_handle_t co) { ctx_.push(fd_, co); }
      connection await_resume()
      {
        int sock = ::accept4(fd_, nullptr, nullptr, SOCK_NONBLOCK);
        return {ctx_, resolver{sock}};
      }
    };
  };
}

#endif //_CONET_ACCEPTOR_H_
