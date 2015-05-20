#include "spsc.hpp"
#include "catch.hpp"

#include <vector>
#include <thread>

TEST_CASE("queues are empty when pointers match")
{ 
  const queue::ptr_size maxsize = std::numeric_limits<queue::ptr_size>::max();
  CHECK(queue::isempty<32>(0,0) == true);
  CHECK(queue::isempty<32>(12,12) == true);
  CHECK(queue::isempty<32>(maxsize,maxsize) == true);
}

TEST_CASE("queues are not empty when back is greater than front")
{
  const queue::ptr_size maxsize = std::numeric_limits<queue::ptr_size>::max();
  CHECK(queue::isempty<32>(0,1) == false);
  CHECK(queue::isempty<32>(-1,0) == false);
  CHECK(queue::isempty<32>(12,24) == false);
  CHECK(queue::isempty<32>(maxsize-1,maxsize) == false);
  CHECK(queue::isempty<32>(maxsize,maxsize+1) == false);
}

TEST_CASE("queues are not empty when front is greater than back")
{
  const queue::ptr_size maxsize = std::numeric_limits<queue::ptr_size>::max();
  CHECK(queue::isempty<32>(0,-1) == false);
  CHECK(queue::isempty<32>(1,0) == false);
  CHECK(queue::isempty<32>(24,12) == false);
  CHECK(queue::isempty<32>(maxsize,maxsize-1) == false);
  CHECK(queue::isempty<32>(maxsize+1,maxsize) == false);
}

TEST_CASE("queues are full when back is exactly capacity greater than front")
{
  const queue::ptr_size maxsize = std::numeric_limits<queue::ptr_size>::max();

  CHECK(queue::isfull<32>(32,-1) == false);
  CHECK(queue::isfull<32>(32,0) == true);
  CHECK(queue::isfull<32>(32,1) == false);
   
  CHECK(queue::isfull<32>(47,16) == false);
  CHECK(queue::isfull<32>(48,16) == true);
  CHECK(queue::isfull<32>(49,16) == false);

  CHECK(queue::isfull<32>(32+maxsize,maxsize-1) == false);
  CHECK(queue::isfull<32>(32+maxsize,maxsize) == true);
  CHECK(queue::isfull<32>(32+maxsize,maxsize+1) == false);
}

TEST_CASE("can test for full when back has overflowed but front has not")
{
  const queue::ptr_size maxsize = std::numeric_limits<queue::ptr_size>::max();
  CHECK(queue::isfull<32>(32+maxsize,maxsize) == true);
  CHECK(queue::isfull<32>(16+maxsize,maxsize-16) == true);
  CHECK(queue::isfull<32>(1+maxsize,maxsize-31) == true);
}

TEST_CASE("ctor spsc")
{
  {
    queue::spsc<int> queue;
  }
  {
    queue::spsc<double> queue;
  }
}

TEST_CASE("popping from an empty queue returns empty")
{
  queue::spsc<int> queue;
  int val = 0;
  auto ret = queue::pop(&queue,&val);
  CHECK(ret == QUEUEISEMPTY);
}

TEST_CASE("can push a single value")
{
  queue::spsc<int> queue;
  int val = 42;
  auto ret = queue::push(&queue,&val);
  CHECK(ret == QUEUESUCCESS);
}

TEST_CASE("Can retrieve a single pushed value using pop")
{
  queue::spsc<int> queue;
  int input = 42;
  int output = 0;
  {
    int copy_input = input;
    auto ret = queue::push<int>(&queue,&copy_input);
    CHECK(ret==QUEUESUCCESS);
  }
  {
    auto ret = queue::pop<int>(&queue,&output);
    CHECK(ret==QUEUESUCCESS);
  }
  CHECK(input == output);
}

TEST_CASE("A queue has a capacity function that returns greater than zero")
{
  queue::spsc<int> queue;
  CHECK(queue::capacity(queue) > 0);
}

TEST_CASE("A queue can be created with specific capacity")
{
  {
    queue::spsc<int,42> queue;
    CHECK(queue::capacity(queue) == 42);
  }
  {
    queue::spsc<double,13> queue;
    CHECK(queue::capacity(queue) == 13);
  }
}

TEST_CASE("A queue can have data pushed exactly capacity() times before it is full")
{
  const std::size_t capacity = 13;
  queue::spsc<std::size_t,capacity> queue;
  std::size_t value = 0;

  for (std::size_t i = 0; i < capacity; i++)
    {
      std::size_t value = i + 1;
      auto ret = queue::push(&queue,&value);
      CHECK(ret==QUEUESUCCESS);
    }

  auto ret = queue::push(&queue,&value);
  CAPTURE(ret);
  CHECK(ret==QUEUEISFULL);
}


static std::vector<double> get_representative_values(std::size_t N, double seed = -1.0)
{
  std::vector<double> values;
  double value = seed;
  values.reserve(N);
  for (std::size_t i = 0; i < N; ++i)
    {
      value += 1.0;
      values.push_back(value);
    }
  return values;
}

TEST_CASE("Filling queue then emptying returns elements in FIFO order")
{
  const std::size_t capacity = 4;
  queue::spsc<double,capacity> queue;
  std::vector<double> values = get_representative_values(capacity);

  for (std::size_t i = 0; i < capacity; ++i)
    {
      double value = values.at(i);
      auto ret = queue::push(&queue,&value);
      CHECK(ret==QUEUESUCCESS);
    }

  {
    double value = 0;
    CHECK(queue.state[queue.push_to.index.value-1] == values.at(capacity-1));
    auto ret = queue::push(&queue,&value);
    CHECK(ret==QUEUEISFULL);
  }

  for (std::size_t i = 0; i < capacity; ++i )
    {
      double value = 0;
      auto ret = queue::pop(&queue,&value);
      CHECK(ret==QUEUESUCCESS);
      CHECK(value == values.at(i));
    }

  {
    double value = 0;
    auto ret = queue::pop(&queue,&value);
    CHECK(ret==QUEUEISEMPTY);
  }
}

TEST_CASE("Queues do not fill up when emptied at the same rate")
{
  const std::size_t capacity = 1;
  const int num_checks = 32;
  queue::spsc<double,capacity> queue;

  for (int i = 0 ; i < num_checks; i++)
    {
      double value = 0;
      CHECK(queue::push(&queue,&value) == QUEUESUCCESS);
      CHECK(queue::pop(&queue,&value)==QUEUESUCCESS);
    }
}

TEST_CASE("Queue fills up at predicatable point when filled at twice the rate")
{
  const std::size_t capacity = 2;
  queue::spsc<double,capacity> queue;
  double value = 0;

  value = 0;
  CHECK(queue::push(&queue,&value) == QUEUESUCCESS);
  value = 0;
  CHECK(queue::push(&queue,&value) == QUEUESUCCESS);
  value = 0;
  CHECK(queue::pop(&queue,&value) == QUEUESUCCESS);

  value = 0;
  CHECK(queue::push(&queue,&value) == QUEUESUCCESS);
  value = 0;
  CHECK(queue::push(&queue,&value) == QUEUEISFULL);

  value = 0;
  CHECK(queue::pop(&queue,&value) == QUEUESUCCESS);
  value = 0;
  CHECK(queue::push(&queue,&value) == QUEUESUCCESS);

  value = 0;
  CHECK(queue::push(&queue,&value) == QUEUEISFULL);
}

TEST_CASE("Can create a safe_thread")
{
  int state = 0;
  {
    safe_thread thread([&state](){state = 13;});
  }
  CHECK(state == 13);
}

TEST_CASE("Can create a safe_thread which takes arguments")
{
  double state = 0;
  {
    auto f = [&state](double a, int b){state = a + 2 * b;};
    safe_thread thread(f,3.0,2);
  }
  CHECK(state == 7.0);
}

template <typename T, std::size_t C>
static std::size_t push_helper(queue::spsc<T,C> * queue, std::vector<T> * values)
{
  std::size_t errors = 0;
  for (std::size_t i = 0; i < C; ++i)
    {
      T value = values->at(i);
      auto ret = queue::spinpush(queue,&value);
      if (ret!=QUEUESUCCESS)
	{
	  errors++;
	}
    }
  return errors;
}

template <typename T, std::size_t C>
static std::size_t pop_helper(queue::spsc<T,C> * queue, std::vector<T> * values)
{
  std::size_t errors = 0;
  for (std::size_t i = 0; i < C; ++i)
    {
      double value = 0;
      auto ret = queue::spinpop(queue,&value);
      if ((ret!=QUEUESUCCESS) || (value != values->at(i)))
	{
	  errors++;
	}
    }
  return errors;
}

static constexpr std::size_t large_queue()
{
  return 128*128;
}

static constexpr std::size_t repeats()
{
  return 128;
}

TEST_CASE("Push and pop using threads running sequentially")
{
  const std::size_t capacity = large_queue();
  queue::spsc<double,capacity> queue;

  for (std::size_t r = 0; r < repeats(); ++r)
    {
      std::vector<double> values = get_representative_values(16*capacity, 3.14);

      std::size_t push_errors = 0;
      std::size_t pop_errors = 0;

      {
	safe_thread push([&]{push_errors = push_helper(&queue,&values);});
      }
  
      {
	safe_thread pop([&]{pop_errors = pop_helper(&queue,&values);});
      }
      CAPTURE(r);
      CHECK(push_errors == 0);
      CHECK(pop_errors == 0);
    }
}

TEST_CASE("Push and pop using threads running simultaneously, push first")
{
  const std::size_t capacity = large_queue();
  queue::spsc<double,capacity> queue;

  for (std::size_t r = 0; r < repeats(); ++r)
    {
      std::vector<double> pushed_values = get_representative_values(capacity, 3.14);
      std::vector<double> popped_values(pushed_values);
  
      std::size_t push_errors = 0;
      std::size_t pop_errors = 0;

      {
	safe_thread push([&]{push_errors = push_helper(&queue,&pushed_values);});
	safe_thread pop([&]{pop_errors = pop_helper(&queue,&popped_values);});
      }
      CAPTURE(r);
      CHECK(push_errors == 0);
      CHECK(pop_errors == 0);
    }
}

TEST_CASE("Push and pop using threads running simultaneously, pop first")
{
  const std::size_t capacity = large_queue();
  queue::spsc<double,capacity> queue;

  for (std::size_t r = 0; r < repeats(); ++r)
    {
      std::vector<double> pushed_values = get_representative_values(capacity, 3.14);
      std::vector<double> popped_values(pushed_values);
  
      std::size_t push_errors = 0;
      std::size_t pop_errors = 0;

      {
	safe_thread pop([&]{pop_errors = pop_helper(&queue,&popped_values);});
	safe_thread push([&]{push_errors = push_helper(&queue,&pushed_values);});
      }
      CAPTURE(r);
      CHECK(push_errors == 0);
      CHECK(pop_errors == 0);
    }
}

TEST_CASE("Queue can handle overflow of internal pointer on single thread")
{
  const std::size_t capacity = 8;
  const queue::ptr_size offset = 64;
  int number_values_moved = offset * 8;
    
  queue::spsc<double,capacity> queue;

  const queue::ptr_size maxsize = std::numeric_limits<queue::ptr_size>::max();

  queue::ptr_size starting_position = maxsize - offset; 
  atomic_store_explicit(&(queue.push_to.index.value),starting_position,std::memory_order_seq_cst);
  atomic_store_explicit(&(queue.pop_from.index.value),starting_position,std::memory_order_seq_cst);

  for (int i = 0 ; i < number_values_moved; i++)
    {
      double value = 3.14 * i;
      double tmp = value;

      CHECK(queue::spinpush(&queue,&tmp) == QUEUESUCCESS);
      tmp = 0;
      CHECK(queue::spinpop(&queue,&tmp) == QUEUESUCCESS);
      CHECK(value == tmp);
    }
}
