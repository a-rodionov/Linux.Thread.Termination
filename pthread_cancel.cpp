#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <vector>
#include <unistd.h>
#include <cxxabi.h>

std::mutex m;
std::condition_variable cv;
bool ready = false;
bool continue_not_interruptible = false;
bool is_not_interruptible_continued = false;

class Object
{
public:
  explicit Object(const std::string& dctrMsg)
    : dctrMsg{dctrMsg} {};

  ~Object()
  {
    std::cout << dctrMsg << " destroyed." << std::endl;
  }

private:
  const std::string dctrMsg;
};

static Object staticObject("Static Object");

void ThreadFunction1_SecondStackFrame()
{
  try
  {
    Object localObject("Local Object. Thread 1. Second stack frame");

    ready = true;
    cv.notify_one();
    while(true)
    {
      sleep(1);
    }
  }
  catch(...)
  {
    throw;
  }
}

void* ThreadFunction1(void*)
{
  try
  {
    Object localObject("Local Object. Thread 1. First stack frame");
    ThreadFunction1_SecondStackFrame();    
  }
  // NPTL generates exception of type abi::__forced_unwind for the thread for
  // which pthread_cancel was called. If this exception is caught and not rethrown,
  // it will lead to segfault. https://udrepper.livejournal.com/21541.html
  catch (abi::__forced_unwind&)
  {
    throw;
  }
  catch(...)
  {}
  return nullptr;
}

void ThreadFunction2_NotInterruptible()
{
  Object localObject("Local Object. Thread 2. Second stack frame");

  auto errCode = pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
  if(errCode)
  {
    std::cerr << "pthread_setcancelstate with PTHREAD_CANCEL_DISABLE failed with error code = " << errCode << std::endl;
    return;
  }

  ready = true;
  cv.notify_one();

  {
    std::unique_lock<std::mutex> lk(m);
    cv.wait(lk, [] { return continue_not_interruptible; });
  }

  is_not_interruptible_continued = true;
  
  errCode = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
  if(errCode)
  {
    std::cerr << "pthread_setcancelstate with PTHREAD_CANCEL_ENABLE failed with error code = " << errCode << std::endl;
    return;
  }

  while(true)
  {
    sleep(1);
  }
}

void* ThreadFunction2(void*)
{
  Object localObject("Local Object. Thread 2. First stack frame");
  ThreadFunction2_NotInterruptible();
  return nullptr;
}

int main()
{
  std::vector<pthread_t> threads(2);
  void* result;

  ready = false;
  pthread_create(&threads[0], NULL, &ThreadFunction1, NULL);
  {
    std::unique_lock<std::mutex> lk(m);
    cv.wait(lk, [] { return ready; });
  }

  ready = false;
  pthread_create(&threads[1], NULL, &ThreadFunction2, NULL);
  {
    std::unique_lock<std::mutex> lk(m);
    cv.wait(lk, [] { return ready; });
  }

  for(auto& thread : threads)
  {
    auto errCode = pthread_cancel(thread);
    if(errCode)
    {
      std::cerr << "pthread_cancel failed with error code = " << errCode << std::endl;
    }
  }

  continue_not_interruptible = true;
  cv.notify_one();

  for(auto& thread : threads)
  {
    auto errCode = pthread_join(thread, &result);
    if(PTHREAD_CANCELED != result)
    {
      std::cerr << "pthread_join failed with error code = " << errCode << std::endl;
    }
  }

  if(is_not_interruptible_continued)
  {
    std::cout << "pthread_setcancelstate with PTHREAD_CANCEL_DISABLE prevented termination of thread" << std::endl;
  }
  else
  {
    std::cout << "pthread_setcancelstate with PTHREAD_CANCEL_DISABLE failed to prevent termination of thread" << std::endl;
  }

  return 0;
}
