/*!
 * @brief
 * @link https://gstreamer.freedesktop.org/documentation/tutorials/basic/time-management.html?gi-language=c
 *
 */

#include <gst/gst.h>

typedef struct
{
    GstElement *playbin;
    gboolean isPlaying, isTerminated, isSeekEnabled, isSeekDone;
    gint64 duration;

} CustomData;

/*!
 * @breif  Processes all messages received through the pipeline's bus
 */
static void handleMessage(CustomData *data, GstMessage *msg)
{
    GError *err;
    gchar *debug_info;

    switch (GST_MESSAGE_TYPE(msg))
    {
    case GST_MESSAGE_ERROR:
    {
        gst_message_parse_error(msg, &err, &debug_info);
        gst_printerr("\nError Message: %s:\n %s", GST_OBJECT_NAME(msg->src), err->message);
        gst_printerr("\nDebug Message: \n %s", debug_info ? debug_info : "none");
        g_clear_error(&err);
        g_free(debug_info);
        data->isTerminated = TRUE;
        break;
    }
    case GST_MESSAGE_EOS:
    {
        g_print("\nReached End of Stream.");
        data->isTerminated = TRUE;
        break;
    }
    case GST_MESSAGE_DURATION:
    {
        // The duration has changed, mark the current one as invalid so it gets re-queried later.
        data->duration = GST_CLOCK_TIME_NONE;
        break;
    }
    case GST_MESSAGE_STATE_CHANGED:
    {
        GstState oldState, newState, pendingState;
        gst_message_parse_state_changed(msg, &oldState, &newState, &pendingState);
        // We are only intererted in state-changed messages from pipeline.
        if (GST_MESSAGE_SRC(msg) == GST_OBJECT(data->playbin))
        {
            g_print("\nState changed from %s to %s",
                    gst_element_state_get_name(oldState),
                    gst_element_state_get_name(newState));

            data->isPlaying = (newState == GST_STATE_PLAYING);

            // Seeks and time queries generally only get a valid reply when in the PAUSED or PLAYING state.
            if (data->isPlaying)
            {
                GstQuery *query;
                gint64 start, end;

                // Creating a query object which queries "seeking properties" from stream.
                // GST_FORMAT_TIME means we are interested in seeking by specifying the new time to which we want to move.
                query = gst_query_new_seeking(GST_FORMAT_TIME);

                // Perform the query on pipeline
                if (gst_element_query(data->playbin, query))
                {
                    gst_query_parse_seeking(query, NULL, &data->isSeekEnabled, &start, &end);
                    if (data->isSeekEnabled)
                    {
                        g_print(
                            "\nSeeking is enabled from %" GST_TIME_FORMAT " to %" GST_TIME_FORMAT,
                            GST_TIME_ARGS(start),
                            GST_TIME_ARGS(end));
                    }
                    else
                    {
                        g_print("\nSeeking is disabled.");
                    }
                }
                else
                {
                    g_print("\nSeeking query failed.");
                }
                gst_query_unref(query);
            }
        }
        break;
    }
    default:
    {
        // We should not reach here
        g_print("\nUnexpected Message recieved.");
        break;
    }
    }
    gst_message_unref(msg);
}

int main()
{
    CustomData data;
    GstBus *bus;
    GstMessage *msg;
    char const *url = "https://www.freedesktop.org/software/gstreamer-sdk/data/media/sintel_trailer-480p.webm";

    data.isPlaying = FALSE;
    data.isTerminated = FALSE;
    data.isSeekEnabled = FALSE;
    data.isSeekDone = FALSE;
    data.duration = GST_CLOCK_TIME_NONE;

    gst_init(NULL, NULL);

    // Create element
    data.playbin = gst_element_factory_make("playbin", "playbin");

    // Set the URL for Playbin
    g_object_set(data.playbin, "uri", url, NULL);

    // Start playing
    if (gst_element_set_state(data.playbin, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
    {
        g_print("\nFailed to start the plabin.");
        gst_object_unref(data.playbin);
        return -1;
    }

    // Listen to the bus
    bus = gst_element_get_bus(data.playbin);
    do
    {
        msg = gst_bus_timed_pop_filtered(
            bus,
            100 * GST_MSECOND,
            (GstMessageType)(GST_MESSAGE_STATE_CHANGED | GST_MESSAGE_ERROR | GST_MESSAGE_EOS | GST_MESSAGE_DURATION));

        if (msg != NULL)
        {
            handleMessage(&data, msg);
        }
        else
        {
            // If no message is recieved it means timeout expired.
            if (data.isPlaying)
            {
                gint64 currentPos = -1;

                // Query current position
                if (!gst_element_query_position(data.playbin, GST_FORMAT_TIME, &currentPos))
                {
                    g_print("\nCouldn't retrive current position");
                }

                // Query stream duration
                if (!GST_CLOCK_TIME_IS_VALID(data.duration))
                {
                    if (!gst_element_query_duration(data.playbin, GST_FORMAT_TIME, &data.duration))
                    {
                        g_print("\nCouldn't retrive stream duration.");
                    }
                }

                // If everything is fine, print current position and total stream duration.
                g_print("\nPosition %" GST_TIME_FORMAT " / %" GST_TIME_FORMAT "\r",
                        GST_TIME_ARGS(currentPos), GST_TIME_ARGS(data.duration));
                if (data.isSeekEnabled && !data.isSeekDone && currentPos > (10 * GST_SECOND))
                {
                    g_print("\nReached 10s. Performing seek...");
                    gst_element_seek_simple(
                        data.playbin,
                        GST_FORMAT_TIME,
                        (GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
                        15 * GST_SECOND);

                    data.isSeekDone = TRUE;
                }
            }
        }

    } while (!data.isTerminated);

    // Deallocate
    gst_object_unref(bus);
    gst_element_set_state(data.playbin, GST_STATE_NULL);
    gst_object_unref(data.playbin);

    return 0;
}