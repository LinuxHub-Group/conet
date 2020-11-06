/***********************************************
        File Name: co_connection.h
        Author: Abby Cin
        Mail: abbytsing@gmail.com
        Created Time: 10/26/20 10:46 PM
***********************************************/

#ifndef _CONET_CONNECTION_H_
#define _CONET_CONNECTION_H_

namespace conet
{
  class connection
  {
  public:
    struct read_awaiter
    {
      int fd_;
      char* data_;
      int n_;
      connection* this_;
      io_result res_{};
      read_awaiter(connection* me, int f, char* data, int n) : fd_{f}, data_{data}, n_{n}, this_{me} {}

      static constexpr bool await_ready() { return false; }

      bool await_suspend(std::coroutine_handle<>& co)
      {
        this_->push(co, true);
        res_.size = ::read(fd_, data_, n_);
        if(res_.size == -1)
        {
          if(errno == EWOULDBLOCK || errno == EAGAIN)
          {
            res_.size = 0;
            return true;
          }
          res_.err = errno;
        }
        if(res_.size == 0)
        {
          res_.err = -1; // customized errno ECLOSED
        }
        return false;
      }

      [[nodiscard]] auto await_resume() const { return res_; }
    };

    struct write_awaiter
    {
      int fd_;
      const char* data_;
      int n_;
      connection* this_;
      io_result res_;
      write_awaiter(connection* me, int f, const char* data, int n) : fd_{f}, data_{data}, n_{n}, this_{me}, res_{} {}

      static constexpr bool await_ready() { return false; }

      bool await_suspend(std::coroutine_handle<>& co)
      {
        this_->push(co, false);
        res_.err = 0;
        res_.size = ::write(fd_, data_, n_);
        if(res_.size == -1)
        {
          if(errno == EWOULDBLOCK || errno == EAGAIN)
          {
            return true; // back to caller
          }
          res_.err = errno;
        }
        return false; // go on
      }

      [[nodiscard]] auto await_resume() const { return res_; }
    };

    connection(context& ctx, resolver r) : fd_{r.fd_}, ctx_{ctx} {}
    connection(connection&& r) noexcept : fd_{r.fd_}, ctx_{r.ctx_} { r.fd_ = -1; }
    connection& operator=(connection&& r) noexcept
    {
      if(this != &r)
      {
        fd_ = r.fd_;
        r.fd_ = -1;
      }
      return *this;
    }
    ~connection() { this->close(); }

    auto read_some(char* data, int n) { return read_awaiter{this, fd_, data, n}; }

    // no guarantee that all data will be sent
    auto write_some(const char* data, int n) { return write_awaiter{this, fd_, data, n}; }

    void close()
    {
      if(fd_ > 0)
      {
        ctx_.pop(fd_);
        ::close(fd_);
        fd_ = -1;
      }
    }

    [[nodiscard]] int handle() const { return fd_; }

    explicit operator bool() { return fd_ != -1; }

    context& ctx() { return ctx_; }

  private:
    int fd_;
    context& ctx_;

    void push(std::coroutine_handle<>& co, bool is_read) { ctx_.push(fd_, co, is_read); }
  };
}

#endif //_CONET_CONNECTION_H_
