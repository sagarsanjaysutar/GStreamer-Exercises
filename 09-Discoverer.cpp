#include <string.h>
#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

typedef struct
{
    GstDiscoverer *disc;
    GMainLoop *loop;
} CustomData;

static void onDiscovered(GstDiscoverer *disc, GstDiscovererInfo *discInfo, GError *err, CustomData *data)
{
    const gchar *url = gst_discoverer_info_get_uri(discInfo);
    GstDiscovererResult result = gst_discoverer_info_get_result(discInfo);
    switch (result)
    {
    case GST_DISCOVERER_URI_INVALID:
        g_print("\nInvalid URI '%s'", url);
        break;
    case GST_DISCOVERER_ERROR:
        g_print("\nDiscoverer error: %s", err->message);
        break;
    case GST_DISCOVERER_TIMEOUT:
        g_print("\nTimeout");
        break;
    case GST_DISCOVERER_BUSY:
        g_print("\nBusy");
        break;
    case GST_DISCOVERER_MISSING_PLUGINS:
        g_print("\nMissing plugins");
        break;
    case GST_DISCOVERER_OK:
        g_print("\nDiscovered '%s'", url);
        break;
    }

    if (result != GST_DISCOVERER_OK)
    {
        g_printerr("\nThis URI cannot be played");
        return;
    }

    gst_printerr("\nDuration: %" GST_TIME_FORMAT, GST_TIME_ARGS(gst_discoverer_info_get_duration(discInfo)));
}

static void onFinished(GstDiscoverer *disc, CustomData *data)
{
    gst_printerr("\nFinished discovering.\n");
    g_main_loop_quit(data->loop);
}

int main(int argc, char **argv)
{
    CustomData data;
    char const *url = "https://www.freedesktop.org/software/gstreamer-sdk/data/media/sintel_trailer-480p.webm";

    if (argc > 1)
    {
        url = argv[1];
    }

    gst_init(NULL, NULL);

    // Instantiate
    GError *err = NULL;
    data.disc = gst_discoverer_new(5 * GST_SECOND, &err);
    if (!data.disc)
    {
        gst_printerr("\nFailed to instantiate discoverer object.");
        g_clear_error(&err);
        return -1;
    }

    // Connect to desired signal
    g_signal_connect(data.disc, "discovered", G_CALLBACK(onDiscovered), &data);
    g_signal_connect(data.disc, "finished", G_CALLBACK(onFinished), &data);

    gst_discoverer_start(data.disc);

    if (!gst_discoverer_discover_uri_async(data.disc, url))
    {
        gst_printerr("\nFailed to read file: %s", url);
        g_object_unref(data.disc);
        return -1;
    }

    data.loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(data.loop);

    g_object_unref(data.disc);
    g_main_loop_unref(data.loop);

    return 0;
}