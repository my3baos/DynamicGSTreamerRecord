/* 
 *   hcl - test program to detect/mesure network streaming (PLC/SHP) performance/quality
 */

#include <gst/gst.h>
#include <string.h>
#include <stdio.h>
#include <glib.h>

#define N_FRAMES_TO_NSECS(x)   (33333334*x) /* about 'x' 30fps-rate frames */
#define N_FRAMES               (30)         /* wait for one second */
typedef struct _CustomData
{
  gboolean is_live;
  GstElement *pipeline;
  GMainLoop *loop;
  guint64 frame_count;
  guint64 drop_count;
  gint32 cam_index;
  GstElement *qmx;
} CustomData;

typedef struct
{
  GstPad *teepad;   // source pad on tee
  //GstPad *teesink;  //sink pad on tee
  GstPad *qsink;
  GstPad *qsource;
  GstElement *queue;
  GstElement *encode;
  GstElement *mux;
  GstElement *sink;
  gboolean removing;
} Sink;

CustomData data;
Sink *recSink;
static void cb_handoff (GstElement* object,
                        GstBuffer* gstBuf,
                        gpointer data) {
   //gint32 byterate; 
   gsize offset;
   gsize maxsize;
   CustomData * priv = (CustomData*)data;
   priv->frame_count++; 
   //g_object_get (G_OBJECT (object), "datarate", &byterate, NULL);
   /* ~ print 'good' status every 3 seconds */
   if (0 == (priv->frame_count % 150))
      g_print ("<%d> total/dropped = %lu/%lu\n", priv->cam_index, 
		                                 priv->frame_count,
					         priv->drop_count);
}


static void cb_message (GstBus * bus, GstMessage * msg, CustomData * data) {
  CustomData * priv = (CustomData*)data;
  guint64 processed;
  guint64 dropped;
  GstFormat format;

  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_ERROR:{
      GError *err;
      gchar *debug;

      gst_message_parse_error (msg, &err, &debug);
      g_print ("Error: %s\n", err->message);
      g_error_free (err);
      g_free (debug);

      gst_element_set_state (data->pipeline, GST_STATE_READY);
      g_main_loop_quit (data->loop);
      break;
    }
    case GST_MESSAGE_EOS:
      /* end-of-stream */
      g_print (">>>>>> GST_MESSAGE_EOS\n");
      gst_element_set_state (data->pipeline, GST_STATE_READY);
      g_print (">>>>>> GST_MESSAGE_EOS\n");
      //g_main_loop_quit (data->loop);
      break;
    case GST_MESSAGE_BUFFERING:{
      gint percent = 0;

      /* If the stream is live, we do not care about buffering. */
      if (data->is_live)
        break;

      gst_message_parse_buffering (msg, &percent);
      g_print ("Buffering (%3d%%)\r", percent);
      /* Wait until buffering is complete before start/resume playing */
      if (percent < 100)
        gst_element_set_state (data->pipeline, GST_STATE_PAUSED);
      else
        gst_element_set_state (data->pipeline, GST_STATE_PLAYING);
      break;
    }
    case GST_MESSAGE_CLOCK_LOST:
      /* Get a new clock */
      g_print (">>>>>> Message: name: %s, source name: %s\n", GST_MESSAGE_TYPE_NAME(msg),
		                                              GST_MESSAGE_SRC_NAME(msg));
      gst_element_set_state (data->pipeline, GST_STATE_PAUSED);
      gst_element_set_state (data->pipeline, GST_STATE_PLAYING);
      break;

    case GST_MESSAGE_QOS:
      gst_message_parse_qos_stats (msg,
                                   &format,
                                   &processed,
                                   &dropped);
      data->drop_count = dropped;
      g_print ("++++++ Message: name: %s, source name: %s\n", GST_MESSAGE_TYPE_NAME(msg),
		                                              GST_MESSAGE_SRC_NAME(msg));
      break;

    case GST_MESSAGE_ELEMENT:
       if ( strcmp ("udpsrc0", GST_MESSAGE_SRC_NAME(msg)) == 0) {
          g_print ("++++++ Message: name: %s, source name: %s\n", GST_MESSAGE_TYPE_NAME(msg),
			                                          GST_MESSAGE_SRC_NAME(msg));
          data->drop_count += N_FRAMES;
          g_print ("[%d] total/dropped = %lu/%lu\n", data->cam_index, 
		                                     data->frame_count,
			                             data->drop_count);
       }
      break;

    default:
     /*
      if (GST_MESSAGE_TYPE (msg) == GST_MESSAGE_QOS)  {
         g_print (">>>>>> Message: name: %s, source name: %s\n", GST_MESSAGE_TYPE_NAME(msg), GST_MESSAGE_SRC_NAME(msg));
         g_print (">>>>>> QoS: unit: %d, processed: %lu, dropped %lu\n", format, processed, dropped);
      }
      */

      /* Unhandled message */
      break;
  }
}

//debug function to make sure pipeline has the expected # of elements
void pipelineElementCount()
{
    GstIterator *it = gst_bin_iterate_elements(GST_BIN(data.pipeline));
    GValue item = G_VALUE_INIT;
    int elementCount = 0;
    gboolean done = FALSE;
    
    while(!done)
    {
    	switch(gst_iterator_next(it, &item)) {
            case GST_ITERATOR_OK:
		elementCount++;
		g_value_reset(&item);
		break;
	    case GST_ITERATOR_ERROR:
	    case GST_ITERATOR_DONE:
                g_print("pipeline element count is %d \n", elementCount);
		done = TRUE;
		break;
        }
    }

    g_value_unset(&item);
    gst_iterator_free(it);
}


static GstPadProbeReturn
unlink_cb (GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
  if (GST_EVENT_TYPE (GST_PAD_PROBE_INFO_DATA (info)) != GST_EVENT_EOS)
    return GST_PAD_PROBE_PASS;

  gst_pad_remove_probe (pad, GST_PAD_PROBE_INFO_ID (info));

  gst_element_set_state (recSink->sink, GST_STATE_NULL);
  gst_element_set_state (recSink->mux, GST_STATE_NULL);
  gst_element_set_state (recSink->encode, GST_STATE_NULL);

  gst_bin_remove (GST_BIN (data.pipeline), recSink->encode);
  gst_bin_remove (GST_BIN (data.pipeline), recSink->mux);
  gst_bin_remove (GST_BIN (data.pipeline), recSink->sink);

  g_print ("removed\n");

  return GST_PAD_PROBE_REMOVE;
}

static GstPadProbeReturn
unlink_cb1 (GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
    GstPad *sinkpad, *eossink, *qsink;

  GST_DEBUG_OBJECT (pad, "pad is blocked now");

  /* remove the probe first */
  gst_pad_remove_probe (pad, GST_PAD_PROBE_INFO_ID (info));

  /* install new probe for EOS */
  sinkpad = gst_element_get_static_pad (recSink->sink, "sink");
  gst_pad_add_probe (sinkpad, GST_PAD_PROBE_TYPE_BLOCK, unlink_cb, user_data, NULL);

  /* push EOS into the element, the probe will be fired when the
   * EOS leaves the effect and it has thus drained all of its data */
  eossink = gst_element_get_static_pad (recSink->encode, "sink"); 
  gst_pad_send_event (eossink, gst_event_new_eos ());
  gst_object_unref (eossink);

  // remove queue element
  //unlink queue element sink pad from tee src
    g_print("unlink queue element sink pad from tee src\n");
  qsink = gst_element_get_static_pad (recSink->queue, "sink");
  gst_pad_unlink (recSink->teepad, qsink);
  gst_object_unref (qsink);
  // remove queue element from pipeline
  g_print("remove queue element from pipeline\n");
  gst_element_set_state (recSink->queue, GST_STATE_NULL);
  gst_bin_remove (GST_BIN (data.pipeline), recSink->queue);

  return GST_PAD_PROBE_OK;
}

gboolean timeout_callback(gpointer d)
{
    static int i = 0;
    
    if(recSink == NULL)
        recSink = g_new0 (Sink, 1);
    
    i++;
    g_print("timeout_callback called %d times\n", i);
    if ( (25 == i)||(80==i))
    {
        GstPad *sinkpad, *teepad, *teesink;
        GstElement *tee, *queue, *encode, *mux, *sink;
        GstPadTemplate *templ, *temp2;
       
        tee = gst_bin_get_by_name(GST_BIN(data.pipeline), "dec");
        templ = gst_element_class_get_pad_template (GST_ELEMENT_GET_CLASS (tee), "src_%u");
        teepad = gst_element_request_pad (tee, templ, NULL, NULL);
        queue = gst_element_factory_make ("queue", NULL);
        encode = gst_element_factory_make ("omxh265enc", NULL);
        mux = gst_element_factory_make ("qtmux", NULL);
        sink = gst_element_factory_make ("filesink", NULL);
	if(i==25)
            g_object_set (sink, "location", "aaa.mp4", NULL);
	else
	    g_object_set (sink, "location", "bbb.mp4", NULL);
        //data.qmx = mux;
        gst_bin_add_many (GST_BIN (data.pipeline), queue, encode, mux, sink, NULL);

        gst_element_sync_state_with_parent (sink);
        gst_element_sync_state_with_parent (mux);
        gst_element_sync_state_with_parent (encode);
        gst_element_sync_state_with_parent (queue);
        gst_element_link_many (queue, encode, mux, sink, NULL);
 
        sinkpad = gst_element_get_static_pad (queue, "sink");
        gst_pad_link (teepad, sinkpad);
        gst_object_unref (sinkpad);
        recSink->teepad = teepad;
        recSink->sink = sink;
        recSink->mux = mux;
        recSink->encode = encode;
        recSink->queue = queue;	
        pipelineElementCount();  // debug function
        return TRUE;
    }
    if ((75 == i)||120==i)
    {
        gst_pad_add_probe (recSink->teepad, GST_PAD_PROBE_TYPE_IDLE, unlink_cb1, d, NULL);
        pipelineElementCount();   //debug function to make sure pipeline has the expected # of elements
    }

    if( 125 == i)
    {
        pipelineElementCount();
	return FALSE;
    }

    return TRUE;
}


int main (int argc, char *argv[])
{
  GstElement *pipeline;
  GstBus *bus;
  GstStateChangeReturn ret;
  GMainLoop *main_loop;
  //CustomData data;
  GstElement *elm_identity, *elm_udpsrc;
  gint32 index;
  gchar pipe_string[1024];
  

  /* Initialize GStreamer */
  gst_init (&argc, &argv);

  /* Initialize our data structure */
  memset (&data, 0, sizeof (data));
  
  if (argc < 2) {
      g_print ("*** Missing required camera index [0,1,2,3]\n");
      return -1;
  }
  index = atoi (argv[1]);
  if (index < 0 || index > 3) {
      g_print ("*** Invalid argument");
      return -1;
  }
  data.cam_index = index;
  
  sprintf (pipe_string, "udpsrc port=%d"
" ! application/x-rtp, media=video, clock-rate=90000, encoding-name=H265,playload=96"
" ! rtpjitterbuffer latency=5 do-lost=true"
" ! rtph265depay"
" ! h265parse"
" ! video/x-h265,alignment=au"
" ! nvv4l2decoder disable-dpb=true enable-max-performance=1"
" ! queue max-size-bytes=0"
" ! videorate max-rate=30"
" ! tee name=dec dec."
" ! queue ! nvvidconv ! nveglglessink window-width=720 window-height=480 sync=false qos=false", 5004+index);

  printf ("%s\n", pipe_string);

  pipeline = gst_parse_launch (pipe_string, NULL);

  bus = gst_element_get_bus (pipeline);

  /* Start playing */
  ret = gst_element_set_state (pipeline, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    g_printerr ("Unable to set the pipeline to the playing state.\n");
    gst_object_unref (pipeline);
    return -1;
  } else if (ret == GST_STATE_CHANGE_NO_PREROLL) {
    data.is_live = TRUE;
  }

  main_loop = g_main_loop_new (NULL, FALSE);
  data.loop = main_loop;
  data.pipeline = pipeline;
  data.frame_count = 0;
  data.drop_count = 0;

  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message", G_CALLBACK (cb_message), &data);

  // add source to default context
  g_timeout_add (100 , timeout_callback , NULL); 
  g_main_loop_run (main_loop);

  /* Free resources */
  g_main_loop_unref (main_loop);
  gst_object_unref (bus);
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);

  return 0;
}
