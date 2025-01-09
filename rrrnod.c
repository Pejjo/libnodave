/*
 Program for interfacing Siemens S7 via Libnodave to rrr.
 
 **********************************************************************
 * WARNING: This and other test programs overwrite data in your PLC.  *
 * DO NOT use it on PLC's when anything is connected to their outputs.*
 * This is alpha software. Use entirely on your own risk.             * 
 **********************************************************************
 
 (C) Per Johansson 2020.

 This is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2, or (at your option)
 any later version.

*/
#include <string.h>
#include <stdlib.h>

#include "cmodule.h"
#include "log.h"
#include "posix.h"

#include <stdio.h>
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

#ifdef LINUX
#include <unistd.h>
#include <sys/time.h>
#include <fcntl.h>
#define UNIX_STYLE
#endif

// Globals
daveInterface * di;
daveConnection * dc;
_daveOSserialType fds;

PDU wr_p;
daveResultSet wr_rs

PDU rd_p;
daveResultSet rd_rs


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





struct dummy_data {
        char *custom_setting;
};

static struct dummy_data dummy_data = {0};

int config(RRR_CONFIG_ARGS) {
    	int localPPI=0;
    	int plcPPI=2;

        struct dummy_data *data = &dummy_data;

        int ret = 0;

//R        ctx->application_ptr = data;

//R        RRR_MSG_1("cmodule in config()\n");

//R        RRR_INSTANCE_CONFIG_PARSE_OPTIONAL_UTF8_DEFAULT_NULL("cmodule_custom_setting", custom_setting);

//R        if (data->custom_setting == NULL || *(data->custom_setting) == '\0') {
//R                RRR_MSG_0("Could not find setting 'cmodule_custom_setting' in configuration\n");
//R                ret = 1;
//R                goto out;
//R        }

//R        RRR_MSG_1("Custom setting: %s\n", data->custom_setting);

       
	fds.rfd=setPort("/dev/ttyUSB0","9600",'E');
	fds.wfd=fds.rfd;
    	if (fds.rfd>0) {
        	di =daveNewInterface(fds, "IF1", localPPI, daveProtoPPI, daveSpeed187k);
        	dc =daveNewConnection(di, plcPPI, 0, 0);
        	daveConnectPLC(dc);
		davePrepareReadRequest(dc, &rd_p);
        	daveAddVarToReadRequest(&rd_p,daveDB,1,100,10);
        	daveAddVarToReadRequest(&rd_p,daveFlags,0,19,4);
        	daveAddVarToReadRequest(&rd_p,daveTimer200,0,37,4);

 	}
        else {
		ret = 1;
	}



        out:
        return ret;
}

int source(RRR_SOURCE_ARGS) {
        (void)(ctx);
        (void)(message_addr);

//R        rrr_free(message);

        return 0;
}

int process(RRR_PROCESS_ARGS) {
//R        RRR_DBG_2("cmodule process timestamp %" PRIu64 "\n", message->timestamp);

//R        return rrr_send_and_free(ctx, message, message_addr);
}

int cleanup(RRR_CLEANUP_ARGS) {
        struct dummy_data *data = ctx->application_ptr;

//R        RRR_MSG_1("cmodule cleanup\n");

//R        RRR_FREE_IF_NOT_NULL(data->custom_setting);

        ctx->application_ptr = NULL;

        return 0;
}




int main(int argc, char **argv) {
    int next,adrPos,res, localPPI, plcPPI, a;
    float plcTemp, plcHumid;
    int plcVatid;
    float plcVatmr;
    int waterflag;
    unsigned char  plcLarm, tempvar, plcVaStrb;
    char outbuf[12];
    daveResultSet rs;
    PDU p;
    
    int rc;



    if (config(0, 0)==0) { 

	davePrepareReadRequest(dc, &p);
	daveAddVarToReadRequest(&p,daveDB,1,100,10);
	daveAddVarToReadRequest(&p,daveFlags,0,19,4);
	daveAddVarToReadRequest(&p,daveTimer200,0,37,4);
	while(1) {


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
		if (next<=0)
		{
		        davePrepareReadRequest(dc, &p);
        		daveAddVarToReadRequest(&p,daveDB,1,100,11);
        		daveAddVarToReadRequest(&p,daveFlags,0,19,4);
        		daveAddVarToReadRequest(&p,daveTimer200,0,37,4);

			res=daveExecReadRequest(dc, &p, &rs);
			if (res==0) {

				res=daveUseResult(dc, &rs, 0); // first result
				if (res==0) {
		                    next=60;
		       	     	    plcTemp=daveGetFloat(dc); // VD100, temperature
	                            plcHumid=daveGetFloat(dc)*10/3; // VD104, hudidity. Scale error correction 
	                            plcVatid=daveGetU16(dc);
                                    plcVaStrb=daveGetU8(dc); 

			            snprintf(outbuf, sizeof(outbuf), "%5.3f", plcTemp);
//		                    plcLarm=daveGetU8(dc);
//	                            tempvar=daveGetU8(dc);
//				    tempvar=daveGetU8(dc);
//      			    tempvar=daveGetU8(dc);
//				    plcVatmr=daveGetS16(dc);
//	                            printf("VaTid: %d\n", plcVatmr);
//				    printf("Larm: %x\n", plcLarm);
				    printf("VD100: %s VD104: %f VW108: %d\n",outbuf, plcHumid, plcVatid);
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
		}
	}
	return 0;
    } else {
	printf("Couldn't open serial port %s\n",argv[adrPos]);	
	return -1;
    }	
}


