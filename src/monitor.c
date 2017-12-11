#include "logpipe_in.h"

static sig_atomic_t		g_SIGTERM_flag = 0 ;

static void sig_set_flag( int sig_no )
{
	/* 接收到不同信号设置不同的全局标志，延后到主流程中处理 */
	if( sig_no == SIGTERM )
	{
		g_SIGTERM_flag = 1 ; /* 退出 */
	}
	
	return;
}

int monitor( struct LogpipeEnv *p_env )
{
	struct sigaction	act ;
	
	pid_t			pid , pid2 ;
	int			status ;
	
	SetLogFile( p_env->conf.log.log_file );
	SetLogLevel( p_env->log_level );
	
	/* 设置信号 */
	if( ! p_env->no_daemon )
	{
		signal( SIGINT , SIG_IGN );
	}
	signal( SIGCLD , SIG_DFL );
	signal( SIGCHLD , SIG_DFL );
	signal( SIGPIPE , SIG_IGN );
	act.sa_handler = & sig_set_flag ;
	sigemptyset( & (act.sa_mask) );
	act.sa_flags = 0 ;
	sigaction( SIGTERM , & act , NULL );
	act.sa_flags = SA_RESTART ;
	signal( SIGCLD , SIG_DFL );
	
	p_env->is_monitor = 1 ;
	while(1)
	{
		/* 创建工作进程 */
		pid = fork() ;
		if( pid == -1 )
		{
			ERRORLOG( "fork failed , errno[%d]" , errno )
			return -1;
		}
		else if( pid == 0 )
		{
			INFOLOG( "child : [%ld] fork [%ld]" , getppid() , getpid() )
			p_env->is_monitor = 0 ;
			return -worker( p_env );
		}
		else
		{
			INFOLOG( "parent : [%ld] fork [%ld]" , getpid() , pid )
		}
		
_GOTO_WAITPID :
		
		/* 堵塞等待工作进程结束 */
		DEBUGLOG( "waitpid ..." )
		pid2 = waitpid( pid , & status , 0 );
		if( pid2 == -1 )
		{
			if( errno == EINTR )
			{
				/* 如果被退出信号中断，退出 */
				if( g_SIGTERM_flag )
				{
					break;
				}
				else
				{
					goto _GOTO_WAITPID;
				}
			}
			
			ERRORLOG( "waitpid failed , errno[%d]" , errno )
			return -1;
		}
		else if( pid2 != pid )
		{
			FATALLOG( "unexpect other child[%d]" , pid2 )
		}
		
		/* 检查工作进程是否正常结束 */
		if( WEXITSTATUS(status) == 0 && WIFSIGNALED(status) == 0 && WTERMSIG(status) == 0 )
		{
			INFOLOG( "waitpid[%d] WEXITSTATUS[%d] WIFSIGNALED[%d] WTERMSIG[%d]" , pid , WEXITSTATUS(status) , WIFSIGNALED(status) , WTERMSIG(status) )
		}
		else
		{
			ERRORLOG( "waitpid[%d] WEXITSTATUS[%d] WIFSIGNALED[%d] WTERMSIG[%d]" , pid , WEXITSTATUS(status) , WIFSIGNALED(status) , WTERMSIG(status) )
		}
		
		sleep(1);
		
		/* 重新创建命令管道，创建工作进程 */
	}
	
	/* 杀死子进程 */
	kill( pid , SIGTERM );
	
	/* 关闭事件总线 */
	close( p_env->epoll_fd );
	
	return 0;
}

int _monitor( void *pv )
{
	return monitor( (struct LogpipeEnv*)pv );
}

