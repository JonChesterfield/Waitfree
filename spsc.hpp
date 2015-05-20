#ifndef SPSC_HPP
#define SPSC_HPP

#include <utility>
#include <thread>
#include <atomic>

#include <cassert>

enum queuereturncode
  {
    QUEUESUCCESS = 0,
    QUEUEISFULL,
    QUEUEISEMPTY,
    QUEUEFAILURE,
    QUEUEAUTHORITYFAILURE,
    QUEUEARGUMENTFAILURE,
  };

struct safe_thread
{
  template <typename Function, typename ... Args>
  explicit safe_thread(Function f, Args... args)
    : t(std::forward<Function>(f),std::forward<Args>(args)...) {}
  ~safe_thread() {t.join();}
private:
  std::thread t;
};

namespace queue
{
  typedef uint32_t ptr_size;
  static inline constexpr std::size_t cacheline_size () { return 64;}

  struct front;
  struct back;
  struct front
  {
    typedef back other;
  };
  struct back
  {
    typedef front other;
  };

  template <typename T>
  struct ptr
  {
    ptr() {}
    ptr(ptr_size value) : value(value) {}

    ptr& operator++ ()
    {
      ++value;
      return *this;
    }

    ptr_size value = 0;
  };

  template <typename T>
  struct atomic_ptr
  {
    std::atomic<ptr_size> value{0};
  };

  template <typename T>
  struct caching_pointer
  {
    atomic_ptr<T> index;
    ptr<typename T::other> cache;
  };

  template <typename T>
  struct plain_pointer
  {
    atomic_ptr<T> index;
  };

  template <typename T,
            std::size_t C = 16,
            template <typename> class PP = caching_pointer
            >
  struct spsc
  {
    PP<back> push_to;
    PP<front> pop_from;
    T state[C];
  };

  template <typename T, std::size_t C, template <typename> class PP>
  std::size_t capacity(const spsc<T,C,PP> &)
  {
    return C;
  }

  // Wrap the memory order requirements
  template <typename T>
  static inline ptr<T> load_primary_index(atomic_ptr<T> * location)
  {
    return ptr<T>(atomic_load_explicit(&(location->value),std::memory_order_relaxed));
  }
  template <typename T>
  static inline ptr<T> load_secondary_index(atomic_ptr<T> * location)
  {
    return ptr<T>(atomic_load_explicit(&(location->value),std::memory_order_acquire));
  }
  template <typename T>
  static inline void store_primary_index(atomic_ptr<T> * location, ptr<T> index)
  {
    atomic_store_explicit(&(location->value),index.value, std::memory_order_release);
  }

  // isfull and isempty implement the asymmetry in the calculation and are unit
  // tested directly. They are wrapped in a uniform interface for use here.
  template<std::size_t C>
  bool isfull(ptr<back> local, ptr<front> remote)
  {
    return (local.value >= remote.value)
      ? ((remote.value + C - local.value) < 1)
      : (isfull<C>(local.value-C-1,remote.value-C-1));
  }

  template<std::size_t C>
  bool isempty(ptr<front> local, ptr<back> remote)
  {
    return (remote.value - local.value) < 1;
  }

  template<std::size_t C>
  bool cannot_exchange_dispatch(ptr<front> local, ptr<back> remote)
  {
    return isempty<C>(local,remote);
  }
  template<std::size_t C>
  bool cannot_exchange_dispatch(ptr<back> local, ptr<front> remote)
  {
    return isfull<C>(local, remote);
  }

  // These hold the difference in behaviour for the caching / noncaching versions
  template <std::size_t C, typename L>
  bool unsafe_to_exchange(ptr<L> local_index,
                          plain_pointer<L> * /*local*/,
                          plain_pointer<typename L::other> * remote)
  {
    auto remote_index = load_secondary_index(&(remote->index));
    return (cannot_exchange_dispatch<C>(local_index,remote_index));
  }

  template <std::size_t C, typename L>
  bool unsafe_to_exchange(ptr<L> local_index,
                          caching_pointer<L> * local,
                          caching_pointer<typename L::other> * remote)
  {
    if (cannot_exchange_dispatch<C>(local_index,local->cache))
      {
        local->cache = load_secondary_index(&(remote->index));
        if (cannot_exchange_dispatch<C>(local_index,local->cache))
          {
            return true;
          }
      }
    return false;
  }

  template <typename E, typename QueueType, typename T>
  typename std::enable_if<std::is_same<E, back>::value, queuereturncode>::type
  exchange_at_end(QueueType * Q, T * element)
  {
    return exchange(Q,element,&(Q->push_to),&(Q->pop_from));
  }

  template <typename E, typename QueueType, typename T>
  typename std::enable_if<std::is_same<E, front>::value, queuereturncode>::type
  exchange_at_end(QueueType * Q, T * element)
  {
    return exchange(Q,element,&(Q->pop_from),&(Q->push_to));
  }

  template <typename T, std::size_t C, template <typename> class PP, typename L>
  queuereturncode exchange(spsc<T,C,PP> * Q,
                           T * element,
                           PP<L> * local,
                           PP<typename L::other> * remote)
  {
    using std::swap;
    auto local_index = load_primary_index(&local->index);

    if (unsafe_to_exchange<C>(local_index,local,remote))
      {
        return QUEUEFAILURE;
      }

    swap((Q->state)[(local_index.value)%C],*element);

    store_primary_index(&(local->index),++local_index);
    return QUEUESUCCESS;
  }

  // Public facing functions
  template <typename T, typename QT>
  queuereturncode push(QT * Q, T * t)
  {
    return (QUEUESUCCESS == exchange_at_end<back>(Q,t))
      ? QUEUESUCCESS
      : QUEUEISFULL;
  }

  template <typename T, typename QT>
  queuereturncode pop(QT * Q, T * t)
  {
    return (QUEUESUCCESS == exchange_at_end<front>(Q,t))
      ? QUEUESUCCESS
      : QUEUEISEMPTY;
  }

  template <typename T, typename QT>
  queuereturncode spinpush(QT * Q, T * t)
  {
    queuereturncode ret = QUEUEISFULL;
    while (ret == QUEUEISFULL)
      {
        ret = push(Q,t);
      }
    return ret;
  }

  template <typename T, typename QT>
  queuereturncode spinpop(QT * Q, T * t)
  {
    queuereturncode ret = QUEUEISEMPTY;
    while (ret == QUEUEISEMPTY)
      {
        ret = pop(Q,t);
      }
    return ret;
  }

}

#endif // SPSC_HPP
