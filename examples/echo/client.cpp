/***********************************************
        File Name: client.cpp
        Author: Abby Cin
        Mail: abbytsing@gmail.com
        Created Time: 10/25/20 8:29 PM
***********************************************/

#include "conet.h"

using namespace conet;

int count = 0;
task<> echo(connection&& tmp, const std::string& msg)
{
  std::vector<char> recv(128);
  auto client = std::move(tmp);

  for(int i = 0; i < 1000; ++i)
  {
    auto res = co_await client.write_some(msg.data(), msg.size());
    if(res.err != 0) { co_return; }
    res = co_await client.read_some(recv.data(), recv.size());
    if(res.err != 0) { co_return; }
    count += 1;
  }
}

int main()
{
  context ctx{};
  signals sig{ctx};
  sig.trap(SIGINT);
  sig.wait([&ctx](int num) {
    debug("signal:", num, "quit.");
    ctx.stop();
  });
  ctx.timer().run_after(std::chrono::seconds(2), [&ctx](auto) {
    debug("stop.");
    ctx.stop();
  });

  std::vector<task<>> tasks{};
  std::string msg{"are you ok?"};
  for(int i = 0; i < 100; ++i)
  {
    auto r = resolver::client("127.0.0.1:8889", resolver::socktype::tcp);
    if(!r)
    {
      debug(r.err());
      return 1;
    }
    auto t = echo(connection{ctx, r}, msg);
    tasks.push_back(std::move(t)); // manage tasks by ourself
  }
  ctx.run();
  debug(count);
}
