#include <gst/gst.h>

typedef struct
{
    GstElement *pipeline, *videoSource, *audioSource;
    GstElement *videoQueue, *videoConvert, *videoEncoder, *videoParser;
    GstElement *audioQueue, *audioConvert, *audioEncoder, *audioVideoMuxer;
    GstElement *fileSink;

    GstPad *queueAudioPad;
    GstPad *queueVideoPad;

    GMainLoop mainLoop;
} CustomData;

int main()
{
    CustomData data;

    gst_init(NULL, NULL);

    GstCaps *videoCaps = gst_caps_from_string("video/x-raw, format=YUY2, width=640, height=480");
    GstCaps *audioCaps = gst_caps_from_string("audio/x-raw, format=S16, rate=44100, channels=1");

    return 0;
}