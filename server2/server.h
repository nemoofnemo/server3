#ifndef SERVER_H_
#define SERVER_H_

#include "taskDataStructure.h"
#include "cmdFlag.h"
#define BUF_SIZE 1024000
#define v_type(X,Y) pair<pair<string,string>,pair<string,string>>((X),(Y))
#include <Python\Python.h>

class transferModule{
private:
	WSADATA wsaData;
	SOCKET sockSrv;	//服务器SOCKET
	SOCKET sockConn;	//连接服务器的客户端的SOCKET

	SOCKADDR_IN addrSrv;
	SOCKADDR_IN  addrClient;
public:
	//默认构造函数。默认服务器监听6000端口
	transferModule(const int & port = 6000 ){
		if ( WSAStartup( MAKEWORD( 2, 2 ), &wsaData ) != 0 ) {
			exit(EXIT_FAILURE);
		}

		if ( LOBYTE( wsaData.wVersion ) != 2 ||	HIBYTE( wsaData.wVersion ) != 2 ){
			WSACleanup( );
			exit(EXIT_FAILURE);
		}

		sockSrv = socket(AF_INET, SOCK_STREAM, 0);
		// 将INADDR_ANY转换为网络字节序，调用 htonl(long型)或htons(整型)
		addrSrv.sin_addr.S_un.S_addr = htonl(INADDR_ANY); 
		addrSrv.sin_family = AF_INET;
		addrSrv.sin_port = htons(port);

	}

	//析构函数。关闭socket
	~transferModule(){
		closesocket( sockSrv );
		closesocket( sockConn );
	}

	//检测包长度
	static bool checkLength( const char * data , const int length ){
		char temp[16] = {0};
		for( int i = 2 ; i < 10 ; ++ i ){
			temp[i-2] = data[i];
		}
		int val = atoi( temp );
		return val == length;
	}

	static int getLength( const char * data ){
		char temp[16] = {0};
		for( int i = 2 ; i < 10 ; ++ i ){
			temp[i-2] = data[i];
		}
		return atoi( temp );
	}

	//waring:this function will release all socket connections
	void releaseAllSocket(){
		WSACleanup();
	}

	//服务器绑定。需要在prepareListen之前调用。
	int startBind( void ){
		return bind(sockSrv, (SOCKADDR*)&addrSrv, sizeof(SOCKADDR));
	}

	//使服务器准备接受连接
	int prepareListen(const int num = 15){
		return listen( sockSrv , num );
	}

	//获取server的socket
	SOCKET getServerSocket( void ){
		return this->sockSrv;
	}

	//获取客户端连接socket。如果建立成功返回socket。否则返回INVALID_SOCKET
	SOCKET acceptClient( void ){
		int len = sizeof(SOCKADDR);
		sockConn = accept(sockSrv, (SOCKADDR*)&addrClient, &len);
		return sockConn ;
	}

	SOCKET getClientSocket( void ){
		return sockConn;
	}

	//获取客户端信息
	char * getClientIp( void ){
		return inet_ntoa(addrClient.sin_addr);
	}

	//关闭客户端socket
	int closeClient( void ){
		return closesocket( sockConn );
	}

	//send的第一个参数是客户端的socket
	//向客户端发送
	int sendData( char * data , int length){
		return send( sockConn , data , length , 0 );
	}

	//向客户端的监听线程发送数据//need test
	static int sendDataToListener( char * data , int length , string ip , int port){

		SOCKADDR_IN addr;
		addr.sin_addr.S_un.S_addr = inet_addr(ip.c_str());		
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);

		SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
		connect(sock, (SOCKADDR*)&addr, sizeof(SOCKADDR));
		int status = send( sock , data , length , 0 );

		closesocket(sock);
		return status;
	}

	//从客户端接收发给服务器的数据
	//recv函数的第一个参数是发送数据的socket。
	int recvData( char * dest , int maxLength ){
		int count = 0;
		int num = 0;
		char * pData = dest;

		char * buffer = ( char *)malloc( sizeof(char) * BUF_SIZE );
		while( (num = recv( sockConn , buffer , BUF_SIZE , 0 ) ) > 0 ){
			count += num;
			if( count < maxLength ){
				memcpy( pData , buffer , num );
				pData += num;
			}
			else{
				return SRV_ERROR;
			}
			num = 0;
		}

		free( buffer );
		return count;
	}

	static vector<char> pack( char cmd , char fmt , string mac , char * dataSection , int dataSectionLength){
		int length = ( dataSectionLength > 0 )?(32 + dataSectionLength):32;
		char * temp = (char * )malloc( sizeof(char) * length );
		memset( temp , '0' , sizeof(char) * length );
		temp[0] = cmd;
		temp[1] = fmt;
		sprintf(temp + 2 ,"%08d", length );
		sprintf(temp + 10,"%012s" , mac.c_str() );
		temp[22] = '0';
		memcpy(temp + 32 , dataSection , dataSectionLength );
		vector<char> ret( temp , temp + length );
		free(temp);
		return ret;
	}
};

bool callPy( bool flag , int arg1 , int arg2 , double arg3 , int * a , int * b , double * c ){
	Py_Initialize();
	PyObject * pModule = NULL;
	PyObject * pFunc = NULL; 
	PyObject * pRet = NULL;
	PyObject * ptr = NULL;
	
	pModule =PyImport_ImportModule("readMac");
	if( pModule == NULL ){
		Log("[python]:importModule error\n");
		return false;
	}
	
	pFunc= PyObject_GetAttrString(pModule, "split");
	if( pFunc == NULL ){
		Log("[python]:getAttrString error\n");
		return false;
	}

	PyObject *pArgs = PyTuple_New(3);
	PyTuple_SetItem( pArgs , 0 , Py_BuildValue("i", arg1) );
	PyTuple_SetItem( pArgs , 1 , Py_BuildValue("i", arg2) );
	PyTuple_SetItem( pArgs , 2 , Py_BuildValue("d", arg3) );

	//call
	if(flag == true ){
		pRet = PyEval_CallObject(pFunc, pArgs);
		* a = -1;
		* b = -1;
		* c = -1.0;
		Py_Finalize();
		return true;
	}
	else{
		pRet = PyEval_CallObject(pFunc, NULL);
	}

	ptr = PyList_GET_ITEM( pRet , 0);
	PyArg_Parse(ptr , "i" , a);

	ptr = PyList_GET_ITEM( pRet , 1);
	PyArg_Parse( ptr , "i" , b);
	
	ptr = PyList_GET_ITEM( pRet , 2);
	PyArg_Parse( ptr , "d" , c);
	Log("[python]:location=%d,%d,%lf\n" , *a,*b,*c );

	Py_Finalize();
	return true;
}

char * stData;
int stLength;
CRITICAL_SECTION stcs;

//全局变量taksManager，负责管理任务
TaskManager taskManager;	//warning:one program,one Taskmanager Obeject!!!
transferModule listener;

vector< pair<pair<string,string>,pair<string,string> > > mac_ip;
CRITICAL_SECTION mics;

vector<vector<string> > mac_key;
CRITICAL_SECTION mkcs;

unsigned int __stdcall taskThread( LPVOID lpArg ){
	char * data = (char*)lpArg;
	char cmdFlag = *data;
	char formatFlag = *(data+1);
	char * dataSection = data;
	int dataSectionLen = 0;
	int length = listener.getLength(data);

	string taskMac( data + 10 , data + 22 );
	string taskIP( data + length , data + length + 16 );
	string targetMac;
	string wardMac;
	string key;
	string targetIP("192.168.23.3");
	string wardIP("192.168.23.3");
	data[length] = '\0';
	FILE * fp;
	//python return
	int a = 0;
	int b = 0;
	double c = 0;
	char str[128];
	int t_l  = 0;

	if( data != NULL){
		dataSection += 32;
		dataSectionLen = length - 32;
	}

	vector<string> r_vec;
	int r_temp = -1;
	char r_flag = -1;
	bool r_run = false;

	int loc = -1;
	int pos = -1;
	for( int i = 0 ; i < (int)mac_ip.size() ; ++ i ){
		if(mac_ip[i].first.first == taskMac  ){
			pos = i;
			loc = 1;
			break;
		}
		if( mac_ip[i].second.first == taskMac ){
			pos = i;
			loc = 2;
			break;
		}
	}

	EnterCriticalSection( &mics );
	if( loc == 1 ){
		targetMac = mac_ip[pos].second.first;
		targetIP = mac_ip[pos].second.second;
		wardMac = taskMac;
		wardIP = taskIP;
		if( mac_ip[pos].first.second != taskIP ){
			mac_ip[pos].first.second = taskIP;
		}
	}else if(loc == 2){
		targetMac = taskMac;
		targetIP = taskIP;
		wardMac = mac_ip[pos].first.first;
		wardIP = mac_ip[pos].first.second ;
		if( mac_ip[pos].second.second != taskIP ){
			mac_ip[pos].second.second = taskIP;
		}
	}
	LeaveCriticalSection( &mics );

	Log("[server]:task thread start,cmd=%c.\n",cmdFlag);
	switch(cmdFlag){
	case TAKEPIC:
		if( pos != -1  && targetIP != "NULL")
			listener.sendDataToListener( data , length , targetIP  ,6002);
		break;
	case SENDPIC:
		if( pos != -1 && wardIP != "NULL" )
			listener.sendDataToListener( data , length , wardIP , 6002 );
		break;
	case GETLOCATION:
		if( length == 32 && targetIP != "NULL")
			if( pos != -1)
				listener.sendDataToListener( data , length , targetIP , 6002 );
		break;
	case SENDLOCATION:
		if( pos != -1 ){
			EnterCriticalSection( &stcs );
			
			if( (fp = fopen("String.txt" , "w+" )) != NULL ) {
				fprintf( fp , "%s" , dataSection );
				fclose(fp);
				stLength = 0;
				//call python
				if( callPy( false , 0 , 0 , 0 , &a , &b , &c ) ){
					sprintf( str , "B0%08d%12s0000000000%d %d %lf" , 0 , wardMac.c_str() , a , b , c );
					t_l = strlen( str );
					sprintf( stData , "%8d" , t_l );
					memcpy( str + 2 , stData , 8 );
					listener.sendDataToListener( str , t_l , wardIP , 6002 );
				}
			}
			LeaveCriticalSection( &stcs );
		}
		break;
	case WIFI_SIG:
		if( stLength > 12 * BUF_SIZE )
			stLength = 0;

		 if( length > 32 ){//need test
			//get wifi.write data to stdata and modify stLength
			//enter cs
			EnterCriticalSection( &stcs );
			Log( "[server]" );
			Log( dataSection );
			memcpy( stData + stLength , dataSection , dataSectionLen );
			stLength += dataSectionLen;
			LeaveCriticalSection( &stcs );
		}
		break;
	case GPS_SIG:
		listener.sendDataToListener( data , length , targetIP , 6002 );
		break;
	case SRV_QUIT://debug
		Log("[server]:quit.\n");
		exit(0);
		break;
	case DUMP:
		Log("[server]:dump.\n");
		for( int i = 0 ; i < dataSectionLen ; ++ i ){
			putchar(*(dataSection + i ));
		}
		putchar('\n');
		++ log_dump;
		break;
	case REGISTER:
		if( length == 32 ){
			/*if( pos != -1 ){
				if( loc == 1 ){
					mac_ip[pos].first.second = taskIP;
				}
				else if( loc == 2 ){
					mac_ip[pos].second.second = taskIP;
				}
			}*/
		}
		else{
			r_flag = * dataSection;
			key = string( dataSection + 1 , dataSection + dataSectionLen );
			for( int i = 0 ; i < mac_key.size() ; ++ i ){
				if( mac_key[i][0] == taskMac ){//mac
					EnterCriticalSection( & mkcs );
					mac_key[i][1] = key;//key
					mac_key[i][2] = r_flag;//flag:0 ward,1 target
					mac_key[i][3] = taskIP;//last ip
					r_run = true;
					LeaveCriticalSection ( & mkcs );
					break;
				}

				if( mac_key[i][1] == key ){
					r_temp = i;
					break;
				}
			}

			EnterCriticalSection( & mkcs );
			if( r_temp != -1 ){
				if( mac_key[r_temp][2].c_str()[0] != r_flag ){
					EnterCriticalSection( &mics );
					if( r_flag == '0' ){
						mac_ip.push_back(v_type((pair<string,string>(taskMac,taskIP)),(pair<string,string>(mac_key[r_temp][0],mac_key[r_temp][3]))));
					}
					else if(r_flag == '1'){
						mac_ip.push_back(v_type((pair<string,string>(mac_key[r_temp][0],mac_key[r_temp][3])),(pair<string,string>(taskMac,taskIP))));
					}
					LeaveCriticalSection ( &mics );
				}
			}
			else if(r_run == false){//==-1
				r_vec.push_back( taskMac );
				r_vec.push_back( key );
				r_vec.push_back( string( dataSection , dataSection + 1 ));
				r_vec.push_back( taskIP );
				mac_key.push_back( r_vec );
			}
			LeaveCriticalSection( &mkcs );
		}
		break;
	default:
		Log("[server]:Invalid CMD flag = %x\n",cmdFlag);
		log_invalid ++;
		//return error message
		for( int i = 0 ; i < dataSectionLen ; ++ i ){
			putchar(*(dataSection + i ));
		}

		putchar('\n');
		break;
	}

	free( data );
	++ log_use;
	Log("[server]:connection=%d,recv=%d,throw=%d,use=%d,dump=%d,invalid=%d\n" , log_connect , log_recv , log_throw , log_use , log_dump,log_invalid);	

	return 0;
}

unsigned int __stdcall listenThread( LPVOID lpArg ){
	WSADATA wsaData;
	SOCKET sockSrv;	//服务器SOCKET
	SOCKET sockClient;
	SOCKADDR_IN addrSrv;
	SOCKADDR_IN addrClient;
	string ip;

	int arg = (int)lpArg;
	int len = sizeof(SOCKADDR);
	int count = 0;
	int num = 0;
	int limit = 2 * BUF_SIZE;

	char * data = NULL;
	char * pData = data;
	char * buffer = NULL;

	if ( WSAStartup( MAKEWORD( 2, 2 ), &wsaData ) != 0 ) {
		exit(EXIT_FAILURE);
	}

	if ( LOBYTE( wsaData.wVersion ) != 2 ||	HIBYTE( wsaData.wVersion ) != 2 ){
		WSACleanup( );
		exit(EXIT_FAILURE);
	}

	sockSrv = socket(AF_INET, SOCK_STREAM, 0);
	// 将INADDR_ANY转换为网络字节序，调用 htonl(long型)或htons(整型)
	addrSrv.sin_addr.S_un.S_addr = htonl(INADDR_ANY); 
	addrSrv.sin_family = AF_INET;
	addrSrv.sin_port = htons(6000);

	int opt=1;
	setsockopt(sockSrv,SOL_SOCKET,SO_REUSEADDR,(char *)&opt,sizeof(opt));
	bind( sockSrv , (SOCKADDR*)&addrSrv, sizeof(SOCKADDR));
	listen( sockSrv , 128);

	while( true ){
		sockClient = accept(sockSrv, (SOCKADDR*)&addrClient, &len);
		log_connect ++;

		if( sockClient != INVALID_SOCKET ){
			num = 0;
			count = 0;
			data =  ( char *)malloc( sizeof(char) * limit + 20);
			buffer = ( char *)malloc( sizeof(char) * BUF_SIZE );
			pData = data;

			while( (num = recv( sockClient , buffer , BUF_SIZE , 0 ) ) > 0 ){
				count += num;
				if( count < limit ){
					memcpy( pData , buffer , num );
					pData += num;
				}
				else{
					break;
				}
				num = 0;
			}

			free(buffer);
			++ log_recv;
			Log("[server]:listen thread=%d,recv data = %d\n" , arg ,count);

			if( count < 32 || listener.checkLength(data,count) == false){
				closesocket( sockClient );
				Log("[server]:length error\n");
				continue;
			}

			Log("[server]:start new task thread.\n");
			char * ip = inet_ntoa(addrClient.sin_addr);
			memcpy( data + count , ip , strlen( ip ) + 1 );
			HANDLE h = (HANDLE)_beginthreadex( NULL , 0 , taskThread , data , 0 , NULL );
			CloseHandle( h );
			closesocket( sockClient );
		}
	}

	return 0;
}

#endif // !SERVER_H_
