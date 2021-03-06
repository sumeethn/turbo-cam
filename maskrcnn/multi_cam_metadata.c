/*
 * Copyright (c) 2019 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 *
 */

#include <gst/gst.h>
#include <glib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <sys/time.h>

#include "gstnvdsmeta.h"
//#include "gstnvstreammeta.h"
#ifndef PLATFORM_TEGRA
	#include "gst-nvmessage.h"
#endif

/* The muxer output resolution must be set if the input streams will be of
 * different resolution. The muxer will scale all the input frames to this
 * resolution. */
#define MUXER_OUTPUT_WIDTH 1920
#define MUXER_OUTPUT_HEIGHT 1080

/* Muxer batch formation timeout, for e.g. 40 millisec. Should ideally be set
 * based on the fastest source's framerate. */
#define MUXER_BATCH_TIMEOUT_USEC 4000000

#define TILED_OUTPUT_WIDTH 1920
#define TILED_OUTPUT_HEIGHT 1080

/* NVIDIA Decoder source pad memory feature. This feature signifies that source
 * pads having this capability will push GstBuffers containing cuda buffers. */
#define GST_CAPS_FEATURES_NVMM "memory:NVMM"

clock_t t_start; 
clock_t t_end;
gint frame_number = 0;

/*
static gboolean
bus_call (GstBus * bus, GstMessage * msg, gpointer data) {
    GMainLoop *loop = (GMainLoop *) data;
    switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_EOS:
        g_print ("End of stream\n");
	    t_end = clock(); 
      	clock_t t = t_end - t_start;
      	double time_taken = ((double)t)/CLOCKS_PER_SEC; // in seconds 
      	double fps = frame_number/time_taken;
      	g_print("\nThe program took %.2f seconds to create masks for %d frames, pref = %.2f fps \n\n", time_taken,frame_number,fps); 
      
        g_main_loop_quit (loop);
        break;
    case GST_MESSAGE_ERROR: {
        gchar *debug;
        GError *error;
        gst_message_parse_error (msg, &error, &debug);
        g_printerr ("ERROR from element %s: %s\n",
                    GST_OBJECT_NAME (msg->src), error->message);
        if (debug)
            g_printerr ("Error details: %s\n", debug);
        g_free (debug);
        g_error_free (error);
        g_main_loop_quit (loop);
        break;
    }
    default:
        break;
    }
    return TRUE;
}
*/

static gboolean
bus_call (GstBus * bus, GstMessage * msg, gpointer data)
{
  GMainLoop *loop = (GMainLoop *) data;
  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_EOS:
      g_print ("End of stream\n");
      t_end = clock(); 
      clock_t t = t_end - t_start;
      double time_taken = ((double)t)/CLOCKS_PER_SEC; // in seconds 
      double fps = frame_number/time_taken;
      g_print("\nThe program took %.2f seconds to create masks for %d frames, pref = %.2f fps \n\n", time_taken,frame_number,fps); 
      	
      g_main_loop_quit (loop);
      break;
    case GST_MESSAGE_WARNING:
    {
      gchar *debug;
      GError *error;
      gst_message_parse_warning (msg, &error, &debug);
      g_printerr ("WARNING from element %s: %s\n",
          GST_OBJECT_NAME (msg->src), error->message);
      g_free (debug);
      g_printerr ("Warning: %s\n", error->message);
      g_error_free (error);
      break;
    }
    case GST_MESSAGE_ERROR:
    {
      gchar *debug;
      GError *error;
      gst_message_parse_error (msg, &error, &debug);
      g_printerr ("ERROR from element %s: %s\n",
          GST_OBJECT_NAME (msg->src), error->message);
      if (debug)
        g_printerr ("Error details: %s\n", debug);
      g_free (debug);
      g_error_free (error);
      g_main_loop_quit (loop);
      break;
    }
#ifndef PLATFORM_TEGRA
    case GST_MESSAGE_ELEMENT:
    {
      if (gst_nvmessage_is_stream_eos (msg)) {
        guint stream_id;
        if (gst_nvmessage_parse_stream_eos (msg, &stream_id)) {
          g_print ("Got EOS from stream %d\n", stream_id);
        }
      }
      break;
    }
#endif
    default:
      break;
  }
  return TRUE;
}


static GstPadProbeReturn
tiler_src_pad_buffer_probe (GstPad * pad, GstPadProbeInfo * info,
    gpointer u_data)
{
    GstBuffer *buf = (GstBuffer *) info->data;
    guint num_rects = 0; 
    NvDsObjectMeta *obj_meta = NULL;
    guint vehicle_count = 0;
    guint person_count = 0;
    NvDsMetaList * l_frame = NULL;
    NvDsMetaList * l_obj = NULL;
    gchar *msg = NULL; 
    
    //NvDsDisplayMeta *display_meta = NULL;
    
    /* FPS output */
    g_object_get(G_OBJECT (u_data), "last-message", &msg, NULL);
    if (msg != NULL) {
    	g_print("FPS info: %s \n", msg);
    }

    NvDsBatchMeta *batch_meta = gst_buffer_get_nvds_batch_meta (buf);

    for (l_frame = batch_meta->frame_meta_list; l_frame != NULL;
      l_frame = l_frame->next) {
        NvDsFrameMeta *frame_meta = (NvDsFrameMeta *) (l_frame->data);
        //int offset = 0;
        for (l_obj = frame_meta->obj_meta_list; l_obj != NULL;
                l_obj = l_obj->next) {
            obj_meta = (NvDsObjectMeta *) (l_obj->data);
            if (obj_meta->class_id > 1) {
                vehicle_count++;
                num_rects++;
            }
            if (obj_meta->class_id == 1) {
                person_count++;
                num_rects++;
            }
        }
          g_print ("Frame Number = %d Number of objects = %d "
            "Vehicle Count = %d Person Count = %d\n",
            frame_meta->frame_num, num_rects, vehicle_count, person_count);

    }
    return GST_PAD_PROBE_OK;
}


/* Buffer probe function that registers on the sink pad of the OSD element. All the infer elements in the pipeline shall attach their metadata to the GstBuffer. */
static GstPadProbeReturn
osd_sink_pad_buffer_probe (GstPad * pad, GstPadProbeInfo * info,
    gpointer u_data)
{
	GstBuffer *buf = (GstBuffer *) info->data;
	
	frame_number++;
  return GST_PAD_PROBE_OK;
}

static void
cb_newpad (GstElement * decodebin, GstPad * decoder_src_pad, gpointer data)
{
  g_print ("In cb_newpad\n");
  GstCaps *caps = gst_pad_get_current_caps (decoder_src_pad);
  const GstStructure *str = gst_caps_get_structure (caps, 0);
  const gchar *name = gst_structure_get_name (str);
  GstElement *source_bin = (GstElement *) data;
  GstCapsFeatures *features = gst_caps_get_features (caps, 0);

  /* Need to check if the pad created by the decodebin is for video and not
   * audio. */
  if (!strncmp (name, "video", 5)) {
    /* Link the decodebin pad only if decodebin has picked nvidia
     * decoder plugin nvdec_*. We do this by checking if the pad caps contain
     * NVMM memory features. */
    if (gst_caps_features_contains (features, GST_CAPS_FEATURES_NVMM)) {
      /* Get the source bin ghost pad */
      GstPad *bin_ghost_pad = gst_element_get_static_pad (source_bin, "src");
      if (!gst_ghost_pad_set_target (GST_GHOST_PAD (bin_ghost_pad),
              decoder_src_pad)) {
        g_printerr ("Failed to link decoder src pad to source bin ghost pad\n");
      }
      gst_object_unref (bin_ghost_pad);
    } else {
      g_printerr ("Error: Decodebin did not pick nvidia decoder plugin.\n");
    }
  }
}

static void
decodebin_child_added (GstChildProxy * child_proxy, GObject * object,
    gchar * name, gpointer user_data)
{
  g_print ("Decodebin child added: %s\n", name);
  if (g_strrstr (name, "decodebin") == name) {
    g_signal_connect (G_OBJECT (object), "child-added",
        G_CALLBACK (decodebin_child_added), user_data);
  }
  if (g_strstr_len (name, -1, "nvv4l2decoder") == name) {
    g_print ("Seting bufapi_version\n");
    g_object_set (object, "bufapi-version", TRUE, NULL);
  }
}

static GstElement *
create_source_bin (guint index, gchar * uri)
{
  GstElement *bin = NULL, *uri_decode_bin = NULL;
  gchar bin_name[16] = { };

  g_snprintf (bin_name, 15, "source-bin-%02d", index);
  /* Create a source GstBin to abstract this bin's content from the rest of the
   * pipeline */
  bin = gst_bin_new (bin_name);

  /* Source element for reading from the uri.
   * We will use decodebin and let it figure out the container format of the
   * stream and the codec and plug the appropriate demux and decode plugins. */
  uri_decode_bin = gst_element_factory_make ("uridecodebin", "uri-decode-bin");

  if (!bin || !uri_decode_bin) {
    g_printerr ("One element in source bin could not be created.\n");
    return NULL;
  }

  /* We set the input uri to the source element */
  g_object_set (G_OBJECT (uri_decode_bin), "uri", uri, NULL);

  /* Connect to the "pad-added" signal of the decodebin which generates a
   * callback once a new pad for raw data has beed created by the decodebin */
  g_signal_connect (G_OBJECT (uri_decode_bin), "pad-added",
      G_CALLBACK (cb_newpad), bin);
  g_signal_connect (G_OBJECT (uri_decode_bin), "child-added",
      G_CALLBACK (decodebin_child_added), bin);

  gst_bin_add (GST_BIN (bin), uri_decode_bin);

  /* We need to create a ghost pad for the source bin which will act as a proxy
   * for the video decoder src pad. The ghost pad will not have a target right
   * now. Once the decode bin creates the video decoder and generates the
   * cb_newpad callback, we will set the ghost pad target to the video decoder
   * src pad. */
  if (!gst_element_add_pad (bin, gst_ghost_pad_new_no_target ("src",
              GST_PAD_SRC))) {
    g_printerr ("Failed to add ghost pad in source bin\n");
    return NULL;
  }

  return bin;
}

int
main (int argc, char *argv[]) {
    GMainLoop *loop = NULL;
    GstElement *pipeline = NULL, *source = NULL, *h264parser = NULL,
               *decoder = NULL, *streammux = NULL, *sink = NULL, *nvsink = NULL, 
               *pgie = NULL, *nvvidconv = NULL, *nvosd = NULL, *tiler = NULL;
#ifdef PLATFORM_TEGRA
    GstElement *transform = NULL;
#endif
    GstBus *bus = NULL;
    guint bus_watch_id;
    GstPad *osd_sink_pad = NULL;
    gulong osd_probe_id = 0;
    guint i, num_sources;
    GstPad *tiler_src_pad = NULL;
 	guint tiler_rows, tiler_columns;
  	guint pgie_batch_size;

    /* Check input arguments */
    if (argc < 3) {
        g_printerr ("Usage: %s config_file <uri1> [uri2] ... [uriN] \n", argv[0]);
        return -1;
    }
    
    num_sources = argc - 2;

    /* Standard GStreamer initialization */
    gst_init (&argc, &argv);
    loop = g_main_loop_new (NULL, FALSE);

    /* Create gstreamer elements */
    /* Create Pipeline element that will form a connection of other elements */
    pipeline = gst_pipeline_new ("turbo-mrcnn-pipeline");
    
    /* Create nvstreammux instance to form batches from one or more sources. */
    streammux = gst_element_factory_make ("nvstreammux", "stream-muxer");
    
    if (!pipeline || !streammux) {
        g_printerr ("One element could not be created. Exiting.\n");
        return -1;
    }
    
    gst_bin_add(GST_BIN (pipeline), streammux);
    
    /* Create source elements for reading from files */
    for (i=0; i<num_sources; i++) {
    	GstPad *sinkpad, *srcpad;
    	gchar pad_name[16] = { };
    	GstElement *source_bin = create_source_bin(i, argv[i+2]);
    	
    	if(!source_bin) {
    		g_printerr("Failed to create source bin. Exiting. \n");
    		return -1;
    	}
    	
    	gst_bin_add(GST_BIN(pipeline), source_bin);    	
    	
    	g_snprintf(pad_name, 15, "sink_%u", i);
    	sinkpad = gst_element_get_request_pad(streammux, pad_name);
    	if (!sinkpad) {
    		g_printerr("Streammux request sink pad failed. Exiting. \n");
    		return -1;
    	}
    	
    	srcpad = gst_element_get_static_pad(source_bin, "src");
    	if (!srcpad) {
    		g_printerr("Failed to get src pad of src bin. Exiting. \n");
    		return -1;
    	}
    	
    	if (gst_pad_link (srcpad, sinkpad) != GST_PAD_LINK_OK) {
    		g_printerr("Failed to link src bin to streammux. Exiting. \n");
    		return -1;
    	}
    	
    	gst_object_unref(srcpad);
    	gst_object_unref(sinkpad);
    }
    
	/* Use nvinfer to infer on batched frame. */
	pgie = gst_element_factory_make ("nvinfer", "primary-nvinference-engine");
	
	/* Use nvtiler to composite the batched frames into a 2D tiled array based
   	* on the source of the frames. */
  	tiler = gst_element_factory_make ("nvmultistreamtiler", "nvtiler");

  	/* Use convertor to convert from NV12 to RGBA as required by nvosd */
  	nvvidconv = gst_element_factory_make ("nvvideoconvert", "nvvideo-converter");

  	/* Create OSD to draw on the converted RGBA buffer */
  	nvosd = gst_element_factory_make ("nvdsosd", "nv-onscreendisplay");

	/* Finally render the osd output */
#ifdef PLATFORM_TEGRA
	transform = gst_element_factory_make ("nvegltransform", "nvegl-transform");
#endif
	
	//sink = gst_element_factory_make ("nveglglessink", "nvvideo-renderer");
	nvsink = gst_element_factory_make ("fakesink", "nvvideo-renderer");
	sink = gst_element_factory_make ("fpsdisplaysink", "fps-display");
	g_object_set (G_OBJECT (sink), "text-overlay", FALSE, "video-sink", nvsink, "sync", FALSE, NULL);
	
	g_object_set (sink, "sync", FALSE, "max-lateness", -1,
      "async", FALSE, "qos", TRUE, NULL);

	if (!pgie || !tiler || !nvvidconv || !nvosd || !sink) {
		g_printerr ("One element could not be created. Exiting.\n");
		return -1;
	}

#ifdef PLATFORM_TEGRA
  	if(!transform) {
    	g_printerr ("One tegra element could not be created. Exiting.\n");
    	return -1;
  	}
#endif

  	g_object_set (G_OBJECT (streammux), "width", MUXER_OUTPUT_WIDTH, "height",
  	    MUXER_OUTPUT_HEIGHT, "batch-size", num_sources,
  	    "batched-push-timeout", MUXER_BATCH_TIMEOUT_USEC, NULL);
      
      
    /* Configure the nvinfer element using the nvinfer config file. */
  	g_object_set (G_OBJECT (pgie),
    	  "config-file-path", argv[1], NULL);

  	/* Override the batch-size set in the config file with the number of sources. */
  	g_object_get (G_OBJECT (pgie), "batch-size", &pgie_batch_size, NULL);
  	if (pgie_batch_size != num_sources) {
    	g_printerr
    	    ("WARNING: Overriding infer-config batch-size (%d) with number of sources (%d)\n",
        pgie_batch_size, num_sources);
    	g_object_set (G_OBJECT (pgie), "batch-size", num_sources, NULL);
  	}

  	tiler_rows = (guint) sqrt (num_sources);
  	tiler_columns = (guint) ceil (1.0 * num_sources / tiler_rows);
  	/* we set the tiler properties here */
  	g_object_set (G_OBJECT (tiler), "rows", tiler_rows, "columns", tiler_columns,
    	  "width", TILED_OUTPUT_WIDTH, "height", TILED_OUTPUT_HEIGHT, NULL);

  	/* we add a message handler */
  	bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  	bus_watch_id = gst_bus_add_watch (bus, bus_call, loop);
  	gst_object_unref (bus);

  	/* Set up the pipeline */
  	/* we add all elements into the pipeline */
#ifdef PLATFORM_TEGRA
 	gst_bin_add_many (GST_BIN (pipeline), pgie, tiler, nvvidconv, nvosd, sink, NULL);
  	/* we link the elements together
   	* nvstreammux -> nvinfer -> nvtiler -> nvvidconv -> nvosd -> video-renderer */
  	if (!gst_element_link_many (streammux, pgie, tiler, nvvidconv, nvosd, sink, NULL)) {
    	g_printerr ("Elements could not be linked. Exiting.\n");
    	return -1;
  	}
#else
	gst_bin_add_many (GST_BIN (pipeline), pgie, tiler, nvvidconv, nvosd, sink, NULL);
  	/* we link the elements together
   	* nvstreammux -> nvinfer -> nvtiler -> nvvidconv -> nvosd -> video-renderer */
  	if (!gst_element_link_many (streammux, pgie, tiler, nvvidconv, nvosd, sink,NULL)) {
    	g_printerr ("Elements could not be linked. Exiting.\n");
    	return -1;
  	}
#endif

  	/* Lets add probe to get informed of the meta data generated, we add probe to
  	 * the sink pad of the osd element, since by that time, the buffer would have
  	 * had got all the metadata. */
 	 tiler_src_pad = gst_element_get_static_pad (pgie, "src");
 	 if (!tiler_src_pad)
 	   g_print ("Unable to get src pad\n");
  	else
  	  gst_pad_add_probe (tiler_src_pad, GST_PAD_PROBE_TYPE_BUFFER,
      	  tiler_src_pad_buffer_probe, (gpointer)sink, NULL);

 	 /* Set the pipeline to "playing" state */
  	g_print ("Now playing:");
  	for (i = 0; i < num_sources; i++) {
  	  g_print (" %s,", argv[i + 2]);
  	}
  	g_print ("\n");
  	
  	t_start = clock();
  	gst_element_set_state (pipeline, GST_STATE_PLAYING);

 	 /* Wait till pipeline encounters an error or EOS */
 	 g_print ("Running...\n");
 	 g_main_loop_run (loop);

  	/* Out of the main loop, clean up nicely */
  	g_print ("Returned, stopping playback\n");
  	gst_element_set_state (pipeline, GST_STATE_NULL);
 	g_print ("Deleting pipeline\n");
  	gst_object_unref (GST_OBJECT (pipeline));
 	g_source_remove (bus_watch_id);
  	g_main_loop_unref (loop);
  	return 0;
}
