#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <map>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <atomic>

std::mutex m, m2;
std::condition_variable cv, cv2;
bool ready = false;
bool processed = false;
std::atomic_bool isSignaled( false );
const static int sleep_period_sec = 5;
const static int SIG_USR_INT = SIGRTMIN + 3;

void signal_handler(int signo)
{ 
  isSignaled = true;
  return; 
}

void set_signal_handler(void)
{
  struct sigaction action;
  
  memset(&action, 0, sizeof(action));
  action.sa_handler = signal_handler;
  
  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, SIG_USR_INT);
  action.sa_mask = set;

  if (sigaction(SIG_USR_INT, &action, NULL) == -1)
  {
    _exit(1);
  }
}

void mask_signal(void)
{
  sigset_t mask;
  sigemptyset(&mask); 
  sigaddset(&mask, SIG_USR_INT); 
  pthread_sigmask(SIG_BLOCK, &mask, NULL);
}

void unmask_signal(void)
{
  sigset_t mask;
  sigemptyset(&mask); 
  sigaddset(&mask, SIG_USR_INT); 
  pthread_sigmask(SIG_UNBLOCK, &mask, NULL);
}

void f_sleep()
{
  unmask_signal();
  ready = true;
  cv.notify_one();
    
  sleep(sleep_period_sec);
    
  processed = true;
  cv.notify_one();
}

void f_usleep()
{
  unmask_signal();
  ready = true;
  cv.notify_one();
    
  usleep(sleep_period_sec * 1000);
    
  processed = true;
  cv.notify_one();
}

void f_sleep_for()
{
  unmask_signal();
  ready = true;
  cv.notify_one();
    
  std::this_thread::sleep_for(std::chrono::seconds(sleep_period_sec));
    
  processed = true;
  cv.notify_one();
}

void condition_variable_wait_for()
{
  unmask_signal();
  ready = true;
  cv.notify_one();

  std::unique_lock<std::mutex> lk(m2);
  if(cv2.wait_for(lk, std::chrono::seconds(sleep_period_sec), [] { return isSignaled.load(); }))
  {
    std::cout << "condition_variable_wait_for isSignaled is set" << std::endl;
  }
  else
  {
    std::cout << "condition_variable_wait_for timeout & isSignaled isn't set" << std::endl;
  }
    
  processed = true;
  cv.notify_one();
}

void run(void(*threadFunction)(), const std::string& functionName)
{
  ready = false;
  processed = false;
  isSignaled = false;

  std::thread thread_inst(threadFunction);
  {
    std::unique_lock<std::mutex> lk(m);
    cv.wait(lk, [] { return ready; });
  }
  auto start = std::chrono::system_clock::now();

  std::this_thread::sleep_for(std::chrono::seconds(1));
  pthread_kill(thread_inst.native_handle(), SIG_USR_INT);

  {
    std::unique_lock<std::mutex> lk(m);
    cv.wait(lk, [] { return processed; });
  }
  if(thread_inst.joinable())
  {
    thread_inst.join();
  }
  auto end = std::chrono::system_clock::now();

  std::cout << "Thread was going to sleep for " << sleep_period_sec << " seconds using " << functionName 
            << " function, but tried to be interrupted by signal and executed for " << std::chrono::duration<double>(end-start).count()
            << " seconds." << std::endl;
}
 
int main()
{
  const std::map<void(*)(), const std::string> functions {{&f_sleep, "sleep"},
                                                          {&f_usleep, "usleep"},
                                                          {&f_sleep_for, "sleep_for"},
                                                          {&condition_variable_wait_for, "condition_variable.wait_for"}};
  set_signal_handler();
  mask_signal();
  for(const auto& function : functions)
  {
    run(function.first, function.second);
  }
  return 0;
}
