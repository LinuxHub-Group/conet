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
  namespace detail
  {
    template<typename T = void>
    struct task_promise;
    template<typename T = void>
    struct task_awaiter;
  }

  template<typename T = void>
  using co_handle_t = std::coroutine_handle<detail::task_promise<T>>;

  template<typename T = void>
  class task;

  template<typename T>
  class task
  {
  public:
    task(co_handle_t<T> h) : co_{h} {}
    task(const task&) = delete;
    task(task&& r) : co_{r.co_} { r.co_ = nullptr; }
    ~task()
    {
      if(co_) { co_.destroy(); }
    }

    task& operator=(const task&) = delete;
    task& operator=(task&& r)
    {
      if(this != &r)
      {
        co_.destroy();
        co_ = r.co_;
        r.co_ = nullptr;
      }
      return *this;
    }

    co_handle_t<T> detach();

    void resume() { co_.resume(); }

    bool done() { return !co_ || co_.done(); }
    T value() { return co_.promise().value; }

    detail::task_awaiter<T> operator co_await();

  private:
    template<typename X>
    friend class detail::task_promise;
    co_handle_t<T> co_;
  };

  template<>
  class task<void>
  {
  public:
    task(co_handle_t<> h) : co_{h} {}
    task(const task&) = delete;
    task(task&& r) : co_{r.co_} { r.co_ = nullptr; }
    ~task()
    {
      if(co_) { co_.destroy(); }
    }
    task& operator=(const task&) = delete;
    task& operator=(task&& r)
    {
      if(this != &r)
      {
        co_.destroy();
        co_ = r.co_;
        r.co_ = nullptr;
      }
      return *this;
    }

    co_handle_t<> detach();

    void resume() { co_.resume(); }

    bool done() { return !co_ || co_.done(); }

    detail::task_awaiter<> operator co_await();

  private:
    friend class detail::task_promise<void>;
    co_handle_t<> co_;
  };

  namespace detail
  {
    template<typename T>
    struct task_awaiter
    {
      co_handle_t<T> co;
      constexpr static bool await_ready() { return false; }
      co_handle_t<T> await_suspend(co_handle_t<T>& h)
      {
        co.promise().co = h;
        return co;
      }
      T& await_resume() { return co.promise().value; }
    };

    template<typename T>
    struct transform_awaiter
    {
      co_handle_t<T> co_;
      bool await_ready() { return false; }
      void await_suspend(std::coroutine_handle<> h) { h.resume(); }
      T await_resume() { return co_.promise().value; }
    };

    template<>
    struct transform_awaiter<void>
    {
      bool await_ready() { return false; }
      void await_suspend(std::coroutine_handle<> h) { h.resume(); }
      void await_resume() {}
    };

    template<typename T>
    struct task_promise
    {
      task_promise() noexcept : detached{false}, value{} {}
      task<T> get_return_object() { return {co_handle_t<T>::from_promise(*this)}; }

      std::suspend_never initial_suspend() { return {}; }

      auto final_suspend()
      {
        struct final_awaiter
        {
          bool ready;
          bool await_ready() { return ready; }
          void await_suspend(co_handle_t<T>& h)
          {
            auto& co = h.promise().co;
            if(co && !co.done()) { co.resume(); }
          }
          void await_resume() {}
        };
        return final_awaiter{detached};
      }

      void unhandled_exception() noexcept {}

      void return_value(T x) { value = std::move(x); }

      template<typename U>
      auto await_transform(task<U>&& t)
      {
        return transform_awaiter<U>{t.detach()};
      }

      template<typename U>
      auto await_transform(task<U>& t)
      {
        return transform_awaiter<U>{t.detach()};
      }

      auto await_transform(task<>&& t)
      {
        t.detach();
        return transform_awaiter<void>{};
      }

      auto await_transform(task<>& t)
      {
        t.detach();
        return transform_awaiter<void>{};
      }

      template<typename U>
      U await_transform(U&& u)
      {
        return std::move(u);
      }

      template<typename U>
      U& await_transform(U& u)
      {
        return u;
      }

      bool detached;
      T value;
      co_handle_t<T> co;
    };

    template<>
    struct task_promise<void>
    {
      task_promise() noexcept : detached{false} {}
      task<> get_return_object() { return {co_handle_t<>::from_promise(*this)}; }

      std::suspend_never initial_suspend() { return {}; }

      auto final_suspend()
      {
        struct final_awaiter
        {
          bool ready;
          bool await_ready() { return ready; }
          void await_suspend(co_handle_t<>& h)
          {
            auto& co = h.promise().co;
            if(co && !co.done()) { co.resume(); }
          }
          void await_resume() {}
        };
        return final_awaiter{detached};
      }

      void unhandled_exception() noexcept {}

      void return_void() {}

      template<typename U>
      auto await_transform(task<U>&& t)
      {
        return transform_awaiter<U>{t.detach()};
      }

      template<typename U>
      auto await_transform(task<U>& t)
      {
        return transform_awaiter<U>{t.detach()};
      }

      auto await_transform(task<>&& t)
      {
        t.detach();
        return transform_awaiter<void>{};
      }

      auto await_transform(task<>& t)
      {
        t.detach();
        return transform_awaiter<void>{};
      }

      template<typename U>
      U await_transform(U&& u)
      {
        return std::move(u);
      }
      bool detached;
      co_handle_t<> co;
    };

    template<>
    struct task_awaiter<void>
    {
      co_handle_t<> co;
      constexpr static bool await_read() { return false; }
      co_handle_t<> await_suspend(co_handle_t<>& h)
      {
        co.promise().co = h;
        return co;
      }
      void await_resume() {}
    };
  }

  template<typename T>
  co_handle_t<T> task<T>::detach()
  {
    co_.promise().detached = true;
    return std::exchange(co_, nullptr);
  }

  co_handle_t<> task<void>::detach()
  {
    co_.promise().detached = true;
    return std::exchange(co_, nullptr);
  }

  template<typename T>
  detail::task_awaiter<T> task<T>::operator co_await()
  {
    return {co_};
  }
}

namespace std
{
  template<typename T, typename... Args>
  struct coroutine_traits<conet::task<T>, Args...>
  {
    using promise_type = conet::detail::task_promise<T>;
  };

  template<typename... Args>
  struct coroutine_traits<conet::task<>, Args...>
  {
    using promise_type = conet::detail::task_promise<>;
  };
}

#endif //_CONET_COROUTINES_H_
