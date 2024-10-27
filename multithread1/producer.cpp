#include <iostream>
#include <thread>
#include <queue>
#include <mutex>
#include <cmath>
#include <condition_variable>

using namespace std;

std::mutex mtx;
std::condition_variable cv;

class DataQueue
{
private:
    queue<int> que;
public:
    void put(int val)
    {
        unique_lock<std::mutex> lock(mtx);
        while(!que.empty())
        {
            cv.wait(lock);
        }
        que.push(val);
        cv.notify_all();
        cout << "producter:" << val <<endl;
    }

    int get()
    {
        unique_lock<std::mutex> lock(mtx);
        while(que.empty())
        {
            cv.wait(lock);
        }

        int val = que.front();
        que.pop();
        cv.notify_all();
        cout << "customer:" << val << endl;
        return val;
    }
};

void producer(DataQueue* que)
{
    for(int i=0; i< 10;i++)
    {
        que->put((int)(random() * 10000)% 1993);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}


void consumer(DataQueue* que)
{
    for(int i=0;i<10;i++)
    {
        que->get();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

int main()
{
    DataQueue que;

    std::thread t1(producer, &que);
    std::thread t2(consumer, &que);

    t1.join();
    t2.join();
    return 0;
}