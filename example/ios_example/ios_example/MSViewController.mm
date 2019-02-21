//
//  MSViewController.m
//  ios_example
//
//  Created by 胡校明 on 2018/12/5.
//  Copyright © 2018 freecoder. All rights reserved.
//

#import "MSViewController.h"
#import <iostream>
#import <MediaStreamPlayer.h>

#import "IotlibTool.h"
#import "NVGLPlayView.h"
#import "DDOpenAlAudioPlayer.h"

using namespace std;
using namespace MS;
using namespace MS::FFmpeg;
using namespace MS::APhard;

@interface MSViewController ()<IotlibToolDelegate>
{
//    MSPlayer<AVFrame> *player;
    MSPlayer<APFrame> *player;
    BOOL updateVideo;
    BOOL updateAudio;
}
@property (weak, nonatomic) IBOutlet UILabel *displayLabel;
@property (weak, nonatomic) IBOutlet NVGLPlayView *displayView;

@end

@implementation MSViewController

static int i;
//static int j;

- (void)viewDidLoad {
    [super viewDidLoad];
    self.displayView.transform = CGAffineTransformRotate(CGAffineTransformIdentity, M_PI);
    self.displayView.layer.transform = CATransform3DRotate(CATransform3DIdentity, M_PI, UIScreen.mainScreen.bounds.size.width/2, 0, 0);
    [self.displayView setupGL];
    
//    auto decoder = new FFDecoder();
//    auto encoder = new FFEncoder(MSCodecID_H264,MSCodecID_AAC);
//    player = new MSPlayer<AVFrame>(decoder,encoder,
//                                   [&](const MSMedia<isDecode,AVFrame> &data) {
//                                       printf("data time: %lld\n", data.timeInterval.count());
//                                   },
//                                   [&](const MSMedia<isDecode,AVFrame> &data) {
//
//                                   });
    
    __weak typeof(MSViewController *) weakSelf = self;
    
    auto decoder = new APDecoder();
//    auto encoder = new APEncoder(MSCodecID_H264,MSCodecID_AAC);
    player = new MSPlayer<APFrame>(decoder,nullptr,
                                   [weakSelf](const MSMedia<MSDecodeMedia,APFrame> &data) {
                                       if (data.frame) {
//                                           dispatch_async(dispatch_get_main_queue(), ^{
//                                               [[weakSelf displayLabel] setText:[NSString stringWithFormat:@"video: %d", i++]];
//                                           });
//                                           printf("data time: %lld\n", data.timeInterval.count());
                                           [[weakSelf displayView] displayPixelBuffer:data.frame->video];
                                       }
                                   },
                                   [&](const MSMedia<MSDecodeMedia,APFrame> &data) {
                                       if (data.frame) {
//                                           printf("audio time: %lld\n", data.timeInterval.count());
                                           AudioBuffer &audio = *data.frame->audio;
                                           [[DDOpenALAudioPlayer sharePalyer] openAudioFromQueue:(uint8_t *)audio.mData
                                                                                        dataSize:audio.mDataByteSize
                                                                                      samplerate:8000
                                                                                        channels:audio.mNumberChannels
                                                                                             bit:16];
                                       }
                                   });
    
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(connectsuccess:)
                                                 name:CONNECTSUCCESS
                                               object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(connectfail:)
                                                 name:CONNECTFAIL
                                               object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(connectfail:)
                                                 name:CONNECTSTOPPED
                                               object:nil];
    [IotlibTool shareIotlibTool].delegate = self;

    // IOTSHMK000S00004EDA785C
    [[IotlibTool shareIotlibTool] startConnectWithDeviceId:@"IOTSHMK000S0008EDA1FCDD"
                                                  callback:^(e_trans_conn_state status,
                                                             int connectId)
     {
         printf("------------connectId: %d\n",connectId);
     }];
}

- (void)connectfail:(NSNotification *)notify {
    printf("------------conn fail\n");
}

- (void)connectsuccess:(NSNotification *)notify {
    printf("------------conn success\n");
    NSDictionary *dic = (NSDictionary *)notify.object;
    int connID = [dic[@"connID"] intValue];
    [[IotlibTool shareIotlibTool] startVideoWithConnectId:connID
                                                     chno:1
                                             videoQuality:500
                                                 callback:^(int status)
    {
        printf("video: -----------------status: %d\n", status);
    }];
    
    [[IotlibTool shareIotlibTool] startAudioWithChannelID:connID Callback:^(int status,
                                                                            uint32_t audio_codec,
                                                                            uint32_t rate,
                                                                            uint32_t bit,
                                                                            uint32_t track) {
        printf("audio: -----------------status: %d\n", status);
    }];
}

- (void)onMediaDataWithConnId:(uint32_t)connId
                  headerMedia:(header_media_t *)headerMedia
                      dataPtr:(const char *)data_ptr
                      dataLen:(uint32_t)dataLen {
//    printf("--------------datalen: %d\n",dataLen);
    if (updateVideo) {
        if (headerMedia->stream_type == e_stream_type_H264) {
            auto data = new MSMedia<MSEncodeMedia>((uint8_t *)data_ptr,dataLen,headerMedia->is_key_frame,MSCodecID_H264);
            player->pushVideoStreamData(data);
        }
    }
    
    if (updateAudio) {
        if (headerMedia->stream_type == e_stream_type_AAC) {
            auto data = new MSMedia<MSEncodeMedia>((uint8_t *)data_ptr,dataLen,headerMedia->is_key_frame,MSCodecID_AAC);
            player->pushAudioStreamData(data);
        }
    }
}



- (IBAction)playVideo:(UIButton *)sender {
    i = 0;
    player->startPlayVideo();
}

- (IBAction)pauseVideo:(UIButton *)sender {
    player->pausePlayVideo();
}

- (IBAction)continueVideo:(UIButton *)sender {
    player->continuePlayVideo();
}

- (IBAction)stopVideo:(UIButton *)sender {
    player->stopPlayVideo();
}

- (IBAction)updateVideo:(UIButton *)sender {
    updateVideo = !updateVideo;
}



- (IBAction)playAudio:(UIButton *)sender {
    player->startPlayAudio();
}

- (IBAction)pauseAudio:(UIButton *)sender {
    player->pausePlayAudio();
}

- (IBAction)continueAudio:(UIButton *)sender {
    player->continuePlayAudio();
}

- (IBAction)stopAudio:(UIButton *)sender {
    player->stopPlayAudio();
}

- (IBAction)updateAudio:(UIButton *)sender {
    updateAudio = !updateAudio;
}



- (IBAction)encodeMedia:(UIButton *)sender {
    FFEncoder &encoder = (FFEncoder &)player->syncEncoder();
    FFDecoder &decoder = (FFDecoder &)player->syncDecoder();
    if (encoder.isEncoding()) {
        player->stopReEncode();
    } else {
        NSString *videoPath = [[NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) lastObject] stringByAppendingPathComponent:[NSString stringWithFormat:@"test.mp4"]];
        bool ret = encoder.configureEncoder(videoPath.UTF8String,
                                            decoder.findDecoderContext(MSCodecID_H264),
                                            decoder.findDecoderContext(MSCodecID_AAC));
        if (ret) {
            player->startReEncode();
        }
    }
}




- (IBAction)back:(UIButton *)sender {
    [self dismissViewControllerAnimated:true completion:^{
        
    }];
}


- (void)dealloc {
    printf("----delloc\n");
    delete player;
}

@end
