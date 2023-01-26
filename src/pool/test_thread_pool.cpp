#include <iostream>
#include "./threadpool.cpp"
using namespace std;
int main() {
  int n = 20;
  atomic<int> v;
  ThreadPool tp(n);
  for(int i = 0; i < 20; ++i)
    tp.push_task([&v]() {
      v += 2;
    });
  tp.wait_for_tasks();
  cout << v << endl;  
  return 0;
}