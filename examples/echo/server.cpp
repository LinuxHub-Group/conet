/***********************************************
        File Name: server.cpp
        Author: Abby Cin
        Mail: abbytsing@gmail.com
        Created Time: 10/24/20 2:45 PM
***********************************************/

#include "conet.h"
#include <deque>

using namespace conet;

task session(connection&& tmp)
{
  connection client{std::move(tmp)};
  char data[128]{};
  for(;;)
  {
    auto res = co_await client.read_some(data, sizeof(data));
    if(res.err != 0) { co_return; }
    int sent = 0;
    while(sent < res.size)
    {
      auto x = co_await client.write_some(data + sent, res.size - sent);
      if(x.err != 0) { co_return; }
      sent += x.size;
    }
  }
}

task spawn(context& ctx, acceptor& a)
{
  for(;;)
  {
    ctx.launch(session, co_await a.accept());
    if(!ctx.running()) { break; }
  }
}

int main()
{
  context ctx{};
  signals sg{ctx};
  sg.trap(SIGINT);
  sg.trap(SIGTERM);
  sg.ignore(SIGPIPE);
  sg.wait([&ctx](int num) {
    debug("signal:", num, "quit.");
    ctx.stop();
  });
  acceptor a{ctx, resolver::server("127.0.0.1:8889", resolver::socktype::tcp, true)};
  if(a.listen() < 0) { return 1; }
  ctx.launch(spawn, ctx, a);
  ctx.run();
}