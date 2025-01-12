/*
 Test and demo program for Libnodave, a free communication libray for Siemens S7.
 
 **********************************************************************
 * WARNING: This and other test programs overwrite data in your PLC.  *
 * DO NOT use it on PLC's when anything is connected to their outputs.*
 * This is alpha software. Use entirely on your own risk.             * 
 **********************************************************************
 
 (C) Thomas Hergenhahn (thomas.hergenhahn@web.de) 2002, 2003.

 This is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2, or (at your option)
 any later version.

 This is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with Libnodave; see the file COPYING.  If not, write to
 the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
//#include "nodavesimple.h"
#include "nodave.h"
#include "setport.h"
//#include "MQTTClient.h"
#include "MQTTAsync.h"

#include <errno.h> 
#include <sys/types.h> 
#include <netinet/in.h> 
#include <sys/socket.h> 
#include <sys/wait.h> 
#include <fcntl.h> /* Added for the nonblocking socket */

#define ADDRESS     "ssl://194.218.40.18:8883"
#define CLIENTID    "S7-200_Reader"
#define TOPICT       "sensors/vaxthus/plc/temp"
#define TOPICH       "sensors/vaxthus/plc/humid"
#define QOS         1
#define TIMEOUT     10000L
#define USERNAME    "wthr"
#define PASSWORD    "FsZ2HZq6chkQGkLnZQ"
#define KEYFILE     "/usr/local/harvest/cert/wthr.crt"
#define PRIVFILE    "/usr/local/harvest/cert/wthr.key"
#define CAFILE      "/usr/local/harvest/cert/ca.crt"

#define MYPORT 3456    /* the port users will be connecting to */
#define BACKLOG 10     /* how many pending connections queue will hold */


#ifdef LINUX
#include <unistd.h>
#include <sys/time.h>
#include <fcntl.h>
#define UNIX_STYLE
#endif

#ifdef BCCWIN
#include <time.h>
    void usage(void);
    void wait(void);
#define WIN_STYLE    
#endif

void usage()
{
    printf("Usage: testPPI [-d] [-w] serial port.\n");
    printf("-w will try to write to Flag words. It will overwrite FB0 to FB15 (MB0 to MB15) !\n");
    printf("-d will produce a lot of debug messages.\n");
    printf("-b will run benchmarks. Specify -b and -w to run write benchmarks.\n");
    printf("-m will run a test for multiple variable reads.\n");
    printf("-c will write 0 to the PLC memory used in write tests.\n");
    printf("-s stops the PLC.\n");
    printf("-r tries to put the PLC in run mode.\n");
    printf("--readout read program and data blocks from PLC.\n");
    printf("--ppi=<number> will use number as the PPI adddres of the PLC. Default is 2.\n");
    printf("--mpi=<number> will use number as the PPI adddres of the PLC.\n");
    printf("--local=<number> will set the local PPI adddres to number. Default is 0.\n");
    printf("--debug=<number> will set daveDebug to number.\n");
#ifdef UNIX_STYLE    
    printf("Example: testPPI -w /dev/ttyS0\n");
#endif    
#ifdef WIN_STYLE    
    printf("Example: testPPI -w COM1\n");
#endif    
}

void kwait() {
    uc c;
    printf("Press return to continue.\n");
#ifdef UNIX_STYLE        
    read(0,&c,1);
#endif
}    

void loadBlocksOfType(daveConnection * dc, int blockType) {
    int i,j,uploadID, len, more;
#ifdef UNIX_STYLE
    int fd;
#endif	        
#ifdef WIN_STYLE	    
    HANDLE fd;
    unsigned long res;
#endif	        
    char blockName [20];
    uc blockBuffer[20000],*bb;
    daveBlockEntry dbe[256];   
    j=daveListBlocksOfType(dc, blockType, dbe);
    if (j<0) {
	printf("error %d = %s\n",-j,daveStrerror(-j));
	return;
    }
    printf("%d blocks of type %s\n",j,daveBlockName(blockType));
    for (i=0; i<j; i++) {
	printf("%s%d  %d %d\n",
	    daveBlockName(blockType),
	    dbe[i].number, dbe[i].type[0],dbe[i].type[1]);	
	bb=blockBuffer;
	len=0;
	if (0==initUpload(dc, blockType, dbe[i].number, &uploadID)) {
    	    do {
		doUpload(dc,&more,&bb,&len,uploadID);
	    } while (more);
	    sprintf(blockName,"%s%d.mc7",daveBlockName(blockType), dbe[i].number);	
#ifdef UNIX_STYLE	    
    	    fd=open(blockName,O_RDWR|O_CREAT|O_TRUNC,0644);
    	    write(fd, blockBuffer, len);
    	    close(fd);
#endif	    
#ifdef WIN_STYLE
	    fd = CreateFile(blockName,
    	      GENERIC_WRITE, 0, 0, 2,
    		FILE_FLAG_WRITE_THROUGH, 0);
    	    WriteFile(fd, blockBuffer, len, &res, NULL);
    	    CloseHandle(fd);
#endif	    
    	    endUpload(dc,uploadID);
	}    
    }
}


//#include "benchmark.c"

void singleThread_send(MQTTAsync* c, int qos, char* topic, char *value)
{
//	MQTTAsync_deliveryToken dt;
	MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
	int rc;

	pubmsg.payload = value;
	pubmsg.payloadlen = strlen(value);
	pubmsg.qos = qos;
	pubmsg.retained = 0;

	rc =  MQTTAsync_sendMessage(c, topic, &pubmsg, NULL);
	if (rc == MQTTASYNC_SUCCESS)
        {
                printf("Good rc from publish rc was: %d", rc);
        }
	else
{
	printf("Err RC: %d", rc);
}

//	if (qos > 0)
//	{
//		rc = MQTTClient_waitForCompletion(c, dt, 20000L);
//                if (rc == MQTTCLIENT_SUCCESS)
//                {
//		    printf("Good rc from waitforCompletion rc was %d", rc);
//                }
//	}
}

void delivered(void *context, MQTTAsync_token dt)
{
    printf("Message with token value %d delivery confirmed\n", dt);
}

void connlost(void *context, char *cause)
{
    printf("\nConnection lost\n");
    printf("     cause: %s\n", cause);
}


int main(int argc, char **argv) {
    int next,adrPos,res, localPPI, plcPPI, a;
    float plcTemp, plcHumid;
    int plcVatid;
    float plcVatmr;
    int waterflag;
    unsigned char  plcLarm, tempvar, plcVaStrb;
    char outbuf[12];
    daveInterface * di;
    daveConnection * dc;
    daveResultSet rs;
    _daveOSserialType fds;
    PDU p;
    
    adrPos=1;
    localPPI=0;
    plcPPI=2;
    
    if (argc<2) {
	usage();
	exit(-1);
    }    

    while (argv[adrPos][0]=='-') {
	if (strcmp(argv[adrPos],"-s")==0) {
	} else
	if (strcmp(argv[adrPos],"-r")==0) {
	} else
	if (strncmp(argv[adrPos],"--local=",8)==0) {
	    localPPI=atol(argv[adrPos]+8);
	    printf("setting local PPI address to:%d\n",localPPI);
	} else
	if (strncmp(argv[adrPos],"--mpi=",6)==0) {
	    plcPPI=atol(argv[adrPos]+6);
	    printf("setting PPI address of PLC to:%d\n",plcPPI);
	} else
	if (strncmp(argv[adrPos],"--ppi=",6)==0) {
	    plcPPI=atol(argv[adrPos]+6);
	    printf("setting PPI address of PLC to:%d\n",plcPPI);
	} else
	if (strncmp(argv[adrPos],"--readout",9)==0) {
	} else
	if (strcmp(argv[adrPos],"-d")==0) {
	    daveSetDebug(daveDebugAll);
	} 
	else if (strcmp(argv[adrPos],"-w")==0) {
	} 
	else if (strcmp(argv[adrPos],"-b")==0) {
	} 
	else if (strcmp(argv[adrPos],"-m")==0) {
	}
	else if (strcmp(argv[adrPos],"-c")==0) {
	}
	else if (strcmp(argv[adrPos],"-n")==0) {
	}
	else if (strcmp(argv[adrPos],"-e")==0) {
	}    
	adrPos++;
	if (argc<=adrPos) {
	    usage();
	    exit(-1);
	}	
    }    
    
    MQTTAsync client;
    MQTTAsync_SSLOptions ssl_opts = MQTTAsync_SSLOptions_initializer;
    MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
    MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
    MQTTAsync_token token;
    int rc;

    MQTTAsync_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;
    conn_opts.username = USERNAME;
    conn_opts.password = PASSWORD;
    conn_opts.ssl=&ssl_opts;
    conn_opts.ssl->enableServerCertAuth = 0;
    conn_opts.ssl->trustStore = CAFILE;
    conn_opts.ssl->keyStore = KEYFILE;
    conn_opts.ssl->privateKeyPassword = NULL;
    conn_opts.ssl->privateKey = PRIVFILE;
    conn_opts.ssl->sslVersion = MQTT_SSL_VERSION_TLS_1_0;

    MQTTAsync_setCallbacks(client, NULL, connlost, NULL, delivered);

    if ((rc = MQTTAsync_connect(client, &conn_opts)) != MQTTASYNC_SUCCESS)
    {
        printf("Failed to connect, return code %d\n", rc);
        exit(-1);
    }



    fds.rfd=setPort(argv[adrPos],"9600",'E');
    fds.wfd=fds.rfd;
    if (fds.rfd>0) { 
	di =daveNewInterface(fds, "IF1", localPPI, daveProtoPPI, daveSpeed187k);
	dc =daveNewConnection(di, plcPPI, 0, 0);  
	daveConnectPLC(dc);
//
// just try out what else might be readable in an S7-200 (on your own risk!):
//
/*
	for (i=0;i<=255;i++) {
	    printf("Trying to read 8 bytes from area %d.\n",i);
	    wait();
	    res=daveReadBytes(dc,i,0,0,10,NULL);
	    printf("Function result: %d %s\n",res,daveStrerror(res));
	    _daveDump("Data",dc->resultPointer,dc->AnswLen);
	}    
*/	    
//	printf("Trying to read 64 bytes (32 words) from data block 1.\n This is V memory of the 200.\n");
//	wait();
// 	    res=daveReadBytes(dc, AREA, area_Number, start_address, length, buffer);
//          res=daveWriteBytes(dc, AREA, area_Number, start_address, length, buffer);

// Open socket for receiving commands
	#define UDPMAXLEN	16
        int 			sockfd;  /* listen on sock_fd, new connection on new_fd */
        fd_set new_fds;
        struct 	sockaddr_in 	my_addr;    /* my address information */
        struct 	sockaddr_in 	cli_addr; /* connector's address information */
        socklen_t clilen = sizeof(cli_addr);
        int 			sin_size;
	char			string_read[UDPMAXLEN];
	int 			n,i;
	struct timeval		tmo;
printf("Socket... ");
        if ((sockfd = socket(AF_INET, SOCK_DGRAM|SOCK_NONBLOCK, 0)) == -1) {
            perror("socket");
            exit(1);
        }
	
        my_addr.sin_family = AF_INET;         /* host byte order */
        my_addr.sin_port = htons(MYPORT);     /* short, network byte order */
        my_addr.sin_addr.s_addr = INADDR_ANY; /* auto-fill with my IP */
        bzero(&(my_addr.sin_zero), 8);        /* zero the rest of the struct */
        if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) \
                                                                      == -1) {
            perror("Error bind");
            exit(1);
        }
	printf("Bound to port %d\n", MYPORT);


printf("Dave\n");
daveSetDebug(daveDebugAll);
	davePrepareReadRequest(dc, &p);
	daveAddVarToReadRequest(&p,daveDB,1,100,10);
	daveAddVarToReadRequest(&p,daveFlags,0,19,4);
	daveAddVarToReadRequest(&p,daveTimer200,0,37,4);
	while(1) {

		FD_ZERO(&new_fds);
		FD_SET(sockfd, &new_fds);
	        tmo.tv_sec=0;
	        tmo.tv_usec=0;

		int n = select(sockfd+1, &new_fds, 0, 0, &tmo);
		if(n < 0)
		{
			perror("ERROR Server : select()\n");
			close(sockfd);
			exit(1);
		}
		if(FD_ISSET(sockfd, &new_fds)) {
			printf("Command received\n");
			sin_size = recvfrom(sockfd, string_read, UDPMAXLEN, 0, (struct sockaddr*)&cli_addr, &clilen);
			if (sin_size < 0){
				perror("ERROR in recvfrom()");
				close(sockfd);
				exit(1);
			}
                        else if (sin_size>0)
			{
				if (strstr(string_read,"VA")!=0)
				{
					waterflag=1;
				}
			}
			//j = ntohs(cli_addr.sin_port);
			FD_CLR(sockfd, &new_fds);
		}

		if (waterflag==1)
		{
			printf("Water command received!\n");
          		a=1;
			res=daveWriteBytes(dc, daveDB, 1, 110, 1, &a);

                        if (res==0) {
                                waterflag=0;
                                printf("Sent to PLC\n");
                                next=10;
                        }
                        else
                        {
                                printf("WriteBit: %d\n",res);
				printf("%s\n", daveStrerror(res));
//                                next=1;
                        }


		}


		if (plcVaStrb&0x01)
                {
                        printf("Reseting water strobe!\n");
                        a=0;
                        res=daveWriteBytes(dc, daveDB, 1, 110, 1, &a);

                        if (res==0) {
                                plcVaStrb=0;
                                printf("Sent to PLC\n");
                                next=10;
                        }
                        else
                        {
                                printf("WriteBit: %d\n",res);
                                printf("%s\n", daveStrerror(res));
//                                next=1;
                        }
		}

		sleep(1);
		next--;
//		MQTTClient_yield();
		if (next<=0)
		{
// Please see doc/readmultiple.html
//		        res=daveReadBytes(dc,daveDB,1,100,10,NULL); // was len=64
		        davePrepareReadRequest(dc, &p);
        		daveAddVarToReadRequest(&p,daveDB,1,100,11);
        		daveAddVarToReadRequest(&p,daveFlags,0,19,4);
        		daveAddVarToReadRequest(&p,daveTimer200,0,37,4);

			res=daveExecReadRequest(dc, &p, &rs);
_daveDump("Data",dc->resultPointer,dc->AnswLen);
			if (res==0) {

				res=daveUseResult(dc, &rs, 0); // first result
				if (res==0) {
		                    next=60;
//				    a=daveGetU16(dc);
		       	     	    plcTemp=daveGetFloat(dc); // VD100, temperature
	                            plcHumid=daveGetFloat(dc)*10/3; // VD104, hudidity. Scale error correction 
	                            plcVatid=daveGetU16(dc);
                                    plcVaStrb=daveGetU8(dc); 
//		                    plcLarm=daveGetU8(dc);
//	                            tempvar=daveGetU8(dc);
//				    tempvar=daveGetU8(dc);
//      			    tempvar=daveGetU8(dc);
//				    plcVatmr=daveGetS16(dc);
//	                            printf("VaTid: %d\n", plcVatmr);
//				    printf("Larm: %x\n", plcLarm);

			            snprintf(outbuf, sizeof(outbuf), "%5.3f", plcTemp);
				    printf("VD100: %s VD104: %f VW108: %d\n",outbuf, plcHumid, plcVatid);
				    singleThread_send(client, 1, TOPICT, outbuf);
	 			    snprintf(outbuf, sizeof(outbuf), "%5.3f", plcHumid);
	                            singleThread_send(client, 1, TOPICH, outbuf);

//			    a=daveGetU16(dc);
//           		    d=daveGetFloat(dc);
//	    	    	printf("VD102: %d\n...\n",d);
				}
	        		else
				{
					printf("DaveConnect: %d\n",res);
                                	printf("%s\n",daveStrerror(res));
					next=1;
				}
//				sleep(1);
//                        	res=daveReadBytes(dc,daveFlags,0,19,4,NULL); //M19-22, M19=Flag, M20=larmminne, M21=reset, M22=start
				res=daveUseResult(dc, &rs, 1); // 2nd result
				_daveDump("Data",dc->resultPointer,dc->AnswLen);
                        	if (res==0) {
                           		plcLarm=daveGetU8(dc);
                           		printf("Larm: %x\n", plcLarm);
                        	}
                        	else
                        	{
                                	printf("DaveConnect2: %d\n",res);
					printf("%s\n",daveStrerror(res));
                                	next=1;
                        	}
//				sleep(1);
//                       	res=daveReadBytes(dc,daveTimer200,0,37,2,NULL); //T38: timer vatten (eller 37?)
				res=daveUseResult(dc, &rs, 2); //3rd result
				_daveDump("Data",dc->resultPointer,dc->AnswLen);

                        	if (res==0) {
                           		plcVatmr=daveGetIECSeconds(dc);
                        	   	printf("VaTid: %0.3f\n", plcVatmr);
                        	}
                        	else
                        	{
                               		printf("DaveConnect3: %d\n",res);
					printf("%s\n",daveStrerror(res));

                                	next=1;
                        	}
			daveFreeResults(&rs);
			}
			else
			{
                          	printf("DaveConnect3: %d\n",res);
                           	printf("%s\n",daveStrerror(res));

                         	next=1;

			} 
//*/
		}
	}
//	a=daveGetU16at(dc,62);
//	printf("DB1:DW32: %d\n",a);
	
//	printf("Trying to read 16 bytes from FW0.\n");
//	wait();
/*
 * Some comments about daveReadBytes():
 *
 * The 200 family PLCs have the V area. This is accessed like a datablock with number 1.
 * This is not a quirk or convention introduced by libnodave, but the command transmitted 
 * to the PLC is exactly the same that would read from DB1 of a 300 or 400.
 *
 * to read VD68 and VD72 use:
 * 	daveReadBytes(dc, daveDB, 1, 68, 6, NULL);
 * to read VD68 and VD72 into your applications buffer appBuffer use:	
 * 	daveReadBytes(dc, daveDB, 1, 68, 6, appBuffer);
 * to read VD68 and VD78 into your applications buffer appBuffer use:	
 * 	daveReadBytes(dc, daveDB, 1, 68, 14, appBuffer);
 * this reads DBD68 and DBD78 and everything in between and fills the range
 * appBuffer+4 to appBuffer+9 with unwanted bytes, but is much faster than:
 *	daveReadBytes(dc, daveDB, 1, 68, 4, appBuffer);
 *	daveReadBytes(dc, daveDB, 1, 78, 4, appBuffer+4);
 */	
//	res=daveReadBytes(dc,daveFlags,0,0,16,NULL);
//	if (res==0) {
/*
 *	daveGetU32(dc); reads a word (2 bytes) from the current buffer position and increments
 *	an internal pointer by 2, so next daveGetXXX() wil read from the new position behind that
 *	word.	
 */	
 //   	    a=daveGetU32(dc);
 //           b=daveGetU32(dc);
//	    c=daveGetU32(dc);
//    	    d=daveGetFloat(dc);
//	    printf("FD0: %d\n",a);
//	    printf("FD4: %d\n",b);
//	    printf("FD8: %d\n",c);
//	    printf("FD12: %f\n",d);
/*
	    d=daveGetFloatAt(dc,12);
	    printf("FD12: %f\n",d);
*/	    
//	}	    
//PJO	    printf("Writing 0 to QB0\n");
//	    res=daveWriteBytes(dc, daveOutputs, 0, 0, 1, &a);
//	    daveDebug=daveDebugAll;
//	    a=1;
//	    printf("About to write 1 to Q0.4\n");
//	    wait();
//	    res=daveWriteBits(dc, daveOutputs, 0, 4, 1, &a);
/*		    
		d=daveGetSeconds(dc);
		printf("Times(by getSeconds()  ): %0.3f, ",d);
		d=daveGetSeconds(dc);
		printf("%0.3f, ",d);
		d=daveGetSeconds(dc);
		printf("%0.3f, ",d);
		d=daveGetSeconds(dc);
		printf(" %0.3f\n",d);
	    
		d=daveGetSecondsAt(dc,0);
		printf("Times(by getSecondsAt()): %0.3f, ",d);
		d=daveGetSecondsAt(dc,2);
		printf("%0.3f, ",d);
		d=daveGetSecondsAt(dc,4);
		printf("%0.3f, ",d);
		d=daveGetSecondsAt(dc,6);
		printf(" %0.3f\n",d);
*/		

	return 0;
    } else {
	printf("Couldn't open serial port %s\n",argv[adrPos]);	
	return -1;
    }	
}

/*
    Changes: 
    07/19/04  added return values in main().
    09/09/04  applied patch for variable Profibus speed from Andrew Rostovtsew.
    09/09/04  removed unused include byteswap.h
    09/10/04  removed SZL read, it doesn?t work on 200 family.
    09/11/04  added multiple variable read example code.
    03/23/04  added options to set local and target PPI addresses.
    04/09/05  removed CYGWIN defines. As there were no more differences against LINUX, it should 
	      work with LINUX defines.
*/
