/***********************************************
        File Name: co_aux.h
        Author: Abby Cin
        Mail: abbytsing@gmail.com
        Created Time: 10/24/20 2:43 PM
***********************************************/

#ifndef _CONET_AUX_H_
#define _CONET_AUX_H_

#ifndef NDEBUG
#include <iostream>

struct debugger
{
  debugger(const char* s, int n) { std::cerr << s << ':' << n << " => "; }
  template<typename... Args>
  void print(Args&&... args)
  {
    ((std::cerr << args << ' '), ...);
    std::cerr << '\n';
  }
};
#define debug(...) debugger(__func__, __LINE__).print(__VA_ARGS__)

#else

#define debug(...)

#endif

namespace conet
{
  class acceptor;
  class connection;

  class resolver
  {
  public:
    enum struct socktype : unsigned
    {
      tcp = SOCK_STREAM,
      udp = SOCK_DGRAM
    };

  private:
    friend acceptor;
    friend connection;
    int fd_;
    std::string err_;
    explicit resolver(int fd, const char* msg = "") : fd_{fd}, err_{msg} {}

    static resolver impl(const std::string& s, socktype f, bool is_passive, bool reuseaddr, bool reuseport)
    {
      addrinfo hints{};
      ::memset(&hints, 0, sizeof(hints));
      hints.ai_family = AF_UNSPEC;
      hints.ai_socktype = static_cast<int>(f);
      hints.ai_protocol = 0;
      addrinfo* addrs{nullptr};

      auto sep = s.find(':');
      if(sep == std::string::npos) { return resolver{-1, "invalid address"}; }
      auto port = s.substr(sep + 1);
      auto domain = s.substr(0, sep);
      if(port.empty() || domain.empty()) { return resolver{-1, "invalid address"}; }

      int r = ::getaddrinfo(domain.data(), port.data(), &hints, &addrs);
      if(r < 0) { return resolver{-1, ::gai_strerror(r)}; }
      int fd = 0;
      for(auto p = addrs; p != nullptr; p = p->ai_next)
      {
        fd = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if(fd == -1) { continue; }

        if(is_passive)
        {
          if(reuseaddr)
          {
            int on = 1;
            on = ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
          }
          if(reuseport)
          {
            int on = 1;
            ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
          }
          if(::bind(fd, p->ai_addr, p->ai_addrlen) == 0) { break; }
        }
        else
        {
          // connect for UDP too
          if(::connect(fd, p->ai_addr, p->ai_addrlen) == 0) { break; }
        }
        ::close(fd);
        fd = 0;
      }
      int saved = errno;
      ::freeaddrinfo(addrs);
      if(fd == 0) { return resolver{-1, ::strerror(saved)}; }
      int flag = ::fcntl(fd, F_GETFL, 0);
      if(flag < 0) { return resolver{-1, ::strerror(saved)}; }
      if(::fcntl(fd, F_SETFL, flag | O_NONBLOCK) < 0) { return resolver{-1, ::strerror(saved)}; }
      return resolver{fd};
    }

  public:
    static resolver client(const std::string& s, socktype f) { return impl(s, f, false, false, false); }

    static resolver server(const std::string& s, socktype f, bool reuseaddr = false, bool reuseport = false)
    {
      return impl(s, f, true, reuseaddr, reuseport);
    }

    explicit operator bool() const { return fd_ != -1; }

    [[nodiscard]] const std::string& err() const { return err_; }
  };

  struct io_result
  {
    int err;
    int size;
  };
}

#endif //_CONET_AUX_H_
