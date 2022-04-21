/*!
* @note To make a pipeline which streams video from a camera, we first pick up the camera name(/dev/video*) by running
`v4l2-ctl --list-devices` and then to see supported resolution we run `v4l2-ctl --list-formats-ext --device path/to/video_device`
 */
#include <gst/gst.h>
#include <stdio.h>

typedef struct
{
    GstElement *pipeline, *source, *capsFilter, *converter, *encoder, *parser, *mux, *sink;
    GstElementFactory *sourceFactory, *capsFilterFactory, *converterFactory, *encoderFactory, *parserFactory, *muxFactory, *sinkFactory;
    GMainLoop *mainLoop;
    gboolean playing;
} CustomData;
/* ======= Helper functions ==========*/
/* Functions below print the Capabilities in a human-friendly format */
static gboolean print_field(GQuark field, const GValue *value, gpointer pfx)
{
    gchar *str = gst_value_serialize(value);

    g_print("%s  %15s: %s\n", (gchar *)pfx, g_quark_to_string(field), str);
    g_free(str);
    return TRUE;
}

static void print_caps(const GstCaps *caps, const gchar *pfx)
{
    guint i;

    g_return_if_fail(caps != NULL);

    if (gst_caps_is_any(caps))
    {
        g_print("%sANY\n", pfx);
        return;
    }
    if (gst_caps_is_empty(caps))
    {
        g_print("%sEMPTY\n", pfx);
        return;
    }

    for (i = 0; i < gst_caps_get_size(caps); i++)
    {
        GstStructure *structure = gst_caps_get_structure(caps, i);

        g_print("%s%s\n", pfx, gst_structure_get_name(structure));
        gst_structure_foreach(structure, print_field, (gpointer)pfx);
    }
}

/* Prints information about a Pad Template, including its Capabilities */
static void print_pad_templates_information(GstElementFactory *factory)
{
    const GList *pads;
    GstStaticPadTemplate *padtemplate;

    g_print("Pad Templates for %s:\n",
            gst_element_factory_get_longname(factory));
    if (!gst_element_factory_get_num_pad_templates(factory))
    {
        g_print("  none\n");
        return;
    }

    pads = gst_element_factory_get_static_pad_templates(factory);
    while (pads)
    {
        padtemplate = (GstStaticPadTemplate *)pads->data;
        pads = g_list_next(pads);

        if (padtemplate->direction == GST_PAD_SRC)
            g_print("  SRC template: '%s'\n", padtemplate->name_template);
        else if (padtemplate->direction == GST_PAD_SINK)
            g_print("  SINK template: '%s'\n", padtemplate->name_template);
        else
            g_print("  UNKNOWN!!! template: '%s'\n", padtemplate->name_template);

        if (padtemplate->presence == GST_PAD_ALWAYS)
            g_print("    Availability: Always\n");
        else if (padtemplate->presence == GST_PAD_SOMETIMES)
            g_print("    Availability: Sometimes\n");
        else if (padtemplate->presence == GST_PAD_REQUEST)
            g_print("    Availability: On request\n");
        else
            g_print("    Availability: UNKNOWN!!!\n");

        if (padtemplate->static_caps.string)
        {
            GstCaps *caps;

            g_print("    Capabilities:\n");
            caps = gst_static_caps_get(&padtemplate->static_caps);
            print_caps(caps, "      ");
            gst_caps_unref(caps);
        }

        g_print("\n");
    }
}

/* Shows the CURRENT capabilities of the requested pad in the given element */
static void print_pad_capabilities(GstElement *element, gchar *pad_name)
{
    GstPad *pad = NULL;
    GstCaps *caps = NULL;

    /* Retrieve pad */
    pad = gst_element_get_static_pad(element, pad_name);
    if (!pad)
    {
        g_printerr("Could not retrieve pad '%s'\n", pad_name);
        return;
    }

    /* Retrieve negotiated caps (or acceptable caps if negotiation is not finished yet) */
    caps = gst_pad_get_current_caps(pad);
    if (!caps)
        caps = gst_pad_query_caps(pad, NULL);

    /* Print and free */
    g_print("Caps for the %s pad:\n", pad_name);
    print_caps(caps, "      ");
    gst_caps_unref(caps);
    gst_object_unref(pad);
}

static gboolean handleKeyboard(GIOChannel *source, GIOCondition cond, CustomData *data)
{
    gchar *str = NULL;
    if (g_io_channel_read_line(source, &str, NULL, NULL, NULL) != G_IO_STATUS_NORMAL)
    {
        return TRUE;
    }

    switch (g_ascii_tolower(str[0]))
    {
    case 'p':
    {
        if (gst_element_set_state(data->pipeline, data->playing ? GST_STATE_PAUSED : GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
        {
            g_print("\nFailed to pause/play the pipeline");
            return FALSE;
        }
        data->playing = !data->playing;
        g_print("\nSettings pipeline to %s", data->playing ? "playing" : "paused.");
        break;
    }
    case 's':
    {
        if (!gst_element_send_event(data->pipeline, gst_event_new_eos()))
        {
            g_print("\nFailed to stop the pipeline");
            return FALSE;
        }
        g_print("\nStopping the pipeline.");
        break;
    }
    default:
        break;
    }

    g_free(str);
    return TRUE;
}

// Bus Message handler
static gboolean busCallBack(GstBus *bus, GstMessage *message, gpointer data)
{
    CustomData *dataPtr = (CustomData *)data;
    switch (GST_MESSAGE_TYPE(message))
    {
    case GST_MESSAGE_ERROR:
    {
        GError *err;
        gchar *debugInfo;
        gst_message_parse_error(message, &err, &debugInfo);
        gst_printerr("\nError: %s", err->message);
        g_free(debugInfo);
        g_clear_error(&err);
        g_main_loop_quit(dataPtr->mainLoop);
        break;
    }
    case GST_MESSAGE_EOS:
    {
        gst_printerr("\nReached End of the stream.");
        g_main_loop_quit(dataPtr->mainLoop);
        break;
    }
    case GST_MESSAGE_STATE_CHANGED:
    {
        // if ((GstObject *)dataPtr->pipeline == message->src)
        if (GST_MESSAGE_SRC(message) == GST_OBJECT(dataPtr->pipeline))
        {
            GstState oldState, newState, pendingState;
            gst_message_parse_state_changed(message, &oldState, &newState, &pendingState);
            g_print("\nState Changed from %s to %s",
                    gst_element_state_get_name(oldState),
                    gst_element_state_get_name(newState));
        }
        break;
    }

    default:
    {
        gst_printerr("\nUnknown message received.");
        break;
    }
    }

    return TRUE; // https://github1s.com/GStreamer/gst-docs/blob/master/examples/bus_example.c#L36-L37
}

int main()
{
    CustomData data;
    GIOChannel *ioStdin;

    gst_init(NULL, NULL);

    // Create FACTORY ELEMENT not the actual element
    data.sourceFactory = gst_element_factory_find("v4l2src");
    data.capsFilterFactory = gst_element_factory_find("capsfilter");
    data.converterFactory = gst_element_factory_find("videoconvert");
    data.encoderFactory = gst_element_factory_find("x264enc");
    data.parserFactory = gst_element_factory_find("h264parse");
    data.muxFactory = gst_element_factory_find("mp4mux");
    data.sinkFactory = gst_element_factory_find("filesink");

    if (
        !data.sourceFactory ||
        !data.capsFilterFactory ||
        !data.converterFactory ||
        !data.encoderFactory ||
        !data.parserFactory ||
        !data.muxFactory ||
        !data.sinkFactory)
    {
        gst_printerr("\nFailed to find a factory element.");
        return -1;
    }

    // Instantiate the ELEMENT and create a empty pipeline
    data.source = gst_element_factory_create(data.sourceFactory, NULL);
    data.capsFilter = gst_element_factory_create(data.capsFilterFactory, NULL);
    data.converter = gst_element_factory_create(data.converterFactory, NULL);
    data.encoder = gst_element_factory_create(data.encoderFactory, NULL);
    data.parser = gst_element_factory_create(data.parserFactory, NULL);
    data.mux = gst_element_factory_create(data.muxFactory, NULL);
    data.sink = gst_element_factory_create(data.sinkFactory, NULL);
    data.pipeline = gst_pipeline_new("test_pipeline");
    if (
        !data.source ||
        !data.capsFilter ||
        !data.converter ||
        !data.encoder ||
        !data.parser ||
        !data.mux ||
        !data.sink ||
        !data.pipeline)
    {
        gst_printerr("\nFailed to instantiate Gst Element or Pipeline.");
        return -1;
    }

    // print_pad_templates_information(data.sourceFactory);
    // print_pad_templates_information(data.capsFilterFactory);
    // print_pad_templates_information(data.converterFactory);
    // print_pad_templates_information(data.encoderFactory);
    // print_pad_templates_information(data.parserFactory);
    // print_pad_templates_information(data.muxFactory);
    // print_pad_templates_information(data.sinkFactory);

    GstCaps *caps = gst_caps_from_string("video/x-raw,format=YUY2,width=320,height=240,framerate=30/1");

    // Setting Element's properties after succesfull creation.
    g_object_set(data.source, "device", "/dev/video0", NULL);
    g_object_set(data.source, "io-mode", 0, NULL);
    g_object_set(data.capsFilter, "caps", caps, NULL);
    // g_object_set(data.encoder, "bitrate", 8000, NULL);
    g_object_set(data.sink, "location", "./test.mp4", NULL);

    // Build the pipeline by adding them in a group(GstBin) and linking them.
    gst_bin_add_many(GST_BIN(data.pipeline), data.source, data.capsFilter, data.converter, data.encoder, data.parser, data.mux, data.sink, NULL);
    if (gst_element_link_many(data.source, data.capsFilter, data.converter, data.encoder, data.parser, data.mux, data.sink, NULL) == FALSE)
    {
        gst_printerr("\nFailed to link the pipeline.");
        gst_object_unref(data.pipeline);
        return -1;
    }

    // Setting up a keyboard watch so we get notified of keystrokes
    ioStdin = g_io_channel_unix_new(fileno(stdin));
    g_io_add_watch(ioStdin, G_IO_IN, (GIOFunc)handleKeyboard, &data);

    // Setting up a GST Bus handler function
    GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(data.pipeline));
    gst_bus_add_watch(bus, (GstBusFunc)busCallBack, &data);

    // Start playing
    if (gst_element_set_state(data.pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
    {
        g_print("\n011");
        gst_printerr("\nFailed to start the pipeline.");
        gst_object_unref(data.pipeline);
        return -1;
    }
    data.playing = TRUE;

    // Create a GLib Main Loop and set it to run. This is for Bus Message handler.
    data.mainLoop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(data.mainLoop);

    g_main_loop_unref(data.mainLoop);
    g_io_channel_unref(ioStdin);
    gst_object_unref(bus);
    gst_element_set_state(data.pipeline, GST_STATE_NULL);
    gst_object_unref(data.pipeline);

    return 0;
}