/*!
    @brief Displaying a video from link.
    @author Sagar Sutar
    @link https://gstreamer.freedesktop.org/documentation/tutorials/basic/hello-world.html
 */

#include <gst/gst.h>

int main()
{
    GstElement *pipeline;
    GstBus *bus;
    GstMessage *msg;

    gst_init(NULL, NULL);

    // Used when pipeline is straightforward. More like a inline pipeline
    pipeline = gst_parse_launch("playbin uri=https://www.freedesktop.org/software/gstreamer-sdk/data/media/sintel_trailer-480p.webm", NULL);
    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    // bus carries the messages.
    bus = gst_element_get_bus(pipeline);
    msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_EOS);

    if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR)
    {
        g_error("Error occured.");
    }

    gst_message_unref(msg);
    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    return 0;
}