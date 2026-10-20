#include <SDL.h>
#include <cmath>
#include <cstdio>
#include <vector>
int main(){
 if(SDL_Init(SDL_INIT_AUDIO))return 1;
 for(int i=0;i<SDL_GetNumAudioDrivers();++i)std::printf("driver %s\n",SDL_GetAudioDriver(i));
 SDL_AudioSpec want{},got{};want.freq=44100;want.format=AUDIO_S16SYS;want.channels=2;want.samples=1024;
 auto dev=SDL_OpenAudioDevice(nullptr,0,&want,&got,0);
 std::printf("audio=%s dev=%u freq=%d channels=%u format=%x samples=%u error=%s\n",SDL_GetCurrentAudioDriver(),dev,got.freq,got.channels,got.format,got.samples,SDL_GetError());
 if(!dev)return 2;
 std::vector<int16_t> data(44100*2*4);
 for(size_t i=0;i<data.size()/2;++i)data[2*i]=data[2*i+1]=int16_t(std::sin(i*2*3.141592653589793*440/44100)*6000);
 auto rc=SDL_QueueAudio(dev,data.data(),data.size()*2);SDL_PauseAudioDevice(dev,0);
 std::printf("queue=%d status=%d bytes=%u\n",rc,SDL_GetAudioDeviceStatus(dev),SDL_GetQueuedAudioSize(dev));
 for(int i=0;i<10;++i){SDL_Delay(500);std::printf("queued=%u\n",SDL_GetQueuedAudioSize(dev));}
 SDL_CloseAudioDevice(dev);SDL_Quit();
}
