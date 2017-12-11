#include "logpipe_in.h"

void InitConfig()
{
#if 0
	logpipe_conf	conf ;
	char		config_path_filename[ PATH_MAX + 1 ] ;
	char		*file_content = NULL ;
	
	int		nret = 0 ;
	
	DSCINIT_logpipe_conf( & conf );
	
	snprintf( conf.inputs[0].input , sizeof(conf.inputs[0].input)-1 , "file://%s/log" , getenv("HOME") );
	conf._inputs_count++;
	
	strcpy( conf.inputs[1].input , "tcp://127.0.0.1:10101" );
	conf._inputs_count++;
	
	snprintf( conf.outputs[0].output , sizeof(conf.outputs[0].output)-1 , "file://%s/log2" , getenv("HOME") );
	conf._outputs_count++;
	
	strcpy( conf.outputs[1].output , "tcp://127.0.0.1:10101" );
	conf._outputs_count++;
	
	conf.rotate.file_rotate_max_size = 0 ;
	
	strcpy( conf.comm.compress_algorithm , "deflate" );
	
	snprintf( conf.log.log_file , sizeof(conf.log.log_file)-1 , "%s/log3/logpipe.log" , getenv("HOME") );
	strcpy( conf.log.log_level , "ERROR" );
	
	nret = DSCSERIALIZE_JSON_DUP_logpipe_conf( & conf , "GB18030" , & file_content , NULL , NULL ) ;
	if( nret )
	{
		printf( "ERROR : DSCSERIALIZE_JSON_DUP_logpipe_conf failed[%d] , errno[%d]\n" , nret , errno );
		return;
	}
	
	memset( config_path_filename , 0x00 , sizeof(config_path_filename) );
	snprintf( config_path_filename , sizeof(config_path_filename)-1 , "%s/etc/logpipe.conf" , getenv("HOME") );
	nret = access( config_path_filename , F_OK ) ;
	if( nret == 0 )
	{
		printf( "ERROR : file[%s] exist\n" , config_path_filename );
		free( file_content );
		return;
	}
	
	nret = WriteEntireFile( config_path_filename , file_content , -1 ) ;
	free( file_content );
	if( nret )
	{
		printf( "ERROR : fopen[%s] failed[%d] , errno[%d]\n" , config_path_filename , nret , errno );
		return;
	}
#endif
	
	return;
}

#define LOGPIPE_CONFIG_LOG_LOGFILE	"/log/log_file"
#define LOGPIPE_CONFIG_LOG_LOGLEVEL	"/log/log_level"
#define LOGPIPE_CONFIG_INPUTS		"/inputs"
#define LOGPIPE_CONFIG_INPUTS_		"/inputs/"
#define LOGPIPE_CONFIG_OUTPUTS		"/outputs"
#define LOGPIPE_CONFIG_OUTPUTS_		"/outputs/"

static int ConvLogLevelStr( char *log_level_str , int log_level_str_len )
{
	if( log_level_str_len == 5 && STRNCMP( log_level_str , == , "DEBUG" , log_level_str_len ) )
		return LOGLEVEL_DEBUG;
	else if( log_level_str_len == 4 && STRNCMP( log_level_str , == , "INFO" , log_level_str_len ) )
		return LOGLEVEL_INFO ;
	else if( log_level_str_len == 4 && STRNCMP( log_level_str , == , "WARN" , log_level_str_len ) )
		return LOGLEVEL_WARN ;
	else if( log_level_str_len == 5 && STRNCMP( log_level_str , == , "ERROR" , log_level_str_len ) )
		return LOGLEVEL_ERROR ;
	else if( log_level_str_len == 5 && STRNCMP( log_level_str , == , "FATAL" , log_level_str_len ) )
		return LOGLEVEL_FATAL ;
	else
		return -1;
}

static int LoadLogpipeInputPlugin( struct LogpipeInputPlugin *p_logpipe_input_plugin )
{
	char		*p = NULL ;
	
	p = QueryPluginConfigItem( & plugin_config_items , "plugin" ) ;
	if( p == NULL )
	{
		ERRORLOG( "expect 'plugin' in 'inputs'" );
		return -1;
	}
	
	strncpy( p_logpipe_input_plugin->so_filename , p , sizeof(p_logpipe_input_plugin->so_filename)-1 );
	if( p_logpipe_input_plugin->so_filename[0] == '/' )
	{
		strcpy( p_logpipe_input_plugin->so_path_filename , p_logpipe_input_plugin->so_filename );
	}
	else
	{
		snprintf( p_logpipe_input_plugin->so_path_filename , sizeof(p_logpipe_input_plugin->so_path_filename)-1 , "%s/%s" , getenv("HOME") , p_logpipe_input_plugin->so_filename );
	}
	
	p_logpipe_input_plugin->so_handler = dlopen( p_logpipe_input_plugin->so_path_filename , RTLD_LAZY ) ;
	if( p_logpipe_input_plugin->so_handler == NULL )
	{
		ERRORLOG( "dlopen[%s] failed , errno[%d]" , p_logpipe_input_plugin->so_path_filename , errno );
		return -1;
	}
	
	p_logpipe_input_plugin->pfuncInitLogpipeInputPlugin = (funcInitLogpipeInputPlugin *)dlsym( "InitLogpipeInputPlugin" ) ;
	if( p_logpipe_input_plugin->pfuncInitLogpipeInputPlugin == NULL )
	{
		ERRORLOG( "dlsym[%s][InitLogpipeInputPlugin] failed , errno[%d]" , p_logpipe_input_plugin->so_path_filename , errno );
		return -1;
	}
	
	p_logpipe_input_plugin->pfuncBeforeReadLogpipeInput = (funcBeforeReadLogpipeInput *)dlsym( "BeforeReadLogpipeInput" ) ;
	if( p_logpipe_input_plugin->pfuncBeforeReadLogpipeInput == NULL )
	{
		ERRORLOG( "dlsym[%s][BeforeReadLogpipeInput] failed , errno[%d]" , p_logpipe_plugin->so_path_filename , errno );
		return -1;
	}
	
	p_logpipe_input_plugin->pfuncOnLogpipeInputEvent = (funcOnLogpipeInputEvent *)dlsym( "OnLogpipeInputEvent" ) ;
	if( p_logpipe_input_plugin->pfuncOnLogpipeInputEvent == NULL )
	{
		ERRORLOG( "dlsym[%s][OnLogpipeInputEvent] failed , errno[%d]" , p_logpipe_plugin->so_path_filename , errno );
		return -1;
	}
	
	p_logpipe_input_plugin->pfuncReadLogpipeInput = (funcReadLogpipeInput *)dlsym( "ReadLogpipeInput" ) ;
	if( p_logpipe_input_plugin->pfuncReadLogpipeInput == NULL )
	{
		ERRORLOG( "dlsym[%s][ReadLogpipeInput] failed , errno[%d]" , p_logpipe_plugin->so_path_filename , errno );
		return -1;
	}
	
	p_logpipe_input_plugin->pfuncAfterReadLogpipeInput = (funcAfterReadLogpipeInput *)dlsym( "AfterReadLogpipeInput" ) ;
	if( p_logpipe_input_plugin->pfuncAfterReadLogpipeInput == NULL )
	{
		ERRORLOG( "dlsym[%s][AfterReadLogpipeInput] failed , errno[%d]" , p_logpipe_plugin->so_path_filename , errno );
		return -1;
	}
	
	p_logpipe_input_plugin->pfuncCleanLogpipeInputPlugin = (funcCleanLogpipeInputPlugin *)dlsym( "CleanLogpipeInputPlugin" ) ;
	if( p_logpipe_input_plugin->pfuncCleanLogpipeInputPlugin == NULL )
	{
		ERRORLOG( "dlsym[%s][CleanLogpipeInputPlugin] failed , errno[%d]" , p_logpipe_plugin->so_path_filename , errno );
		return -1;
	}
	
	list_add_tail( & (p_logpipe_input_plugin->this_node) , & (p_env->logpipe_inputs_plugin_list.this_node) );
	
	return 0;
}

static int LoadLogpipeOutputPlugin( struct LogpipeOutputPlugin *p_logpipe_input_plugin )
{
	char		*p = NULL ;
	
	p = QueryPluginConfigItem( & plugin_config_items , "plugin" ) ;
	if( p == NULL )
	{
		ERRORLOG( "expect 'plugin' in 'outputs'" );
		return -1;
	}
	
	strncpy( p_logpipe_output_plugin->so_filename , p , sizeof(p_logpipe_output_plugin->so_filename)-1 );
	if( p_logpipe_output_plugin->so_filename[0] == '/' )
	{
		strcpy( p_logpipe_output_plugin->so_path_filename , p_logpipe_output_plugin->so_filename );
	}
	else
	{
		snprintf( p_logpipe_output_plugin->so_path_filename , sizeof(p_logpipe_output_plugin->so_path_filename)-1 , "%s/%s" , getenv("HOME") , p_logpipe_output_plugin->so_filename );
	}
	
	p_logpipe_output_plugin->so_handler = dlopen( p_logpipe_output_plugin->so_path_filename , RTLD_LAZY ) ;
	if( p_logpipe_output_plugin->so_handler == NULL )
	{
		ERRORLOG( "dlopen[%s] failed , errno[%d]" , p_logpipe_output_plugin->so_path_filename , errno );
		return -1;
	}
	
	p_logpipe_output_plugin->pfuncInitLogpipeOutputPlugin = (funcInitLogpipeOutputPlugin *)dlsym( "InitLogpipeOutputPlugin" ) ;
	if( p_logpipe_output_plugin->pfuncInitLogpipeOutputPlugin == NULL )
	{
		ERRORLOG( "dlsym[%s][InitLogpipeOutputPlugin] failed , errno[%d]" , p_logpipe_output_plugin->so_path_filename , errno );
		return -1;
	}
	
	p_logpipe_output_plugin->pfuncBeforeWriteLogpipeOutput = (funcBeforeWriteLogpipeOutput *)dlsym( "BeforeWriteLogpipeOutput" ) ;
	if( p_logpipe_output_plugin->pfuncBeforeWriteLogpipeOutput == NULL )
	{
		ERRORLOG( "dlsym[%s][BeforeWriteLogpipeOutput] failed , errno[%d]" , p_logpipe_plugin->so_path_filename , errno );
		return -1;
	}
	
	p_logpipe_output_plugin->pfuncWriteLogpipeOutput = (funcWriteLogpipeOutput *)dlsym( "WriteLogpipeOutput" ) ;
	if( p_logpipe_output_plugin->pfuncWriteLogpipeOutput == NULL )
	{
		ERRORLOG( "dlsym[%s][WriteLogpipeOutput] failed , errno[%d]" , p_logpipe_plugin->so_path_filename , errno );
		return -1;
	}
	
	p_logpipe_output_plugin->pfuncAfterWriteLogpipeOutput = (funcAfterWriteLogpipeOutput *)dlsym( "AfterWriteLogpipeOutput" ) ;
	if( p_logpipe_output_plugin->pfuncAfterWriteLogpipeOutput == NULL )
	{
		ERRORLOG( "dlsym[%s][AfterWriteLogpipeOutput] failed , errno[%d]" , p_logpipe_plugin->so_path_filename , errno );
		return -1;
	}
	
	p_logpipe_output_plugin->pfuncCleanLogpipeOutputPlugin = (funcCleanLogpipeOutputPlugin *)dlsym( "CleanLogpipeOutputPlugin" ) ;
	if( p_logpipe_output_plugin->pfuncCleanLogpipeOutputPlugin == NULL )
	{
		ERRORLOG( "dlsym[%s][CleanLogpipeOutputPlugin] failed , errno[%d]" , p_logpipe_plugin->so_path_filename , errno );
		return -1;
	}
	
	list_add_tail( & (p_logpipe_output_plugin->this_node) , & (p_env->logpipe_outputs_plugin_list.this_node) );
	
	return 0;
}

int CallbackOnJsonNode( int type , char *jpath , int jpath_len , int jpath_size , char *node , int node_len , char *content , int content_len , void *p )
{
	struct LogpipeEnv			*p_env = (struct LogpipeEnv *)p ;
	static struct LogpipeInputPlugin	*p_logpipe_input_plugin = NULL ;
	static struct LogpipeOutputPlugin	*p_logpipe_output_plugin = NULL ;
	
	if( (type&FASTERJSON_NODE_ENTER) && (type&FASTERJSON_NODE_BRANCH) )
	{
		if( jpath_len == sizeof(LOGPIPE_CONFIG_INPUTS)-1 && STRNCMP( jpath , == , LOGPIPE_CONFIG_INPUTS , jpath_len ) )
		{
			p_logpipe_input_plugin = (struct LogpipeInputPlugin *)malloc( sizeof(struct LogpipeInputPlugin) ) ;
			if( p_logpipe_input_plugin == NULL )
				return -1;
			memset( p_logpipe_input_plugin , 0x00 , sizeof(struct LogpipeInputPlugin) );
			
			INIT_LIST_HEAD( & (p_logpipe_input_plugin->plugin_config_items.this_node) );
		}
		else if( jpath_len == sizeof(LOGPIPE_CONFIG_OUTPUTS)-1 && STRNCMP( jpath , == , LOGPIPE_CONFIG_OUTPUTS , jpath_len ) )
		{
			p_logpipe_output_plugin = (struct LogpipeOutputPlugin *)malloc( sizeof(struct LogpipeOutputPlugin) ) ;
			if( p_logpipe_output_plugin == NULL )
				return -1;
			memset( p_logpipe_output_plugin , 0x00 , sizeof(struct LogpipeOutputPlugin) );
			
			INIT_LIST_HEAD( & (p_logpipe_output_plugin->plugin_config_items.this_node) );
		}
	}
	else if( (type&FASTERJSON_NODE_LEAVE) && (type&FASTERJSON_NODE_BRANCH) )
	{
		if( jpath_len == sizeof(LOGPIPE_CONFIG_INPUTS_)-1 && STRNCMP( jpath , == , LOGPIPE_CONFIG_INPUTS_ , jpath_len ) )
		{
			nret = LoadLogpipeInputPlugin( p_logpipe_input_plugin ) ;
			if( nret )
				return nret;
		}
		else if( jpath_len == sizeof(LOGPIPE_CONFIG_OUTPUTS_)-1 && STRNCMP( jpath , == , LOGPIPE_CONFIG_OUTPUTS_ , jpath_len ) )
		{
			nret = LoadLogpipeOutputPlugin( p_logpipe_output_plugin ) ;
			if( nret )
				return nret;
		}
	}
	else if( (type&FASTERJSON_NODE_LEAF) )
	{
		if( jpath_len == sizeof(LOGPIPE_CONFIG_LOG_LOGFILE)-1 && STRNCMP( jpath , == , LOGPIPE_CONFIG_LOG_LOGFILE , jpath_len ) )
		{
			strncpy( p_env->log_file , content , content_len );
		}
		else if( jpath_len == sizeof(LOGPIPE_CONFIG_LOG_LOGLEVEL)-1 && STRNCMP( jpath , == , LOGPIPE_CONFIG_LOG_LOGLEVEL , jpath_len ) )
		{
			p_env->log_level = ConvLogLevelA( content , content_len ) ;
			if( p_env->log_level == -1 )
			{
				ERRORLOG( "log_level[%.*s] invalid" , content_len , content );
				return -1;
			}
		}
		else if( jpath_len == sizeof(LOGPIPE_CONFIG_INPUTS_)-1 && STRNCMP( jpath , == , LOGPIPE_CONFIG_INPUTS_ , jpath_len ) )
		{
			nret = AddPluginConfigItem( & (p_logpipe_input_plugin->plugin_config_items) , node , node_len , content , content_len ) ;
			if( nret )
			{
				ERRORLOG( "AddPluginConfigItem [%.*s][%.*s] failed" , node_len , node , content_len , content );
				return -1;
			}
		}
		else if( jpath_len == sizeof(LOGPIPE_CONFIG_OUTPUTS_)-1 && STRNCMP( jpath , == , LOGPIPE_CONFIG_OUTPUTS_ , jpath_len ) )
		{
			nret = AddPluginConfigItem( & (p_logpipe_output_plugin->plugin_config_items) , node , node_len , content , content_len ) ;
			if( nret )
			{
				ERRORLOG( "AddPluginConfigItem [%.*s][%.*s] failed" , node_len , node , content_len , content );
				return -1;
			}
		}
	}
	
	return 0;
}

int LoadConfig( struct LogpipeEnv *p_env )
{
	char		*file_content = NULL ;
	int		file_len ;
	char		jpath[ 1024 + 1 ] ;
	
	
	int		nret = 0 ;
	
	file_content = StrdupEntireFile( p_env->config_path_filename , NULL ) ;
	if( file_content == NULL )
	{
		ERRORLOG( "open file[%s] failed , errno[%d]" , p_env->config_path_filename , errno );
		return -1;
	}
	
	g_fasterjson_encoding = FASTERJSON_ENCODING_GB18030 ;
	
	memset( jpath , 0x00 , sizeof(jpath) );
	nret = TravelJsonBuffer( file_content , jpath , sizeof(jpath) , & CallbackOnJsonNode , p_env ) ;
	if( nret )
	{
		ERRORLOG( "parse config[%s] failed" , p_env->config_path_filename );
		free( file_content );
		return -1;
	}
	
	free( file_content );
	
	return 0;
}

