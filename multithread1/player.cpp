#include <iostream>
#include <thread>
#include <queue>
#include <mutex>
#include <cmath>
#include <condition_variable>
// #include <string>

using namespace std;

class TLoadThread;
class TAudioDecodeThread;
class TVideoDecodeThread;
class TAudioPlayThread;
class TVideoPlayThread;

/*
1. 主线程准备
    - 调用Open函数打开文件
    - 调用create\_loader\_thread创建加载线程
    - 调用create\_decoder\_thread创建解码线程
    - 调用create\_display\_thread创建显示线程
2. 加载线程：按行读文件，放到一个DataQueue队列中，处理队列满的情况
3. 解码线程：从DataQueue队列中获取行，进行解码，解码放入解码后队列DecodedQueue。处理DataQueue不足或DecodedQueue满的情况
4. 显示线程：从DecodedQueue中获取DecodedPackage，调用打印函数打印
*/

/*

数据流向：
1. load线程加载数据得到每帧数据 放入RawDataPacketQueue中，通知decode线程
2. decode线程收到数据通知，从RawDataPacketQueue中pop一个RawData，进行解码，然后放入DecodeDataQueue，通知display线程
3. display线程检查解码数据通知，从DecodeDataQueue里面取对应的数据，并进行展示

*/


struct AudioPacket
{
    int PacketId;
    int PTS;
};

struct VideoPacket
{
    int PacketId;
    int PTS;
};

struct AudioDecodeData
{
    int DecodePacketId;
    int PTS;
};

struct VideoDecodeData
{
    int DecodePacketId;
    int PTS;
};


class ThreadEvent
{
public:
    ThreadEvent() {}

    void Trigger()
    {

    };

    void Reset()
    {

    };

    bool Wait(float milliseconds=0.0f)
    {
        return false;
    }

    bool IsSignal() {
        return false;
    }

    void Lock() {}
    void UnLock() {}

public:
    std::mutex mtx;
    std::condition_variable cv;
};


class Thread
{
public:
    Thread(const string& ThreadName): bStarted(false), ThreadName(ThreadName) {
    }

    virtual ~Thread() {
        cout << "~Thread " <<ThreadName <<endl;
        Stop();
    }

    void Start(){
        bStarted = true;
        std::thread t(&Thread::ThreadWork, this);
        NativeThread = std::move(t);
        cout << "Thread " << ThreadName << " Started"<<endl;
    }

    void Stop(){
        if(bStarted) {
            TerminalEvent.Trigger();
            QuitedEvent.Wait();
            bStarted = false;
            cout << "Thread " << ThreadName << " Finished"<<endl;
        }
    }

    void Join()
    {
        NativeThread.join();
    }

    virtual void ThreadProc()
    {

    }



private:
    string ThreadName;
    bool bStarted;
    ThreadEvent TerminalEvent;
    ThreadEvent QuitedEvent;
    std::thread NativeThread;
private:

    void ThreadWork()
    {
        while(true)
        {
            if(TerminalEvent.IsSignal())
            {
                cout << "Terminate thread event received"<<endl;
                break;
            }
            ThreadProc();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        QuitedEvent.Trigger();
    }
};


class TPlayer
{
public:
    string                        FilePath;
    ThreadEvent AudioPacketLock;
    ThreadEvent VideoPacketLock;
    queue<AudioPacket>            AudioPacketQueue;
    queue<VideoPacket>            VideoPacketQueue;

    ThreadEvent AudioDecodeLock;
    ThreadEvent VideoDecodeLock;
    queue<AudioDecodeData>        AudioDecodeQueue;
    queue<VideoDecodeData>        VideoDecodeQueue;

private:
    shared_ptr<TLoadThread>        Load;
    shared_ptr<TAudioDecodeThread> AudioDecode;
    shared_ptr<TVideoDecodeThread> VideoDecode;
    shared_ptr<TAudioPlayThread>   AudioPlay;
    shared_ptr<TVideoPlayThread>   VideoPlay;


public:
    void CreateLoadThread(const string &FilePath);

    void CreateDecodeThreads();
    void CreatePlayThreads();
    void Open(const string &FilePath);

    void Play() 
    {

    }

    void Pause()
    {

    }

    void Seek()
    {

    }

    void Resume()
    {

    }

    void Stop()
    {
        
    }
};

class TLoadThread: public Thread {
public:
    TLoadThread(TPlayer* Player, string FilePath): Thread("LoadThread"), Player(Player), FilePath(FilePath){
    }

    virtual void ThreadProc() override
    {
        cout << "TLoadThread" << endl;
        AudioPacket audioPacket;
        VideoPacket videoPacket;
        HandlePackets(audioPacket, videoPacket);
        if (Player)
        {
            {
                unique_lock<std::mutex> lock(Player->AudioPacketLock.mtx);
                Player->AudioPacketQueue.push(audioPacket);
                Player->AudioPacketLock.cv.notify_all();
            }
            {
                unique_lock<std::mutex> lock(Player->VideoPacketLock.mtx);
                Player->VideoPacketQueue.push(videoPacket);
                Player->VideoPacketLock.cv.notify_all();
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
private:
    void HandlePackets( AudioPacket& audioPacket, VideoPacket& videoPacket) {
        static int packetId = 0;
        audioPacket.PacketId = packetId;
        videoPacket.PacketId = packetId;
        packetId++;
    }

private:
    TPlayer* Player;
    string FilePath;
};

class TAudioDecodeThread : public Thread {
public:
    TAudioDecodeThread(TPlayer*  Player):Thread("TAudioDecodeThread"), Player(Player) {}
    virtual void ThreadProc() override
    {
        while(true) {
            cout << "TAudioDecodeThread" << endl;
            
            unique_lock<std::mutex> lock(Player->AudioPacketLock.mtx);
            // 等线程队列长度不为0
            while (!Player->AudioDecodeQueue.empty())
            {
                Player->AudioPacketLock.cv.wait(lock);
            }
            const AudioPacket &packet = Player->AudioPacketQueue.front();
            Player->AudioPacketQueue.pop();

            AudioDecodeData data;

            DecodeAudio(packet, data);

            {
                unique_lock<std::mutex> lock(Player->AudioDecodeLock.mtx);
                Player->AudioDecodeQueue.push(data);
                Player->AudioDecodeLock.cv.notify_all(); //通知展示线程
            }
        }
    }

    bool DecodeAudio(const AudioPacket& packet, AudioDecodeData& data)
    {
        data.DecodePacketId = packet.PacketId;
        data.PTS = packet.PTS;
        return true;
    }

private:
    TPlayer* Player;
};

class TVideoDecodeThread : public Thread {
public:
    TVideoDecodeThread(const TPlayer*  Player): Thread("TVideoDecodeThread"),Player(Player) {}
    virtual void ThreadProc() override
    {
        cout << "TVideoDecodeThread" << endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    bool DecodeVideo(const VideoPacket& packet, VideoDecodeData& data)
    {
        data.DecodePacketId = packet.PacketId;
        data.PTS = packet.PTS;
        return true;
    }

private:
     const TPlayer* Player;
};


class TVideoPlayThread : public Thread {
public:
    TVideoPlayThread(const TPlayer* Player):Thread("TVideoPlayThread"), Player(Player) {}
    virtual void ThreadProc() override
    {
        cout << "TVideoPlayThread" << endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

private:
    const TPlayer* Player;
};


class TAudioPlayThread : public Thread {
public:
    TAudioPlayThread(TPlayer*  Player):Thread("TAudioPlayThread"), Player(Player) {}

    virtual void ThreadProc() override
    {
        cout << "TAudioPlayThread" << endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        {
            unique_lock<std::mutex> lock(Player->AudioDecodeLock.mtx);
            // 等线程队列长度不为0
            while (!Player->AudioDecodeQueue.empty())
            {
                Player->AudioDecodeLock.cv.wait(lock);
            }

            const AudioDecodeData &data = Player->AudioDecodeQueue.front();
            Player->AudioDecodeQueue.pop();
            cout << "Play Thread Get Audio " << data.DecodePacketId << endl;
        }
    }

private:
    TPlayer* Player;
};



void TPlayer::CreateLoadThread(const string& FilePath)
{
    cout << "CreateLoadThread" << endl;
    this->FilePath = FilePath;
    shared_ptr<TLoadThread> p(new TLoadThread(this, FilePath));
    Load = move(p);
    Load->Start();
}

void TPlayer::CreateDecodeThreads()
{
    cout << "CreateDecodeThreads" << endl;
    shared_ptr<TAudioDecodeThread> p(new TAudioDecodeThread(this));
    AudioDecode = move(p);
    AudioDecode->Start();

    shared_ptr<TVideoDecodeThread> p1(new TVideoDecodeThread(this));
    VideoDecode = move(p1);
    VideoDecode->Start();
}

void TPlayer::CreatePlayThreads()
{
    cout << "CreatePlayThreads" << endl;
    shared_ptr<TAudioPlayThread> p(new TAudioPlayThread(this));
    AudioPlay = move(p);
    AudioPlay->Start();

    shared_ptr<TVideoPlayThread> p1(new TVideoPlayThread(this));
    VideoPlay = move(p1);
    VideoPlay->Start();
}

void TPlayer::Open(const string& FilePath) {
    CreateLoadThread(FilePath);
    CreateDecodeThreads();
    CreatePlayThreads();
    Load->Join();
}



int main()
{
    shared_ptr<TPlayer> Player(new TPlayer());
    Player->Open("======> file");
    return 0;
}