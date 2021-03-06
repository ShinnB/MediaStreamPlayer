//
//  MSTimer.hpp
//  MediaStreamPlayer
//
//  Created by xiaoming on 2018/11/22.
//  Copyright © 2018 freecoder. All rights reserved.
//

#ifndef MSTimer_hpp
#define MSTimer_hpp

#include <thread>
#include <future>
#include <chrono>
#include <mutex>
#include <condition_variable>

namespace MS {

    typedef std::function<void()> TaskType;
    
    using namespace std;
    
    using namespace chrono;
    
    class MSTimer {
        thread timerThread;
        
        thread taskThread;
        
        mutex mtx;
        
        condition_variable condition;
        
        bool isRunning = false;
        
        bool isPausing = false;
        
        microseconds delayTime;
        
        microseconds timeInterval;
        
        microseconds deviation;
        
        TaskType task;
    public:
        MSTimer(const microseconds delayTime,
                const microseconds timeInterval,
                const TaskType task);
        
        ~MSTimer();

        void start();
        
        void pause();
        
        void rePlay();
        
        void stop();
        
        bool isActivity();
        
        microseconds getTimeInterval();
        
        MSTimer & updateTask(const TaskType task);
        
        MSTimer & updateDelayTime(const microseconds delayTime);
        
        MSTimer & updateTimeInterval(const microseconds timeInterval);
    };
    
}

#endif /* MSTimer_hpp */
