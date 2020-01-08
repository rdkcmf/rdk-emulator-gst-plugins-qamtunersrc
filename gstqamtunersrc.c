/*
 * Copyright 2014 RDK Management
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation, version 2
 * of the license.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "gstqamtunersrc.h"

#ifdef QAM_LIVE_RPI
  #include <gst/net/gstnetaddressmeta.h>
  #if GLIB_CHECK_VERSION (2, 35, 7)
    #include <gio/gnetworking.h>
  #else
    #include <gst/net/gstnetaddressmeta.h>
  #endif    
  #ifdef HAVE_SYS_SOCKET_H
    #include <sys/socket.h>
  #endif
#endif

#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#define QAMVERSION "0.10.32"
#define GST_PACKAGE_NAME "GStreamer"
#define GST_PACKAGE_ORIGIN "http://gstreamer.net/"
#define DEFAULT_TUNECONFIG_PATH "/usr/bin/tune.config"
#define DEFAULT_BITRATE_PATH "/usr/bin/bitrate.config"
#define DEFAULT_HDHOMERUN_PATH "/usr/bin/hdhomerun.config"
#define CHANNEL_INFO_DAT "/opt/channel_info.dat"
#define CHANNEL_INFO_CONFIG "/usr/bin/channel_info.config"

#ifdef QAM_LIVE_RPI 
  /* not 100% correct, but a good upper bound for memory allocation purposes */
  #define IPV4_PACKET_MAX_SIZE (65536 - 8)
  /* Enabled DEBUG to Trace logs */
  #define DEBUG 0
  /* Use TRACE to enable QAM Logging */
  #define TRACE if (DEBUG) printf("Function:%s FILE:%s LINE:%d",__func__,__FILE__, __LINE__);
#endif


static GstStaticPadTemplate Qamsrctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

#ifdef QAM_LIVE_RPI
#define DEFAULT_PORT_NUMBER                2005
#define DEFAULT_MULTICAST_IP_ADDRESS     "0.0.0.0"
#endif

static int
Qam_gst_open (const gchar * filename, int flags, int mode)
{
  return open (filename, flags, mode);
}

enum
{
  QAM0,
  QAM_LOCATION,
  QAM_PGM_NO,
  QAM_TUNEPARAMS,
  QAM_TUNERID
};


#ifdef QAM_LIVE_RPI
static gboolean gst_QamTuner_src_close (QamTunerSrc * qamsrc);
static gboolean gst_QamTuner_src_open (QamTunerSrc * qamsrc);
static void gst_QamTuner_src_release_memory (QamTunerSrc * qamsrc);
static gboolean gst_QamTuner_src_ensure_mem (QamTunerSrc * qamsrc);
static gboolean gst_QamTuner_src_set_address (QamTunerSrc * qamsrc, const gchar * port_number);
static GstFlowReturn gst_QamTuner_src_create_udp_read (QamTunerSrc *qamsrc, GstBuffer ** buf);
#endif

static int hdhomerun_tune(QamTunerSrc *qamsrc);

static guint gstqamtunersrc_status_signal;

static void gst_QamTuner_src_finalize (GObject * object);

static void gst_QamTuner_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_QamTuner_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_QamTuner_src_start (GstBaseSrc * basesrc);
static gboolean gst_QamTuner_src_stop (GstBaseSrc * basesrc);

static gboolean gst_QamTuner_src_is_seekable (GstBaseSrc * src);
static gboolean gst_QamTuner_src_get_size (GstBaseSrc * src, guint64 * size);
static GstFlowReturn gst_QamTuner_src_create (GstBaseSrc * src, guint64 offset,
    guint length, GstBuffer ** buffer);
static gboolean gst_QamTuner_src_query (GstBaseSrc * src, GstQuery * query);

static void gst_QamTuner_src_uri_handler_init(gpointer g_iface, gpointer iface_data);

static void calculate_channel_no(guint frequency);
static void store_channel_info(char *chno);

//  GST_DEBUG_CATEGORY_INIT (gst_QamTuner_src_debug, "filesrc", 0, "filesrc element");

#ifdef USE_GST1

//G_DEFINE_TYPE (QamTunerSrc, gst_QamTuner_src, GST_TYPE_PUSH_SRC);
G_DEFINE_TYPE (QamTunerSrc, gst_QamTuner_src, GST_TYPE_BASE_SRC);
#define parent_class gst_QamTuner_src_parent_class

#else

GST_BOILERPLATE (QamTunerSrc, gst_QamTuner_src, GstBaseSrc, GST_TYPE_BASE_SRC);

static void
gst_QamTuner_src_base_init (gpointer g_class)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_set_details_simple (gstelement_class,
      "Qam Source",
      " TS File",
      "emulating tuner feature",
      "kalyan kumar <kalyankumar.nagabhirava@lnttechservices.com>");
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&Qamsrctemplate));
}

#endif

static void
gst_QamTuner_src_class_init (QamTunerSrcClass * klass)
{
  GObjectClass *qamobject_class;
  GstBaseSrcClass *qamsrc_class;
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);

  qamobject_class = G_OBJECT_CLASS (klass);
  qamsrc_class = GST_BASE_SRC_CLASS (klass);

  qamobject_class->set_property = gst_QamTuner_src_set_property;
  qamobject_class->get_property = gst_QamTuner_src_get_property;


  g_object_class_install_property (qamobject_class, QAM_LOCATION,
      g_param_spec_string ("location", "Qam File Location",
          "Qam Location of the file to read", NULL,
        (GParamFlags) ( G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY)));
	    g_object_class_install_property (qamobject_class, QAM_PGM_NO,
                        g_param_spec_uint ("pgmno", "pgmno", "Program number",
                                        0, G_MAXUINT, 0, (GParamFlags)G_PARAM_READWRITE));

  g_object_class_install_property (qamobject_class, QAM_TUNEPARAMS,
                        g_param_spec_boxed ("tuneparams", "Tune Params",
                                        "GstStructure specifying tune params  - frequency(in KHz) and modulation (QAMSRC_MODULATION_* values). Initiates tuning",
                                        GST_TYPE_STRUCTURE,  (GParamFlags)(G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS)));

 g_object_class_install_property (qamobject_class, QAM_TUNERID,
			g_param_spec_uint ("tunerid", "tunerid", "Tuner ID",
					0, G_MAXUINT, 0, (GParamFlags)G_PARAM_READABLE));

  gstqamtunersrc_status_signal =
            g_signal_new ("qamtunersrc-status", G_TYPE_FROM_CLASS (gstelement_class),
                          (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
                          G_STRUCT_OFFSET (QamTunerSrcClass, qamtunersrcstatuscb), NULL, NULL,
                          g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1, G_TYPE_INT);

  qamobject_class->finalize = gst_QamTuner_src_finalize;
  qamsrc_class->start = GST_DEBUG_FUNCPTR (gst_QamTuner_src_start);
  qamsrc_class->stop = GST_DEBUG_FUNCPTR (gst_QamTuner_src_stop);
  qamsrc_class->is_seekable = GST_DEBUG_FUNCPTR (gst_QamTuner_src_is_seekable);
  qamsrc_class->get_size = GST_DEBUG_FUNCPTR (gst_QamTuner_src_get_size);
  qamsrc_class->create = GST_DEBUG_FUNCPTR (gst_QamTuner_src_create);
  qamsrc_class->query = GST_DEBUG_FUNCPTR (gst_QamTuner_src_query);

#ifdef USE_GST1
  gst_element_class_set_static_metadata (gstelement_class,
      "Qam Source",
      " TS File/UDP Src",
      "emulating tuner feature",
      "kalyan kumar <kalyankumar.nagabhirava@lnttechservices.com>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&Qamsrctemplate));
#endif
}

static void
#ifdef USE_GST1
gst_QamTuner_src_init (QamTunerSrc * qamsrc)
#else
gst_QamTuner_src_init (QamTunerSrc * qamsrc, QamTunerSrcClass * g_class)
#endif
{

  qamsrc->linearloc = NULL;
  qamsrc->qamfd = 0;
  qamsrc->linearpath = NULL;

  qamsrc->touch = TRUE;
//Set the default bitrate 
  qamsrc->m_bitRate = 6;

#ifdef QAM_LIVE_RPI 
  qamsrc->address = g_strdup (DEFAULT_MULTICAST_IP_ADDRESS);
  qamsrc->port = DEFAULT_PORT_NUMBER;
  qamsrc->socket = NULL;
  qamsrc->reuse = TRUE;
  gst_base_src_set_format (GST_BASE_SRC (qamsrc), GST_FORMAT_TIME);
  gst_base_src_set_do_timestamp (GST_BASE_SRC (qamsrc), TRUE);
#endif
}

static void
gst_QamTuner_src_finalize (GObject * obj)
{
  QamTunerSrc *qamsrc;

  qamsrc = GST_QAM_TUNER_SRC (obj);

  g_free (qamsrc->linearloc);
  g_free (qamsrc->linearpath);

#ifdef QAM_LIVE_RPI
  g_free (qamsrc->address);
  qamsrc->address = NULL;

  if (qamsrc->socket)
    g_object_unref (qamsrc->socket);
  qamsrc->socket = NULL;

  if (qamsrc->used_socket)
    g_object_unref (qamsrc->used_socket);
  qamsrc->used_socket = NULL;

#endif
  G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static gboolean
gst_QamTuner_src_set_location (QamTunerSrc * qamsrc, const gchar * qamlocation)
{
  GstState state;

  GST_OBJECT_LOCK (qamsrc);
  state = GST_STATE (qamsrc);
  if (state != GST_STATE_READY && state != GST_STATE_NULL)
    goto wrongstate;
  GST_OBJECT_UNLOCK (qamsrc);

  g_free (qamsrc->linearloc);
  g_free (qamsrc->linearpath);

  if (qamlocation == NULL) {
    qamsrc->linearloc = NULL;
    qamsrc->linearpath = NULL;
  } else {
    qamsrc->linearloc = g_strdup (qamlocation);
    qamsrc->linearpath = gst_filename_to_uri (qamlocation, NULL);
    printf ("linearloc : %s", qamsrc->linearloc);
    printf ("linearpath      : %s", qamsrc->linearpath);
  }
  g_object_notify (G_OBJECT (qamsrc), "location");
#ifndef USE_GST1
gst_uri_handler_new_uri(GST_URI_HANDLER(qamsrc), qamsrc->linearpath);
#endif
  return TRUE;

wrongstate:
  {
    g_warning ("Qamsrc playing is "
        "now open is not supported.");
    GST_OBJECT_UNLOCK (qamsrc);
    return FALSE;
  }
}
static int readline(FILE *f, char *buffer, size_t len)
{
   int i =0;
  if(f == NULL)
  {
	printf("File open Failed\n");
	return -1;
  }

   memset(buffer, 0, len);

   for (i = 0; i < len; i++)
   {
      int c = fgetc(f);

      if (!feof(f))
      {
         if (c == '\r')
            buffer[i] = 0;
         else if (c == '\n')
         {
            buffer[i] = 0;

            return i+1;
         }
         else
	{
            buffer[i] = c;
	
	}
      }
      else
      {
         return -1;
      }
   }
}
static void GetBitRate(QamTunerSrc *src,char* filename)
{
    char *ret = NULL;
    char data1[512];
    FILE *fp;
    int len=0;
    char *bitrate;
    int IsTune=1;
    int line;
       //fp=fopen("/usr/bin/bitrate.config","r");	
       fp=fopen(DEFAULT_BITRATE_PATH,"r");	
        if(fp)
     {
	while(IsTune)
        {
                line=readline(fp,data1,512);
                if(line > 0)
                {
                        len = strlen(filename);


                        ret = strstr(data1,filename);
			
                        if(ret)
                        {
                                bitrate = len + ret + 1;
 				src->m_bitRate=atof(bitrate);
                         	printf("###########bitrate = %f#################\n",src->m_bitRate); 
                                IsTune=0;
                        }
                        else
                        {
                                continue;
                        }
                }
                else
                {
                        break;
                }
        }
        fclose(fp);
      }

}
static void open_file(GObject * object,char *tune_params)
{
    char *ret = NULL;
    char data1[512];
    FILE *fp;
    int len=0;
    char *filename;
    int IsTune=1;
    int line;
    QamTunerSrc *src;
    char *temp = NULL; 
	char *config_params = NULL, *port_number = NULL;

    src = GST_QAM_TUNER_SRC (object);

 	//fp=fopen("/usr/bin/tune.config","r");
 	fp=fopen(DEFAULT_TUNECONFIG_PATH,"r");
        while(IsTune)
        {
		if(fp)
		{
			memset(data1,'\0',sizeof(data1));
			line = readline(fp,data1,512);
			temp = data1;
			//skipping the blank space
			while(*temp == ' ' && temp++);
			// ignore commented line
			if(*temp == '#' || *temp == '\\' || *temp == '/' )
				continue;
			if(line>0)
			{
				len = strlen(tune_params);

				ret = strstr(data1,tune_params);
				if(ret)
				{
					printf("tune param for file==%s\n",tune_params);
					config_params = ret + len + 1;
					printf("config param %s\n",config_params);
				    char *tmp = NULL;
					tmp = strtok(config_params," ");
					if(tmp){
					    filename = tmp;
						port_number = strtok(NULL, " ");
					}
					else {
						printf("\nIncorrect format %s for %s",config_params, tune_params);
						printf("\nfilename and port number should be space separated");
						continue;
					}
#ifdef QAM_LIVE_RPI 
					printf("Retrived Port Number = %s\n",port_number);
					gst_QamTuner_src_set_address(src, port_number);
#endif					
					printf("Retrived Filename = %s\n",filename);
					gst_QamTuner_src_set_location (src, filename);
					GetBitRate(src,filename);
					IsTune=0;
				}
				else
				{
					continue;
				}
			}
			else
			{
				break;
			}
		}
		else
		{
			printf("Fopen Failed\n");
			IsTune = 0;
		}
        }
        fclose(fp);
}

/** parse hdhomerun.config file, populate hdhomerun_device_id, hdhomerun_tuner
 *  hdhomerun_channel_map members of qamsrc struct                
 */ 
static void read_hdhomerun_config(QamTunerSrc *qamsrc)            
{   
    FILE *fp;                                                     
    fp = fopen(DEFAULT_HDHOMERUN_PATH, "r");                      
    char buf[512];                                                
    int ret;
    char *tmp;                                                    
            
    if (fp) 
    {           
        while ((ret = readline(fp, buf, 512)) != -1)              
        {                                                         
            if (ret > 0)
            {                                                     
                if (strstr(buf, "#"))
                    continue;                                     
                                
                tmp = strtok(buf, ":");
                                                                  
                if (strcmp(tmp, "device_id") == 0)        
                {                                                 
                    /** HDHOMERUN device id is in hex */          
                    qamsrc->hdhomerun_device_id = strtoul(strtok(NULL, ""),                                                          
                                                          NULL, 16); 
                }else if (strcmp(tmp, "tuner") == 0)
                {                                                 
                    /** HDHOMERUN tuner id */
                    qamsrc->hdhomerun_tuner = atoi(strtok(NULL, ""));     
                }else if(strcmp(tmp, "channel_map") == 0)         
                {   
                    /** HDHOMERUN channel map */                  
                    strncpy(qamsrc->hdhomerun_channel_map, strtok(NULL, ""),14);                                                     
                }                                                 
                                                                  
            }                                                     
        }                                                         
        
        fclose(fp);                                               
    }                                                             
}                                                                 

/** Set GST QAMTuner Properties */                                
char tune_params[250];      


static void
gst_QamTuner_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  QamTunerSrc *qamsrc;

  g_return_if_fail (GST_IS_QAM_TUNER_SRC (object));

  qamsrc = GST_QAM_TUNER_SRC (object);

  switch (prop_id) {
    case QAM_LOCATION:
      gst_QamTuner_src_set_location (qamsrc, g_value_get_string (value));
       qamsrc->IsStart=1;
       qamsrc->m_bitRate=6;
      break;
    case QAM_TUNEPARAMS:
          {
                       GST_INFO_OBJECT( object, "Set Property: PROP_TUNEPARAMS\n");
                        const GstStructure *tuneinfo = gst_value_get_structure (value);
			printf("#####################################################\n");
			printf("GstQAMtunersrc :	Set the Tune information\n");
			printf("#####################################################\n");
                        if (NULL != tuneinfo )
                        {
                                gboolean ret;
                                guint frequency;
                                guint modulation;

                                ret = gst_structure_get_uint (tuneinfo, "frequency",
                                        &frequency);
                                if ( FALSE == ret )
                                {
                                        GST_ERROR_OBJECT( object, "Set frequency failed\n");
                                }
                                
				calculate_channel_no(frequency/1000);

				ret = gst_structure_get_uint (tuneinfo, "modulation",
                                        (guint*)&modulation);
                                if ( FALSE == ret )
                                {
                                        GST_ERROR_OBJECT( object, "Set modulation failed\n");
                                }
			  sprintf(tune_params,"modulation:%d frequency:%d",modulation,(frequency/1000));
				printf("tune params = %s\n",tune_params);
			}
           }
        break;
      case QAM_PGM_NO:
      //      int programNumber = g_value_get_uint (value);
        //    GST_DEBUG_OBJECT( object, "Set Property: PROP_PGM_NO : %d\n", programNumber);
 
         {                                                         
            int status = 0;
            char programNumberString[32];                         
                        
            /** get program_number from sidb.xml */               
                                
            guint programNumber = g_value_get_uint(value);        
                                
            GST_DEBUG_OBJECT(object, "Set Property: PROP_PGM_NO : %d\n",
                             programNumber);                      
                                
            /** store program number in qamsrc struct. if program_number is -1
             * in sidb.xml, g_value_get_uint returns an           
             * unsigned short(two's complement), even though the size of gint is                                                     
             * 4 bytes, this is because of Gvalue struct          
             * (parameter to g_value_get_uint) which uses two element data union                                                     
             * .Since, it returns 16 bit unsigned value we are comparing it with
             * 65535.           
             */                 
                                                                  
            (programNumber >= 65535) ? (qamsrc->program_number = -1) :             
            (qamsrc->program_number = programNumber);             
                                
            if(qamsrc->program_number == -1)
            {
                open_file(object,tune_params);                    
            }             
            else
            {                   
#ifdef QAM_LIVE_RPI
                //reading port number
                open_file(object,tune_params);                    
#else
                /** get the tuner frequency from sidb.xml. Save to the             
                 *  qamsource structure. Multiply by 1000
                 *  to set frequency in MHZ
                 */     
                qamsrc->frequency = qamsrc->frequency * 1000;     
                /** read HDHOMERUN specific elements into qamsrc */                
                read_hdhomerun_config(qamsrc);                    
                /** Set the tuner */
                if((hdhomerun_tune(qamsrc) < 0))
                {
                    GST_ERROR_OBJECT(object, "HD Home Run Tuning failed\n");   
                }                                                 
#endif
            }
            qamsrc->IsStart = 1;                                  
        }
     
       break;
    default:
      printf("Error: gst_QamTuner_src_set_property no property set\n");
      break;
  }
}

/*Store current channel information*/

static void store_channel_info(char *buff)
{
        FILE *fp;
        fp = fopen(CHANNEL_INFO_DAT,"w");
        if(fp)
        {
                while(*buff)
                {
                        fputc(*buff,fp);
                        buff++;
                }
                fclose(fp);
        }
        else
        {
                printf("file %s cannot be opened\n", CHANNEL_INFO_DAT);
        }
}

static void calculate_channel_no(guint freq)
{
        char ch_no[30];
        FILE *fp;
        char ch_info[512];

        sprintf(ch_no,"%d",freq-250);// 250 is base frequency
        fp = fopen(CHANNEL_INFO_CONFIG,"r");
        if(fp)
        {
                int istune = 1;
                while(istune)
                {
                        if(readline(fp,ch_info,512) > 0)
                        {
                                char *match = strstr(ch_info,ch_no);
                                if(match)
                                {
					printf("current channel info :%s\n", ch_info);
                                        store_channel_info(ch_info);
                                        istune = 0;
                                        fclose(fp);
                                }
                                else
                                {
                                        continue;
                                }

                        }
                        else
                        {
                                printf("No such channel present: %s\n",ch_no);
                                istune = 0;
                                fclose(fp);
                        }
                }
        }
        else
        {
                printf("cannot open file %s \n",CHANNEL_INFO_CONFIG);
        }

}


/** Set the HDHomeRun Tuner Parameters */

static int hdhomerun_tune(QamTunerSrc *qamsrc)
{
    int status = 0;
    char hdhomeruntune_params[32];

    gst_base_src_set_blocksize(&(qamsrc->element),VIDEO_DATA_BUFFER_SIZE_1S);

    /** Create HDHOMERUN device */
    qamsrc->pDevice = hdhomerun_device_create(qamsrc->hdhomerun_device_id,
                                              0,
                                              0,
                                              NULL);

    if(qamsrc->pDevice != NULL)
    {
        g_print("HD Home Run Device create qamsrc->pDevice 0x%x\n",
                qamsrc->pDevice);
    }
    else
    {
        g_print("HD Home Run Device create failed \n");
    }

        /** set the tuner */

    sprintf(hdhomeruntune_params, "/tuner%d", qamsrc->hdhomerun_tuner);
    status = hdhomerun_device_set_tuner_channel(qamsrc->pDevice,
                                                hdhomeruntune_params);

    if(status < 0)
    {
        g_print(" Setting the tuner failed -- %d\n ", status);
                return status;
    }

        /** set the channel map */

    status = hdhomerun_device_set_tuner_channelmap(qamsrc->pDevice,
                                                   qamsrc->hdhomerun_channel_map);

    if(status < 0)
    {
                g_print(" Setting the channel map failed -- %d\n ", status);
                return status;
    }

    /** extract tune frequency from qamsrc structure.
     *  convert to string for use in HDHOMERUN API.
     */
 sprintf(hdhomeruntune_params, "auto:%d", qamsrc->frequency);

    /** tune HDHOMERUN device */
    status = hdhomerun_device_set_tuner_channel(qamsrc->pDevice,
                                                hdhomeruntune_params);

    if(status < 0)
    {
        g_print(" Setting the mod params failed -- %d\n ", status);
        return status;
    }

    /** convert to string so it can be passed to HDHOMERUN API */

    memset(hdhomeruntune_params, 0, sizeof(hdhomeruntune_params));
    sprintf(hdhomeruntune_params, "%d", qamsrc->program_number);
    /** set tuner program number */
    status = hdhomerun_device_set_tuner_program(qamsrc->pDevice,
                                                        hdhomeruntune_params);
    if(status < 0)
    {
        g_print(" Setting the program number failed -- %d\n ", status);
    }

    /** prepare HDHOMERUN for streaming */
    status = hdhomerun_device_stream_start(qamsrc->pDevice);

    if(status <= 0)
    {
        g_print("unable to start stream\n");
        return status;
    }

    g_print(" HDHome Run Tuning Success \n");
    return status;
}


static void
gst_QamTuner_src_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  QamTunerSrc *qamsrc;
  unsigned int tunerId=0;
  g_return_if_fail (GST_IS_QAM_TUNER_SRC (object));

  qamsrc = GST_QAM_TUNER_SRC (object);

  switch (prop_id) {
    case QAM_LOCATION:
      g_value_set_string (value, qamsrc->linearloc);
      break;
    case QAM_TUNERID:
      g_value_set_uint(value, tunerId);
      break;

    default:
      printf("Error:gst_QamTuner_src_get_property no property to get\n");
      break;
  }
}


static unsigned int file_offset=0;
static void Delay(QamTunerSrc * src)

   {

      gint64 delay= 0;

      gint64 time= (g_get_monotonic_time()-src->start_time);
      double seconds= (((double)time)/1000000.0);

      double megabits= ((src->read_position*8.0)/1000000.0);
      double rate= megabits/seconds;
  //    printf("megabits %lf==rate %f=====seconds %lf===========read postion %lld=src->m_bitRate %lf=====\n",megabits,rate,seconds,src->read_position,src->m_bitRate);
      if ( rate > src->m_bitRate && src->m_bitRate > 0.0)

      {
     
         delay= (gint64)(((megabits-(seconds*(src->m_bitRate)))*1000000.0)/src->m_bitRate);
           //printf("======================microsecond to sleep======%lld===\n",delay);
        g_usleep(delay);
           //printf("==========end of sleep====================\n");
      }

   }

static GstFlowReturn
gst_QamTuner_src_create_read (QamTunerSrc * qamsrc, guint64 offset, guint length,
    GstBuffer ** buffer)
{
  int ret;
  GstBuffer *qambuf;
  static unsigned int count=0;
  const char* disableDelay=NULL;
#ifdef USE_GST1
  GstMapInfo map;
#endif
size_t Data_size;

        /** File Based Reading */
    if(qamsrc->program_number==-1)
    {

    if (G_UNLIKELY (qamsrc->read_position != offset))
    {
                off_t res;
    }

  if((qamsrc->IsStart)&&(offset))
  {
       qamsrc->start_time= g_get_monotonic_time();
       qamsrc->IsStart=0;
  }
  else
  {
	  if(count>50)
	  {        
		  disableDelay = getenv("DISABLE_QAMSRC_DELAY");
		 if(disableDelay == NULL)
			disableDelay = "FALSE";
		  if(strcmp(disableDelay, "TRUE")){
			  Delay(qamsrc);
		}
	  }
  }
  count++;
  //  printf("================================cread position is %d=======file =offset is ====%d===\n",src->read_position,file_offset);

#ifdef USE_GST1
  qambuf = gst_buffer_new_allocate (NULL, length, NULL);
#else
  qambuf = gst_buffer_try_new_and_alloc (length);
#endif

  if (G_UNLIKELY (qambuf == NULL && length > 0)) {
    GST_ERROR_OBJECT (qamsrc, "Failed to allocate %u bytes", length);
    return GST_FLOW_ERROR;
  }

#ifdef USE_GST1
  gst_buffer_map (qambuf, &map, GST_MAP_WRITE);
#endif

  if (length > 0) {

    GST_LOG_OBJECT (qamsrc, "Reading %d bytes at offset 0x%" G_GINT64_MODIFIER "x",
        length, offset);

#ifdef USE_GST1
    ret = read (qamsrc->qamfd, map.data, length);
#else
    ret = read (qamsrc->qamfd, GST_BUFFER_DATA (qambuf), length);
#endif

    if (G_UNLIKELY (ret < 0))
      goto couldNotRead;

    if (G_UNLIKELY ((guint) ret < length && qamsrc->Is_seekable))
      goto unexpectedEos;

    if (G_UNLIKELY (ret == 0 && length > 0))
    {
  //      printf("======================EOS=======================================\n");
	lseek (qamsrc->qamfd, 0, SEEK_SET);
     }
     if(ret<length)
     {
//	 printf("======================EOS=======================================\n");
        lseek (qamsrc->qamfd, 0, SEEK_SET);

     }

    length = ret;

#ifdef USE_GST1
    gst_buffer_set_size (qambuf, length);
#else
    GST_BUFFER_SIZE (qambuf) = length;
#endif

    GST_BUFFER_OFFSET (qambuf) = offset;
    GST_BUFFER_OFFSET_END (qambuf) = offset + length;

    qamsrc->read_position += length;
    if(((qamsrc->read_position+(2*192512))>=file_offset)&&((qamsrc->read_position%188)==0))
    {	 
  //      printf("======================EOS=====src->read_position==================================\n",src->read_position);
        lseek (qamsrc->qamfd, 0, SEEK_SET);
	qamsrc->read_position =0;

    }
  }
}

else
    {
    /** HDHomeRun Device Reading */

    /** allocating GstBuffer struct. The allocation is different than the file
     * source. The actual video buffer is allocated in the HDHOMERUN Lib. In
     * order to support zero copy, the same buffer is passed to the GStreamer
     * pipeline                                                   
     */

    uint8_t *data = NULL;
    while (1)                                                     
    {
        /** Trying to reduce memory footprint by going soft on run time memory.
         *  Not allocating data buffer for qambuf, using the video buffer
         *  allocated by the HDHOMERUN stream read                
         */
        data = hdhomerun_device_stream_recv(qamsrc->pDevice,
                                                                length,
                                                                &Data_size);
        if(!data)
        {                                                         
            msleep_approx(1);
            continue;
        }                                                         
        
        break;                                                    
    }                                                             

#ifdef USE_GST1
    qambuf = gst_buffer_new_wrapped_full( (GstMemoryFlags) 0, data,
        Data_size, 0, Data_size, NULL, NULL);
    if( G_UNLIKELY(qambuf == NULL))
    {
        GST_ERROR_OBJECT(qamsrc, "Failed to allocate bytes");
        return GST_FLOW_ERROR;
    }

#else
    qambuf = gst_buffer_new();

    if( G_UNLIKELY(qambuf == NULL))
    {
        GST_ERROR_OBJECT(qamsrc, "Failed to allocate bytes");
        return GST_FLOW_ERROR;
    }
    length = Data_size;
    offset = 0;                                                   
    GST_BUFFER_DATA (qambuf) = data;
    GST_BUFFER_SIZE (qambuf) = length;                            
    GST_BUFFER_OFFSET (qambuf) = offset;
    GST_BUFFER_OFFSET_END (qambuf) = offset + length;
#endif

    }


  *buffer = qambuf;

#ifdef USE_GST1
  gst_buffer_unmap (qambuf, &map);
#endif
  return GST_FLOW_OK;

couldNotRead:
  {
    GST_ELEMENT_ERROR (qamsrc, RESOURCE, READ, (NULL), GST_ERROR_SYSTEM);
#ifdef USE_GST1
    gst_buffer_unmap (qambuf, &map);
#endif
    gst_buffer_unref (qambuf);
    return GST_FLOW_ERROR;
  }
unexpectedEos:
  {
    GST_ELEMENT_ERROR (qamsrc, RESOURCE, READ, (NULL),
        ("error end of file."));
#ifdef USE_GST1
    gst_buffer_unmap (qambuf, &map);
#endif
    gst_buffer_unref (qambuf);
    return GST_FLOW_ERROR;
  }
}

static GstFlowReturn
gst_QamTuner_src_create (GstBaseSrc * basesrc, guint64 offset, guint length,
    GstBuffer ** buffer)
{
  QamTunerSrc *qamsrc;
  GstFlowReturn ret;
  qamsrc = GST_QAM_TUNER_SRC_CAST (basesrc);
  /* read live data from udp port in case of QAM_LIVE data enabled
   * and program number configured as +ve
   */
#ifdef QAM_LIVE_RPI  
  if(qamsrc->program_number == -1) {
#endif  
      ret = gst_QamTuner_src_create_read (qamsrc, offset, length, buffer);
#ifdef QAM_LIVE_RPI	  
  }
  else {
      ret = gst_QamTuner_src_create_udp_read (qamsrc, buffer);
  }
#endif  
  return ret;
}

static gboolean
gst_QamTuner_src_query (GstBaseSrc * qambasesrc, GstQuery * query)
{
  gboolean Ret = FALSE;
  QamTunerSrc *qamsrc = GST_QAM_TUNER_SRC (qambasesrc);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_URI:
      if (qamsrc->linearpath) {
        gst_query_set_uri (query, qamsrc->linearpath);
        Ret = TRUE;
      }
      break;
    default:
      Ret = FALSE;
      break;
  }

  if (!Ret)
    Ret = GST_BASE_SRC_CLASS (parent_class)->query (qambasesrc, query);

  return Ret;
}

static gboolean
gst_QamTuner_src_is_seekable (GstBaseSrc * qambasesrc)
{
  QamTunerSrc *qamsrc = GST_QAM_TUNER_SRC (qambasesrc);

  return qamsrc->Is_seekable;
}

static gboolean
gst_QamTuner_src_get_size (GstBaseSrc * qambasesrc, guint64 * size)
{
  struct stat statResults;
  QamTunerSrc *qamsrc;

  qamsrc = GST_QAM_TUNER_SRC (qambasesrc);

  if (!qamsrc->Is_seekable) {
    return FALSE;
  }

  if (fstat (qamsrc->qamfd, &statResults) < 0)
    goto CouldNotStat;

  *size = statResults.st_size*100;

  return TRUE;
  
CouldNotStat:
  {
    return FALSE;
  }
}

static gboolean
gst_QamTuner_src_start (GstBaseSrc * qambasesrc)
{
  QamTunerSrc *qamsrc = GST_QAM_TUNER_SRC (qambasesrc);

  if(qamsrc->program_number==-1) {
  if (qamsrc->linearloc == NULL || qamsrc->linearloc[0] == '\0')
    goto NoFile;

  GST_INFO_OBJECT (qamsrc, "opening qam  file %s", qamsrc->linearloc);

  qamsrc->qamfd = Qam_gst_open (qamsrc->linearloc, O_RDONLY, 0);

  if (qamsrc->qamfd < 0)
    goto OpenFail;


  qamsrc->read_position = 0;


  {

   file_offset=0;
    off_t res = lseek (qamsrc->qamfd, 0, SEEK_END);

    if (res < 0) {
      GST_LOG_OBJECT (qamsrc, "disabling seeking "
          "fail: %s", g_strerror (errno));
      qamsrc->Is_seekable = FALSE;
    } else {
      qamsrc->Is_seekable = TRUE;
	file_offset=res;
//     printf("####################################file offset is %d=============\n",file_offset);
    }
    lseek (qamsrc->qamfd, 0, SEEK_SET);
  }

  qamsrc->Is_seekable = TRUE;

  return TRUE;
} 
else {                                                       

#ifdef QAM_LIVE_RPI
      /* open qam socket on listening mode */
      if (!gst_QamTuner_src_open (qamsrc)) {
        GST_DEBUG_OBJECT (qamsrc, "failed to open socket");
        return FALSE;
      }
#endif
    qamsrc->qamfd = -1; /** We do not use file in this case */    
    file_offset = 0; /** This is global static variable. To be on safe side */                                                       
    qamsrc->read_position = 0;                                    
    /** Since this is not a file and the buffer where packets are 
     *  received over the socket. This can not be seekable device.                                                                   
     */                                                           
    qamsrc->Is_seekable = FALSE;                                  
    
    return TRUE;
    }

NoFile:
  {
    GST_ELEMENT_ERROR (qamsrc, RESOURCE, NOT_FOUND,
        (("No file name mentioned for reading.")), (NULL));
    return FALSE;
  }
OpenFail:
  {
   
        GST_ELEMENT_ERROR (qamsrc, RESOURCE, OPEN_READ,
            (("Could not open file \"%s\" for reading."), qamsrc->linearloc),
            GST_ERROR_SYSTEM);
    
    return FALSE;
  }
}

static gboolean
gst_QamTuner_src_stop (GstBaseSrc * qambasesrc)
{
  QamTunerSrc *qamsrc = GST_QAM_TUNER_SRC (qambasesrc);

  if(qamsrc->qamfd) {
	  close (qamsrc->qamfd);
  }
  qamsrc->qamfd = 0;
#ifdef QAM_LIVE_RPI
  /* close qam socket on receiving stop request */
  gst_QamTuner_src_close(qamsrc);
#endif
  return TRUE;
}
static gboolean
QamTunerSrc_init (GstPlugin * plugin)
{
  gst_element_register (plugin, "qamtunersrc", GST_RANK_NONE,
      gst_QamTuner_src_get_type ());

  return TRUE;
}

#ifndef PACKAGE
#define PACKAGE "emulate-qamtuning"
#endif

GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
#ifdef USE_GST1
    qamtunersrc,
#else
    "qamtunersrc",
#endif
    "reads file data and outputs in specfic bitrate",
    QamTunerSrc_init,
    QAMVERSION,
    "LGPL",
    GST_PACKAGE_NAME,
    GST_PACKAGE_ORIGIN )

#ifdef QAM_LIVE_RPI 
/* Network GST APIs has been reference from udpsrc plugin to 
 * make qamsrc read UDP data for live channel playback
 */
static gboolean
gst_QamTuner_src_alloc_mem (QamTunerSrc * qamsrc, GstMemory ** p_mem, GstMapInfo * map,
    gsize size)
{
  GstMemory *temp = NULL;
  /* Enable DEBUG for logging */
  TRACE;
  /* memory allocation with required size */
  temp = gst_allocator_alloc (qamsrc->allocator, 
                             size, 
                             &qamsrc->params);

  TRACE;
  if (!gst_memory_map (temp, map, GST_MAP_WRITE)) 
  {
    gst_memory_unref (temp);
    memset (map, 0, sizeof (GstMapInfo));
    return FALSE;
  }
  /* assigning the memory allocated */
  *p_mem = temp;
  TRACE;
  return TRUE;
}

/* Ensure to assign enough memory to read data */
static gboolean
gst_QamTuner_src_ensure_mem (QamTunerSrc * qamsrc)
{
  if (qamsrc->mem == NULL) {
    gsize extra_mem = 1500;

    TRACE;
    if (qamsrc->max_size > 0 && qamsrc->max_size < extra_mem)
      extra_mem = qamsrc->max_size;

    if (!gst_QamTuner_src_alloc_mem (qamsrc, &qamsrc->mem, &qamsrc->map, extra_mem))
      return FALSE;

    qamsrc->vec[0].buffer = qamsrc->map.data;
    qamsrc->vec[0].size = qamsrc->map.size;
  }

  if (qamsrc->mem_max == NULL) {
    gsize max_size = IPV4_PACKET_MAX_SIZE;

    if (!gst_QamTuner_src_alloc_mem (qamsrc, &qamsrc->mem_max, &qamsrc->map_max, max_size))
      return FALSE;

    qamsrc->vec[1].buffer = qamsrc->map_max.data;
    qamsrc->vec[1].size = qamsrc->map_max.size;
  }

  return TRUE;
}

static gboolean
gst_QamTuner_src_close (QamTunerSrc * qamsrc)
{
  GST_DEBUG ("closing sockets");

  if (qamsrc->used_socket) {
    if (qamsrc->close_socket || !qamsrc->external_socket) {
      GError *err = NULL;
      if (!g_socket_close (qamsrc->used_socket, &err)) {
        GST_ERROR_OBJECT (qamsrc, "Failed to close socket: %s", err->message);
        g_clear_error (&err);
      }
    }

    TRACE;
    g_object_unref (qamsrc->used_socket);
    qamsrc->used_socket = NULL;
    g_object_unref (qamsrc->adr);
    qamsrc->adr = NULL;
  }

  gst_QamTuner_src_release_memory (qamsrc);
  TRACE;

  return TRUE;
}

static void
gst_QamTuner_src_release_memory (QamTunerSrc * qamsrc)
{
  TRACE;
  if (qamsrc->mem != NULL) {
    gst_memory_unmap (qamsrc->mem, &qamsrc->map);
    gst_memory_unref (qamsrc->mem);
    qamsrc->mem = NULL;
  }
  if (qamsrc->mem_max != NULL) {
    gst_memory_unmap (qamsrc->mem_max, &qamsrc->map_max);
    gst_memory_unref (qamsrc->mem_max);
    qamsrc->mem_max = NULL;
  }
  qamsrc->vec[0].buffer = NULL;
  qamsrc->vec[0].size = 0;
  qamsrc->vec[1].buffer = NULL;
  qamsrc->vec[1].size = 0;
  TRACE;
  if (qamsrc->allocator != NULL) {
    gst_object_unref (qamsrc->allocator);
    qamsrc->allocator = NULL;
  }
  TRACE;
}

static GInetAddress *
gst_QamTuner_src_resolve (QamTunerSrc * qamsrc, const gchar * address) {
  GInetAddress *iAdr;

  iAdr = g_inet_address_new_from_string (address);
  TRACE;
  if (iAdr == NULL) {
    GError *error = NULL;
    GResolver *name_resolver;
    GList *res;
    name_resolver = g_resolver_get_default ();  /* get resolver to resolve INET Address */
    res = g_resolver_lookup_by_name (name_resolver, address, NULL, &error);
    TRACE;
    if (!res) {
      GST_ERROR_OBJECT (qamsrc, "Failed to resolve %s: %s", address, error->message);
      g_clear_error (&error);
      /* unref the name resolver and return */
      g_object_unref (name_resolver);
      return NULL;
    }
    TRACE;
    /* get INET address and return */
    iAdr = G_INET_ADDRESS (g_object_ref (res->data));
    g_resolver_free_addresses (res);
    g_object_unref (name_resolver);
  }
  TRACE;
  return iAdr;
}

static gboolean
gst_QamTuner_src_open (QamTunerSrc * qamsrc)
{
  GInetAddress *adr, *bind_adr;
  GSocketAddress *bind_sadr;
  GError *error = NULL;

  TRACE;
  if (qamsrc->socket == NULL) {
    /* resolve INET address to create socket */
    adr = gst_QamTuner_src_resolve (qamsrc, qamsrc->address);
    if (!adr) {
    /* fail to resolve the address */
        GST_ERROR_OBJECT(qamsrc,"Failed to resolve the address\n");
        return FALSE;
    }        

    TRACE;
    /* creating socket */
    if ((qamsrc->used_socket =
            g_socket_new (
            g_inet_address_get_family (adr), 
            G_SOCKET_TYPE_DATAGRAM, 
            G_SOCKET_PROTOCOL_UDP, 
            &error)) == NULL) 
    {
        GST_ERROR_OBJECT(qamsrc,"Failed to create the socket with error: %s", error->message);
        g_clear_error (&error);
        g_object_unref (adr);
        return FALSE;
    }
    qamsrc->external_socket = FALSE;

    TRACE;
    if (qamsrc->adr)
      g_object_unref (qamsrc->adr);

    qamsrc->adr = G_INET_SOCKET_ADDRESS (g_inet_socket_address_new (adr, qamsrc->port));

    bind_adr = G_INET_ADDRESS (g_object_ref (adr));

    g_object_unref (adr);

    bind_sadr = g_inet_socket_address_new (bind_adr, qamsrc->port);

    g_object_unref (bind_adr);

    TRACE;
    if (!g_socket_bind (qamsrc->used_socket, bind_sadr, qamsrc->reuse, &error)) {
        GST_ERROR_OBJECT(qamsrc,"bind failed with error: %s", error->message);
        g_clear_error (&error);
        g_object_unref (bind_sadr);
        gst_QamTuner_src_close (qamsrc);
        return FALSE;
    }
    g_object_unref (bind_sadr);
    TRACE;
  } else {
    TRACE;
    qamsrc->used_socket = G_SOCKET (g_object_ref (qamsrc->socket));
    qamsrc->external_socket = TRUE;

    if (qamsrc->adr)
      g_object_unref (qamsrc->adr);

    TRACE;
    qamsrc->adr = G_INET_SOCKET_ADDRESS (g_socket_get_local_address (qamsrc->used_socket, &error));
    if (!qamsrc->adr) {
        TRACE;
        GST_ERROR_OBJECT(qamsrc,"local socket address failed: %s", error->message);
        g_clear_error (&error);
        gst_QamTuner_src_close (qamsrc);
        return FALSE;
    }
    TRACE;
  }

  qamsrc->allocator = NULL;
  gst_allocation_params_init (&qamsrc->params);

  qamsrc->max_size = 0;
  TRACE;

  return TRUE;
}

static gboolean
gst_QamTuner_src_set_address (QamTunerSrc * qamsrc, const gchar * port_number)
{
  GstState state;

  GST_OBJECT_LOCK (qamsrc);
  state = GST_STATE (qamsrc);
  GST_OBJECT_UNLOCK (qamsrc);

  if (state != GST_STATE_READY && state != GST_STATE_NULL) {
    GST_WARNING_OBJECT (qamsrc, "Qamsrc playing is reading from new port number is not supported.");
    return FALSE;
  }

  g_free (qamsrc->address);

  TRACE;
  if (port_number == NULL) {
    qamsrc->port = 0;
    qamsrc->address = NULL;
  } else {
    /* listen to default route address */
    TRACE;
    qamsrc->address = g_strdup ("0.0.0.0");
    qamsrc->port = (gint)atoi (port_number);
    printf ("\naddress : %s", qamsrc->address);
    printf ("\nport number      : %d", qamsrc->port);
  }
  return TRUE;
}

static GstFlowReturn
gst_QamTuner_src_create_udp_read (QamTunerSrc *qamsrc, GstBuffer ** buf)
{
  TRACE;
  GstBuffer *outbuf = NULL;
  GSocketAddress *saddr = NULL;
  gint flags = G_SOCKET_MSG_NONE;
  gboolean read = TRUE;
  GError *err = NULL;
  gssize size;
  gsize offset;
  GstFlowReturn ret;

  TRACE;
  /* ensure that enough memory has been allocated */
  if (!gst_QamTuner_src_ensure_mem(qamsrc))
  {
    GST_ERROR_OBJECT(qamsrc,"Failed to allocate or map memory\n");
    return GST_FLOW_ERROR;
  }

  TRACE;
  while (read) {

    TRACE;
    if (!g_socket_condition_timed_wait (qamsrc->used_socket, 
                                        G_IO_IN | G_IO_PRI,
                                        -1, 
                                        NULL, 
                                        &err)) {
          /* in case of timeout state keep listening for data */                                
          GST_ERROR_OBJECT(qamsrc,"select error: %s", err->message);
          g_clear_error (&err);
          TRACE;
          continue;
    }
    size = g_socket_receive_message (qamsrc->used_socket, 
                                 &saddr, 
                                 qamsrc->vec, 
                                 2,
                                 NULL, 
                                 NULL, 
                                 &flags, 
                                 NULL, 
                                 &err);

    /* check if read the data successfully */
    if (G_UNLIKELY (size < 0)) {
        /* check if host is not reachable 
         * in that case keep listing to the port 
         */
        TRACE;
        if (g_error_matches (err, 
                    G_IO_ERROR, 
                    G_IO_ERROR_HOST_UNREACHABLE)) {
            g_clear_error (&err);
            TRACE;
            continue;
        }
        else {
            GST_ERROR_OBJECT(qamsrc,"receive error: %s", err->message);
            g_clear_error (&err);
            TRACE;
            return GST_FLOW_ERROR;
        }
    }
    read = FALSE;
  }

  /* assign max_size with data read from socket */
  if (size > qamsrc->max_size)
    qamsrc->max_size = size;

  outbuf = gst_buffer_new ();

  TRACE;
  gst_buffer_append_memory (outbuf, qamsrc->mem);

  /* if the packet didn't fit into the first chunk, add second one as well */
  if (size > qamsrc->map.size) {
    TRACE;
    gst_buffer_append_memory (outbuf, qamsrc->mem_max);
    gst_memory_unmap (qamsrc->mem_max, &qamsrc->map_max);
    qamsrc->vec[1].buffer = NULL;
    qamsrc->vec[1].size = 0;
    qamsrc->mem_max = NULL;
  }
  TRACE;
  gst_memory_unmap (qamsrc->mem, &qamsrc->map);
  qamsrc->vec[0].buffer = NULL;
  qamsrc->vec[0].size = 0;
  qamsrc->mem = NULL;

  /* casting outbuf to buf */
  *buf = GST_BUFFER_CAST (outbuf);

  TRACE;
  return GST_FLOW_OK;
}
#endif
