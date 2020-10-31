/***********************************************
        File Name: timer.h
        Author: Abby Cin
        Mail: abbytsing@gmail.com
        Created Time: 10/26/20 9:40 PM
***********************************************/

#ifndef _CONET_TIMER_H_
#define _CONET_TIMER_H_

namespace conet::detail
{
  class timer_queue
  {
    struct telem;

  public:
    struct handle
    {
    private:
      friend timer_queue;
      telem* e_;
      handle(telem* e) : e_{e} {}

    public:
      void cancel() { e_->again = -1; }
    };

  private:
    using cb_t = std::function<void(handle)>;
    struct telem
    {
      int again; // 0: single, 1: multiple, -1: done
      long long dur;
      cb_t fp;
    };
    struct key
    {
      long long dur;
      telem* elem;
      key(long long d, telem* e) : dur{d}, elem{e} {}
      auto operator<=>(const key&) const = default;
    };

  public:
    template<typename Rep, typename Peroid>
    handle run_after(const std::chrono::duration<Rep, Peroid>& d, cb_t fp)
    {
      return this->add(d, std::move(fp), 0);
    }

    template<typename Rep, typename Peroid>
    handle run_every(const std::chrono::duration<Rep, Peroid>& d, cb_t fp)
    {
      return this->add(d, std::move(fp), 1);
    }

    void tick()
    {
      auto t = curr_time();
      auto cur = key{t, (telem*)UINTPTR_MAX};
      auto end = q_.lower_bound(cur);
      std::vector<key> re{};
      for(auto iter = q_.begin(); iter != end;)
      {
        if(iter->elem->again == -1) { delete iter->elem; }
        else
        {
          iter->elem->fp({iter->elem});
          if(iter->elem->again == 1) { re.emplace_back(*iter); }
          else
          {
            delete iter->elem;
          }
        }
        iter = q_.erase(iter);
      }

      while(!re.empty())
      {
        q_.emplace(key{t + re.back().elem->dur, re.back().elem});
        re.pop_back();
      }
    }

    bool empty() { return q_.empty(); }

    timer_queue() : q_{} {}
    timer_queue(timer_queue&& r) noexcept : q_{std::move(r.q_)} {}
    timer_queue& operator=(timer_queue&&) = delete;
    timer_queue& operator=(const timer_queue&) = delete;
    ~timer_queue()
    {
      for(auto& x: q_)
      {
        delete x.elem;
      }
      q_.clear();
    }

  private:
    std::set<key> q_;
    friend handle;

    template<typename Rep, typename Peroid>
    handle add(const std::chrono::duration<Rep, Peroid>& d, cb_t&& f, bool repeat)
    {
      static_assert(Peroid::den <= 1'000);
      auto milli = std::chrono::duration_cast<std::chrono::milliseconds>(d).count();
      auto e = key{next_time(milli), new telem{repeat, milli, std::move(f)}};
      q_.emplace(e);
      return {e.elem};
    }

    static long long curr_time()
    {
      auto x = std::chrono::steady_clock::now();
      return std::chrono::duration_cast<std::chrono::milliseconds>(x.time_since_epoch()).count();
    }
    static long long next_time(long long d) { return curr_time() + d; }
  };
}

#endif //_CONET_TIMER_H_
