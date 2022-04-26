/*!
 * @brief Worked with "On Request Pad" instead of "Always available" and did manual linking of those pads.
 * Added multi threading by introducing Queue elements.
 * @note Refer diagram on the tutorials page. Straight forward pipeline.
 * @link https://gstreamer.freedesktop.org/documentation/tutorials/basic/multithreading-and-pad-availability.html?gi-language=c
 */

#include <gst/gst.h>

typedef struct
{
    GstElement *pipeline, *audioSource, *tee, *audioQueue, *audioConvert, *audioResample, *audioSink;
    GstElement *videoQueue, *visualizer, *videoConvert, *videoSink;
    GstPad *teeAudioPad, *teeVideoPad;
    GstPad *queueAudioPad, *queueVideoPad;

} CustomData;

int main()
{
    CustomData data;

    gst_init(NULL, NULL);

    // Create Elements
    data.audioSource = gst_element_factory_make("audiotestsrc", NULL);
    data.tee = gst_element_factory_make("tee", NULL);
    data.audioQueue = gst_element_factory_make("queue", NULL);
    data.audioConvert = gst_element_factory_make("audioconvert", NULL);
    data.audioResample = gst_element_factory_make("audioresample", NULL);
    data.audioSink = gst_element_factory_make("autoaudiosink", NULL);
    data.videoQueue = gst_element_factory_make("queue", NULL);
    data.visualizer = gst_element_factory_make("wavescope", NULL);
    data.videoConvert = gst_element_factory_make("videoconvert", NULL);
    data.videoSink = gst_element_factory_make("autovideosink", NULL);

    // Create empty pipeline
    data.pipeline = gst_pipeline_new("another-pipeline");

    if (!data.audioSource ||
        !data.tee ||
        !data.audioQueue ||
        !data.audioConvert ||
        !data.audioResample ||
        !data.audioSink ||
        !data.videoQueue ||
        !data.visualizer ||
        !data.videoConvert ||
        !data.videoSink)
    {
        gst_printerr("\nFailed to make one of the GST elements.");
        return -1;
    }

    // Set element properties
    g_object_set(data.audioSource, "freq", 235.0f, NULL);
    g_object_set(data.visualizer, "shader", 0, NULL);

    // Add to elements to bin
    gst_bin_add_many(GST_BIN(data.pipeline),
                     data.audioSource,
                     data.tee,
                     data.audioQueue,
                     data.audioConvert,
                     data.audioResample,
                     data.audioSink,
                     data.videoQueue,
                     data.visualizer,
                     data.videoConvert,
                     data.videoSink,
                     NULL);

    // #00 Link "Always Avialable" elements.
    if (!gst_element_link_many(data.audioSource, data.tee, NULL) ||
        !gst_element_link_many(data.audioQueue, data.audioConvert, data.audioResample, data.audioSink, NULL) ||
        !gst_element_link_many(data.videoQueue, data.visualizer, data.videoConvert, data.videoSink, NULL))
    {
        gst_printerr("\nUnable to link 'Always Avialable' GST Elements");
        gst_object_unref(data.pipeline);
        return -1;
    }

    // Creating audio & video source pads in tee element.
    data.teeAudioPad = gst_element_get_request_pad(data.tee, "src_%u");
    data.teeVideoPad = gst_element_get_request_pad(data.tee, "src_%u");

    // Creating audio & video sink pads in queue element
    data.queueAudioPad = gst_element_get_static_pad(data.audioQueue, "sink");
    data.queueVideoPad = gst_element_get_static_pad(data.videoQueue, "sink");

    // #01 Link "On Request" elements.
    if (gst_pad_link(data.teeAudioPad, data.queueAudioPad) != GST_PAD_LINK_OK ||
        gst_pad_link(data.teeVideoPad, data.queueVideoPad) != GST_PAD_LINK_OK)
    {
        gst_printerr("\nUnable to link 'On Request' GST Elements");
        gst_object_unref(data.pipeline);
        return -1;
    }

    // Start the pipeline
    if (gst_element_set_state(data.pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
    {
        gst_printerr("\nUnable to start the pipeline.");
        gst_object_unref(data.pipeline);
        return -1;
    }

    // Only watch for Error or EOS
    GstBus *bus = gst_element_get_bus(data.pipeline);
    GstMessage *msg = gst_bus_timed_pop_filtered(
        bus,
        GST_CLOCK_TIME_NONE,
        (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));

    // Dereference
    gst_element_release_request_pad(data.tee, data.teeAudioPad);
    gst_element_release_request_pad(data.tee, data.teeVideoPad);
    gst_object_unref(data.teeAudioPad);
    gst_object_unref(data.teeVideoPad);
    gst_object_unref(data.queueAudioPad);
    gst_object_unref(data.queueVideoPad);
    gst_message_unref(msg);
    gst_object_unref(bus);
    gst_element_set_state(data.pipeline, GST_STATE_NULL);
    gst_object_unref(data.pipeline);

    return 0;
}