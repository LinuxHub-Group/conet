/***********************************************
        File Name: x.cpp
        Author: Abby Cin
        Mail: abbytsing@gmail.com
        Created Time: 11/5/20 8:31 PM
***********************************************/

#include "conet.h"

using namespace conet;

task<std::string> get(connection& client)
{
  std::string header{"GET / HTTP/1.1\r\nhost: isliberty.me\r\n\r\n"};
  auto o = co_await client.write_some(header.data(), header.size());
  if(o.err != 0)
  {
    debug("send request:", ::strerror(o.err));
    co_return {};
  }
  std::vector<char> buf(8192);
  std::string res{};
  while(true)
  {
    o = co_await client.read_some(buf.data(), buf.size());
    if(o.err != 0)
    {
      debug("read response:", ::strerror(o.err));
      co_return {};
    }
    if(o.size > 0)
    {
      res = std::string{buf.data(), (size_t)o.size};
      break;
    }
  }

  co_return res;
}

task<std::string> foo(connection& client) { return get(client); }

int main()
{
  context ctx{};
  auto r = resolver::client("isliberty.me:80", resolver::socktype::tcp);
  if(!r)
  {
    debug(r.err());
    return 1;
  }
  connection client{ctx, r};
  auto x = foo(client);
  ctx.run_once();
  debug(x.value());
}