#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <signal.h>
#include <unistd.h>
#include <string.h>

std::mutex m;
std::condition_variable cv;
bool ready = false;
bool processed = false;
const static int sleep_period_sec = 5;

void mask_signal(void)
{
  sigset_t mask;
  sigemptyset(&mask); 
  sigaddset(&mask, SIGRTMIN + 3); 
  pthread_sigmask(SIG_BLOCK, &mask, NULL);
}

void signal_handler(int signo)
{ 
  return; 
}

void set_signal_handler(void)
{
  struct sigaction action;
  
  memset(&action, 0, sizeof(action));
  action.sa_handler = signal_handler;
  
  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, SIGRTMIN + 3);
  action.sa_mask = set;

  if (sigaction(SIGRTMIN + 3, &action, NULL) == -1)
  {
    _exit(1);
  }
}

void f_signal_not_masked()
{
  ready = true;
  cv.notify_one();

  sleep(sleep_period_sec);
    
  processed = true;
  cv.notify_one();
}

void f_signal_masked()
{
  mask_signal();

  ready = true;
  cv.notify_one();

  sleep(sleep_period_sec);
    
  processed = true;
  cv.notify_one();
}

void run(void(*threadFunction)(), const bool isSignalMasked)
{
  ready = false;
  processed = false;

  std::thread thread_inst(threadFunction);
  {
    std::unique_lock<std::mutex> lk(m);
    cv.wait(lk, [] { return ready; });
  }
  auto start = std::chrono::system_clock::now();

  pthread_kill(thread_inst.native_handle(), SIGRTMIN + 3);

  {
    std::unique_lock<std::mutex> lk(m);
    cv.wait(lk, [] { return processed; });
  }
  if(thread_inst.joinable())
  {
    thread_inst.join();
  }
  auto end = std::chrono::system_clock::now();

  std::cout << "Thread was going to sleep for " << sleep_period_sec << " seconds using sleep"
            << " function, but tried to be interrupted by signal and executed for " << std::chrono::duration<double>(end-start).count()
            << " seconds. ";
  if(isSignalMasked)
  {
    std::cout << "The signal was masked." << std::endl;
  }
  else
  {
    std::cout << "The signal wasn't masked." << std::endl;
  }
}

int main()
{
  set_signal_handler();
  run(&f_signal_not_masked, false);
  run(&f_signal_masked, true);
  return 0;
}
