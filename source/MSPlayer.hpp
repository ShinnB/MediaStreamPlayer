//
//  MSPlayer
//  MediaStreamPlayer
//
//  Created by 胡校明 on 2018/11/14.
//  Copyright © 2018 freecoder. All rights reserved.
//

#ifndef MSPlayer_hpp
#define MSPlayer_hpp

#include <string>
#include <queue>
#include <cassert>
#include "MSTimer.hpp"
#include "MSAsynDataProtocol.h"
#include "MSCodecSyncProtocol.h"
#include "MSCodecAsynProtocol.h"

namespace MS {
    
    using namespace std;
    
    using namespace chrono;

#pragma mark - MSPlayer<T>(declaration)
    template <typename T>
    class MSPlayer : public MSAsynDataProtocol<T> {
        typedef function<void(const MSMediaData<isDecode,T> &decodeData)> ThrowDecodeData;
        
        MSSyncDecoderProtocol<T> * const syncDecoder;
        
        MSSyncEncoderProtocol<T> * const syncEncoder;
        
        MSTimer * const videoTimer = new MSTimer(microseconds(0),intervale(1),nullptr);
        
        MSTimer * const audioTimer = new MSTimer(microseconds(0),intervale(1),nullptr);
        
        thread videoDecodeThread;
        
        thread audioDecodeThread;
        
        condition_variable videoThreadCondition;
        
        condition_variable audioThreadCondition;
        
        mutex videoConditionMutex;
        
        mutex audioConditionMutex;
        
        mutex videoQueueMutex;
        
        mutex pixelQueueMutex;
        
        mutex audioQueueMutex;
        
        mutex sampleQueueMutex;
        
        queue<const MSMediaData<isEncode> *> videoQueue;
        
        queue<const MSMediaData<isEncode> *> audioQueue;
        
        queue<const MSMediaData<isDecode, T> *> pixelQueue;
        
        queue<const MSMediaData<isDecode, T> *> sampleQueue;
        
        bool isDecoding = true;
        
        bool isEncoding = false;
        
        const ThrowDecodeData throwDecodeVideo;
        
        const ThrowDecodeData throwDecodeAudio;
        
        void clearAllVideo();
        
        void clearAllAudio();
        
    public:
        MSPlayer(MSSyncDecoderProtocol<T> * const decoder,
                 MSSyncEncoderProtocol<T> * const encoder,
                 const ThrowDecodeData throwDecodeVideo,
                 const ThrowDecodeData throwDecodeAudio);
        
        MSPlayer(MSAsynDecoderProtocol<T> * const decoder,
                 MSAsynEncoderProtocol<T> * const encoder,
                 const ThrowDecodeData throwDecodeVideo,
                 const ThrowDecodeData throwDecodeAudio);
        
        ~MSPlayer();
        
        MSSyncDecoderProtocol<T> & getDecoder();
        
        MSSyncEncoderProtocol<T> & getEncoder();
        
        void startPlayVideo();
        
        void pausePlayVideo();
        
        void continuePlayVideo();
        
        void stopPlayVideo();
        
        void startPlayAudio();

        void pausePlayAudio();

        void continuePlayAudio();

        void stopPlayAudio();
        
        void startReEncode();
        
        void pauseReEncode();
        
        void continueReEncode();
        
        void stopReEncode();
        
        void pushVideoStreamData(MSMediaData<isEncode> *streamData);
        
        void pushAudioStreamData(MSMediaData<isEncode> *streamData);
        
    private: // 不允许外部主调, 请通过 Protocol 进行多态调用
        void asynPushVideoFrameData(const MSMediaData<isDecode, T> * const frameData);
        
        void asynPushAudioFrameData(const MSMediaData<isDecode, T> * const frameData);
    };
    
#pragma mark - MSPlayer<T>(implementation)
    template <typename T>
    MSPlayer<T>::MSPlayer(MSSyncDecoderProtocol<T> * const decoder,
                          MSSyncEncoderProtocol<T> * const encoder,
                          const ThrowDecodeData throwDecodeVideo,
                          const ThrowDecodeData throwDecodeAudio)
    :syncDecoder(decoder), syncEncoder(encoder),
    throwDecodeVideo(throwDecodeVideo),
    throwDecodeAudio(throwDecodeAudio) {
        assert(throwDecodeVideo && throwDecodeAudio);
        
        videoDecodeThread = thread([this](){
            const MSMediaData<isEncode> *sourceData = nullptr;
            const MSMediaData<isDecode,T> *frameData = nullptr;
            while (isDecoding) {
                while (videoQueue.empty() || pixelQueue.size() > 20) {
                    unique_lock<mutex> lock(videoConditionMutex);
                    videoThreadCondition.wait(lock);
                    if (!isDecoding) break;
                }
                if (!isDecoding) break;
                sourceData = videoQueue.front();
                while (!videoQueueMutex.try_lock());
                videoQueue.pop();
                videoQueueMutex.unlock();
                frameData = syncDecoder->decodeVideo(*sourceData);
                if (frameData) {
                    while (!pixelQueueMutex.try_lock());
                    pixelQueue.push(frameData);
                    pixelQueueMutex.unlock();
                }
                delete sourceData;
            }
        });
        
        audioDecodeThread = thread([this](){
            const MSMediaData<isEncode> *sourceData = nullptr;
            const MSMediaData<isDecode,T> *frameData = nullptr;
            while (isDecoding) {
                while (audioQueue.empty() || sampleQueue.size() > 20) {
                    unique_lock<mutex> lock(audioConditionMutex);
                    audioThreadCondition.wait(lock);
                    if (!isDecoding) break;
                }
                if (!isDecoding) break;
                sourceData = audioQueue.front();
                while (!audioQueueMutex.try_lock());
                audioQueue.pop();
                audioQueueMutex.unlock();
                frameData = syncDecoder->decodeAudio(*sourceData);
                if (frameData) {
                    while (!sampleQueueMutex.try_lock());
                    sampleQueue.push(frameData);
                    sampleQueueMutex.unlock();
                }
                delete sourceData;
            }
        });
        
        videoTimer->updateTask([this](){
            if (!pixelQueue.empty()) {
                const MSMediaData<isDecode,T> *frameData = nullptr;
                frameData = pixelQueue.front();
                while (!pixelQueueMutex.try_lock());
                pixelQueue.pop();
                pixelQueueMutex.unlock();
                if (pixelQueue.size() < 5) {
                    videoThreadCondition.notify_one();
                }
                videoTimer->updateTimeInterval(frameData->content->timeInterval);
                this->throwDecodeVideo(*frameData);
                if (isEncoding) {
                    syncEncoder->encodeVideo(*frameData);
                }
                delete frameData;
            } else {
                this->throwDecodeVideo(MSMediaData<isDecode,T>::defaultNullData);
                videoThreadCondition.notify_one();
            }
        });
        
        audioTimer->updateTask([this](){
            if (!sampleQueue.empty()) {
                const MSMediaData<isDecode,T> *frameData = nullptr;
                frameData = sampleQueue.front();
                while (!sampleQueueMutex.try_lock());
                sampleQueue.pop();
                sampleQueueMutex.unlock();
                if (sampleQueue.size() < 5) {
                    audioThreadCondition.notify_one();
                }
                audioTimer->updateTimeInterval(frameData->content->timeInterval);
                this->throwDecodeAudio(*frameData);
                if (isEncoding) {
                    syncEncoder->encodeAudio(*frameData);
                }
                delete frameData;
            } else {
                this->throwDecodeAudio(MSMediaData<isDecode,T>::defaultNullData);
                audioThreadCondition.notify_one();
            }
        });
    }
    
    template <typename T>
    MSPlayer<T>::~MSPlayer() {
        stopPlayVideo();
        stopPlayAudio();
        isDecoding = false;
        videoThreadCondition.notify_one();
        if (videoDecodeThread.joinable()) {
            videoDecodeThread.join();
        }
        audioThreadCondition.notify_one();
        if (audioDecodeThread.joinable()) {
            audioDecodeThread.join();
        }
        delete syncDecoder;
        delete syncEncoder;
        delete videoTimer;
    }
    
    template <typename T>
    MSSyncDecoderProtocol<T> &
    MSPlayer<T>::getDecoder() {
        return *syncDecoder;
    }
    
    template <typename T>
    MSSyncEncoderProtocol<T> &
    MSPlayer<T>::getEncoder() {
        return *syncEncoder;
    }
    
    template <typename T>
    void MSPlayer<T>::clearAllVideo() {
        const MSMediaData<isEncode> *encodeData = nullptr;
        const MSMediaData<isDecode,T> *decodeData = nullptr;
        while (!videoQueue.empty()) {
            encodeData = videoQueue.front();
            videoQueue.pop();
            delete encodeData;
        }
        while (!pixelQueue.empty()) {
            decodeData = pixelQueue.front();
            pixelQueue.pop();
            delete decodeData;
        }
    }
    
    template <typename T>
    void MSPlayer<T>::clearAllAudio() {
        const MSMediaData<isEncode> *encodeData = nullptr;
        const MSMediaData<isDecode,T> *decodeData = nullptr;
        while (!audioQueue.empty()) {
            encodeData = audioQueue.front();
            audioQueue.pop();
            delete encodeData;
        }
        while (!sampleQueue.empty()) {
            decodeData = sampleQueue.front();
            sampleQueue.pop();
            delete decodeData;
        }
    }
    
    template <typename T>
    void MSPlayer<T>::startPlayVideo() {
        assert(videoTimer->isValid() == false);
        microseconds timeInterval = audioTimer->getTimeInterval();
        if (timeInterval != intervale(1)) { // 同步音视频播放时间
            videoTimer->updateDelayTime((audioQueue.size() + sampleQueue.size()) * timeInterval);
        }
        videoTimer->start();
    }
    
    template <typename T>
    void MSPlayer<T>::pausePlayVideo() {
        videoTimer->pause();
    }
    
    template <typename T>
    void MSPlayer<T>::continuePlayVideo() {
        videoTimer->_continue();
    }
    
    template <typename T>
    void MSPlayer<T>::stopPlayVideo() {
        videoTimer->stop();
        isEncoding = false;
        clearAllVideo();
    }
    
    template <typename T>
    void MSPlayer<T>::startPlayAudio() {
        assert(audioTimer->isValid() == false);
        microseconds timeInterval = videoTimer->getTimeInterval();
        if (timeInterval != intervale(1)) { // 同步音视频播放时间
            audioTimer->updateDelayTime((videoQueue.size() + pixelQueue.size()) * timeInterval);
        }
        audioTimer->start();
    }
    
    template <typename T>
    void MSPlayer<T>::pausePlayAudio() {
        audioTimer->pause();
    }
    
    template <typename T>
    void MSPlayer<T>::continuePlayAudio() {
        audioTimer->_continue();
    }
    
    template <typename T>
    void MSPlayer<T>::stopPlayAudio() {
        audioTimer->stop();
        isEncoding = false;
        clearAllAudio();
    }
    
    template <typename T>
    void MSPlayer<T>::startReEncode() {
        assert(syncEncoder->isEncoding() == false);
        syncEncoder->beginEncode();
        isEncoding = true;
    }
    
    template <typename T>
    void MSPlayer<T>::pauseReEncode() {
        isEncoding = false;
    }
    
    template <typename T>
    void MSPlayer<T>::continueReEncode() {
        isEncoding = true;
    }
    
    template <typename T>
    void MSPlayer<T>::stopReEncode() {
        isEncoding = false;
        syncEncoder->endEncode();
    }
    
    template <typename T>
    void MSPlayer<T>::pushVideoStreamData(MSMediaData<isEncode> *streamData) {
        if (videoTimer->isValid()) {
            while (!videoQueueMutex.try_lock());
            videoQueue.push(streamData);
            videoQueueMutex.unlock();
        } else {
            delete streamData;
        }
    }
    
    template <typename T>
    void MSPlayer<T>::pushAudioStreamData(MSMediaData<isEncode> *streamData) {
        if (audioTimer->isValid()) {
            while (!audioQueueMutex.try_lock());
            audioQueue.push(streamData);
            audioQueueMutex.unlock();
        } else {
            delete streamData;
        }
    }
    
    template <typename T>
    void MSPlayer<T>::asynPushVideoFrameData(const MSMediaData<isDecode, T> * const frameData) {
        while (!pixelQueueMutex.try_lock());
        pixelQueue.push(frameData);
        pixelQueueMutex.unlock();
    }
    
    template <typename T>
    void MSPlayer<T>::asynPushAudioFrameData(const MSMediaData<isDecode, T> * const frameData) {
        while (!sampleQueueMutex.try_lock());
        sampleQueue.push(frameData);
        sampleQueueMutex.unlock();
    }
    
}

#endif /* MSPlayer_hpp */
