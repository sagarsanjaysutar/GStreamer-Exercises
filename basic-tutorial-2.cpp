/*!
 * @brief Exploring bus messages and testing out more GST Elements
 * @author Sagar Sutar
 * @link https://gstreamer.freedesktop.org/documentation/tutorials/basic/concepts.html?gi-language=c
 */
#include <gst/gst.h>
#include <iostream>

using std::cout;
using std::endl;

int main()
{
    GstElement *pipeline, *source, *sink;
    GstBus *bus;
    GstMessage *msg;
    GstStateChangeReturn ret;

    gst_init(NULL, NULL);

    // Create elements
    source = gst_element_factory_make("videotestsrc", "source");
    sink = gst_element_factory_make("autovideosink", "sink");

    // Create empty pipelines
    pipeline = gst_pipeline_new("test-pipeline");

    if (!pipeline || !source || !sink)
    {
        cout << "Failed to create gst-elements" << endl;
        return -1;
    }

    // Add element to pipeline and link it.
    gst_bin_add_many(GST_BIN(pipeline), source, sink, NULL);
    if (gst_element_link(source, sink) != TRUE)
    {
        cout << "Failed to link gst-elements. " << endl;
        gst_object_unref(pipeline);
        return -1;
    }

    // Modify the "videotestsrc" property
    g_object_set(source, "pattern", 0, NULL);

    // Start playing
    if (gst_element_set_state(pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
    {
        cout << "Failed to start the gst-element" << endl;
        gst_object_unref(pipeline);
        return -1;
    }

    // Wait for the End of Stream
    bus = gst_element_get_bus(pipeline);
    msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_ERROR);

    if (msg != NULL)
    {
        GError *err;
        gchar *debug_info;

        switch (GST_MESSAGE_TYPE(msg))
        {
        case GST_MESSAGE_ERROR:
        {
            gst_message_parse_error(msg, &err, &debug_info);
            cout << "GST message error " << msg->src << "\t" << err->message << endl;
            cout << "Debug info " << debug_info << endl;
            g_clear_error(&err);
            g_free(debug_info);
            break;
        }
        case GST_MESSAGE_EOS:
        {
            cout << "End of stream" << endl;
            break;
        }
        default:
            cout << "Something went wrong. " << endl;
            break;
        }
        gst_object_unref(msg);
    }

    // Deallocate
    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);

    return 0;
}