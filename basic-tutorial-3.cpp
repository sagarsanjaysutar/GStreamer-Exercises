/*!
 * @brief Working with Dynamic Pipelines e.g. Demuxers
 * @author Sagar Sutar
 * @link https://gstreamer.freedesktop.org/documentation/tutorials/basic/dynamic-pipelines.html?gi-language=c
 *
 * Usually we build a static pipeline where the is 1 source pad - multiple filters - 1 sink pad.
 * Here both the source & sink element have 1 pad while filter has both.
 * Now when it comes to Demuxers, it has 1 sink(to consume data) and 2 source pad(to dish out the seperated data).
 * But we can't build a static pipeline with a demuxer because it requires the data to be passed first,
 * and then and then only it will create source pads to provide the output.
 */

#include <iostream>
#include <gst/gst.h>

using std::cout;
using std::endl;

typedef struct
{
    GstElement *pipeline, *source, *convert, *resample, *sink;
} CustomData;

static void pad_added_handler(GstElement *source, GstPad *new_pad, CustomData *data)
{
    GstPad *sink_pad = gst_element_get_static_pad(data->convert, "sink");
    GstCaps *new_pad_caps = NULL;
    GstStructure *new_pad_struct = NULL;
    const gchar *new_pad_type = NULL;
    GstPadLinkReturn ret;

    cout << "Received new pad " << GST_PAD_NAME(new_pad) << " from " << GST_ELEMENT_NAME(source) << endl;

    // If converter is already linked, exit.
    if (gst_pad_is_linked(sink_pad))
    {
        cout << "We are already linked." << endl;
        gst_object_unref(sink_pad);
    }

    // Check new pads type
    new_pad_caps = gst_pad_get_current_caps(new_pad);
    new_pad_struct = gst_caps_get_structure(new_pad_caps, 0);
    new_pad_type = gst_structure_get_name(new_pad_struct);
    if (!g_str_has_prefix(new_pad_type, "audio/x-raw"))
    {
        cout << "Not a raw audio. Invalid type: " << new_pad_type << endl;
        gst_caps_unref(new_pad_caps);
        gst_object_unref(sink_pad);
    }

    // Attempt linking
    ret = gst_pad_link(new_pad, sink_pad);
    if (GST_PAD_LINK_FAILED(ret))
    {
        cout << new_pad_type << " type linking failed." << endl;
    }
    else
    {
        cout << new_pad_type << " type linked successfully." << endl;
    }
}

int main()
{
    CustomData data;
    GstBus *bus;
    GstMessage *msg;
    gboolean terminateLoop = FALSE;

    gst_init(NULL, NULL);

    // Create the gst element and empty pipeline
    data.source = gst_element_factory_make("uridecodebin", "source");
    data.convert = gst_element_factory_make("audioconvert", "convert");
    data.resample = gst_element_factory_make("audioresample", "resample");
    data.sink = gst_element_factory_make("autoaudiosink", "sink");
    data.pipeline = gst_pipeline_new("dynamic-pipeline");

    if (
        !data.source ||
        !data.convert ||
        !data.resample ||
        !data.sink ||
        !data.pipeline)
    {
        cout << "Couldn't create GST pipeline or element." << endl;
        return -1;
    }

    // Build the pipeline.
    gst_bin_add_many(GST_BIN(data.pipeline), data.source, data.convert, data.resample, data.sink, NULL);

    // Linking elements. We'll link the source later.
    if (!gst_element_link_many(data.convert, data.resample, data.sink, NULL))
    {
        cout << "Couldn't link GST elements." << endl;
        gst_object_unref(data.pipeline);
        return -1;
    }

    // Set the URI to play i.e. Passing the Data first.
    g_object_set(data.source, "uri", "https://www.freedesktop.org/software/gstreamer-sdk/data/media/sintel_trailer-480p.webm", NULL);

    // Connect to the pad-added signal i.e. Linking source
    g_signal_connect(data.source, "pad-added", G_CALLBACK(pad_added_handler), &data);

    // Start playing
    if (gst_element_set_state(data.pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
    {
        cout << "Failed to start the GST Pipeline." << endl;
        gst_object_unref(data.pipeline);
        return -1;
    }

    // Listen to the bus.
    bus = gst_element_get_bus(data.pipeline);
    do
    {
        msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
                                         (GstMessageType)(GST_MESSAGE_STATE_CHANGED | GST_MESSAGE_ERROR | GST_MESSAGE_EOS));
        if (msg != NULL)
        {
            GError *err;
            gchar *debug_info;

            switch (GST_MESSAGE_TYPE(msg))
            {
            case GST_MESSAGE_ERROR:
            {
                gst_message_parse_error(msg, &err, &debug_info);
                gst_printerr("Error Message: \n%s:\n %s", GST_OBJECT_NAME(msg->src), err->message);
                gst_printerr("Debug Message: \n%s", debug_info ? debug_info : "none");
                g_clear_error(&err);
                g_free(debug_info);
                terminateLoop = TRUE;
                break;
            }
            case GST_MESSAGE_EOS:
            {
                cout << "End of stream reached." << endl;
                terminateLoop = TRUE;
                break;
            }
            case GST_MESSAGE_STATE_CHANGED:
            {
                // We are only intererted in state-changed messages from pipeline.
                if (GST_MESSAGE_SRC(msg) == GST_OBJECT(data.pipeline))
                {
                    GstState old_state, new_state, pending_state;
                    gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
                    cout << "Pipeline state changed from " << old_state << " to " << new_state << endl;
                }
                break;
            }
            default:
            {
                cout << "Unexpected meesage." << endl;
                break;
            }
            }
            gst_message_unref(msg);
        }
    } while (!terminateLoop);

    // Deallocate
    gst_object_unref(bus);
    gst_element_set_state(data.pipeline, GST_STATE_NULL);
    gst_object_unref(data.pipeline);

    return 0;
}
