/***********************************************
        File Name: co_coroutines.h
        Author: Abby Cin
        Mail: abbytsing@gmail.com
        Created Time: 10/26/20 10:22 PM
***********************************************/

#ifndef _CONET_COROUTINES_H_
#define _CONET_COROUTINES_H_

namespace conet
{
  class context;

  class task
  {
    friend context;

  public:
    struct promise_type
    {
      context* ctx_{nullptr};
      std::coroutine_handle<promise_type> co_;
      task get_return_object()
      {
        co_ = std::coroutine_handle<promise_type>::from_promise(*this);
        return {co_};
      }
      static std::suspend_never initial_suspend() { return {}; }
      std::suspend_always final_suspend();
      void return_void() {}
      static void unhandled_exception() { std::terminate(); }

      void set_context(context* ctx) { ctx_ = ctx; }
    };
    using handle_t = std::coroutine_handle<promise_type>;

    task(handle_t h) : co_{h} {}
    task(task&& r) = delete;
    task& operator=(task&& r) = delete;
    task(const task&) = delete;
    task& operator=(const task&) = delete;
    ~task()
    {
      if(co_) { co_.destroy(); }
    }

  private:
    handle_t co_;

    void set_context(context* ctx)
    {
      co_.promise().set_context(ctx);
      co_ = nullptr;
    }
  };

  using simple_co_handle_t = task::handle_t;
}

#endif //_CONET_COROUTINES_H_
