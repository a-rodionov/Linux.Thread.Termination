#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <vector>
#include <unistd.h>
#include <cxxabi.h>

std::mutex m, m2;
std::timed_mutex socket_mutex;
std::condition_variable cv, cv2;
bool ready = false;
bool continue_not_interruptible = false;
bool is_not_interruptible_continued = false;
const static int sleep_period_sec = 5;

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

void* ThreadFunction1(void*)
{
  bool isTimeout = false;
  try
  {
    std::unique_lock<std::mutex> lk(m2);

    ready = true;
    cv.notify_one();

    isTimeout = !cv2.wait_for(lk, std::chrono::seconds(sleep_period_sec), [] { return false; });
  }
  catch (abi::__forced_unwind&)
  {
    if(isTimeout)
    {
      std::cout << "ThreadFunction1. condition_variable.wait_for wasn't interrupted by pthread_cancel." << std::endl;
    }
    else
    {
      std::cout << "ThreadFunction1. condition_variable.wait_for was interrupted by pthread_cancel." << std::endl;
    }
    throw;
  }
  catch(...)
  {}

  if(isTimeout)
  {
    std::cout << "ThreadFunction1. condition_variable.wait_for wasn't interrupted by pthread_cancel." << std::endl;
  }
  else
  {
    std::cout << "ThreadFunction1. condition_variable.wait_for was interrupted by pthread_cancel." << std::endl;
  }

  return nullptr;
}

// ThreadFunction2 will be waiting for socket_mutex which is locked by main thread.
void* ThreadFunction2(void*)
{
  bool isTimeout = false;
  try
  {
    ready = true;
    cv.notify_one();

    isTimeout = !socket_mutex.try_lock_for(std::chrono::seconds(sleep_period_sec));
    if (!isTimeout)
    {
      socket_mutex.unlock();
    }
  }
  catch (abi::__forced_unwind&)
  {
    if(isTimeout)
    {
      std::cout << "ThreadFunction2. timed_mutex.try_lock_for with locked mutex wasn't canceled by pthread_cancel." << std::endl;
    }
    else
    {
      std::cout << "ThreadFunction2. timed_mutex.try_lock_for with locked mutex was canceled by pthread_cancel." << std::endl;
    }
    throw;
  }
  catch(...)
  {}

  if(isTimeout)
  {
    std::cout << "ThreadFunction2. timed_mutex.try_lock_for with locked mutex wasn't canceled by pthread_cancel." << std::endl;
  }
  else
  {
    std::cout << "ThreadFunction2. timed_mutex.try_lock_for with locked mutex was canceled by pthread_cancel." << std::endl;
  }
  return nullptr;
}

void ThreadFunction3_SecondStackFrame()
{
  try
  {
    Object localObject("Local Object. Thread 3. Second stack frame");

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

void* ThreadFunction3(void*)
{
  try
  {
    Object localObject("Local Object. Thread 3. First stack frame");
    ThreadFunction3_SecondStackFrame();    
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

void ThreadFunction4_NotInterruptible()
{
  Object localObject("Local Object. Thread 4. Second stack frame");

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

void* ThreadFunction4(void*)
{
  Object localObject("Local Object. Thread 4. First stack frame");
  ThreadFunction4_NotInterruptible();
  return nullptr;
}

int main()
{
  std::vector<pthread_t> threads(4);
  void* result;

  ready = false;
  pthread_create(&threads[0], NULL, &ThreadFunction1, NULL);
  {
    std::unique_lock<std::mutex> lk(m);
    cv.wait(lk, [] { return ready; });
  }

  socket_mutex.lock();

  ready = false;
  pthread_create(&threads[1], NULL, &ThreadFunction2, NULL);
  {
    std::unique_lock<std::mutex> lk(m);
    cv.wait(lk, [] { return ready; });
  }

  ready = false;
  pthread_create(&threads[2], NULL, &ThreadFunction3, NULL);
  {
    std::unique_lock<std::mutex> lk(m);
    cv.wait(lk, [] { return ready; });
  }

  ready = false;
  pthread_create(&threads[3], NULL, &ThreadFunction4, NULL);
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

  socket_mutex.unlock();

  return 0;
}
