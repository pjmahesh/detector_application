#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include <sys/socket.h> /* for socket(), bind(), and connect() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_ntoa() */
#include <pthread.h>


#define GW_SERVER_PORT  45671

#define PFWLS_LEVEL_MEAS_VALUE_SZ  2  // In bytes

#define PFWLS_UART_MSG_MAX_LEN  10

#define PFWLS_UART_MSG_DELIM_FIELD_LEN  1
#define PFWLS_UART_MSG_TYPE_FIELD_LEN  1
#define PFWLS_UART_MSG_PYLD_LEN_FIELD_LEN  1
#define PFWLS_UART_MSG_CKSUM_FIELD_LEN   1
#define PFWLS_UART_MSG_DELIM_FIELD_LEN  1


#define PFWLS_UART_MSG_MIN_LEN  \
        (PFWLS_UART_MSG_DELIM_FIELD_LEN  \
         + PFWLS_UART_MSG_TYPE_FIELD_LEN \
         + PFWLS_UART_MSG_PYLD_LEN_FIELD_LEN \
         + PFWLS_UART_MSG_CKSUM_FIELD_LEN \
         + PFWLS_UART_MSG_DELIM_FIELD_LEN)


typedef enum
{
   PFWLS_UART_MSG_TYPE_ACK = 100,
   PFWLS_UART_MSG_TYPE_START_MEAS_CMD = 102,
   PFWLS_UART_MSG_TYPE_DISABLE_PERIODIC_MEAS_CMD = 104,
   PFWLS_UART_MSG_TYPE_ENABLE_PERIODIC_MEAS_CMD = 105,
   PFWLS_UART_MSG_TYPE_GET_LATEST_MEAS = 106,
   PFWLS_UART_MSG_TYPE_LATEST_MEAS_VALUE = 201,
   PFWLS_UART_MSG_TYPE_ERROR_IND = 201,
} PFWLS_msgType_t;


#define PFWLS_UART_MSG_START_DELIM  0xa5
#define PFWLS_UART_MSG_END_DELIM  0x5a

/*
 * Byte 0: De-limiter (0xa5)
 * Byte 1: Msg Type
 * Byte 2: Payload Length
 * Byte 3: Msg CheckSum (over all the bytes)
 * Byte 4-N: Payload
 * Byte N+1: De-limiter (0x5a)
 */


typedef struct
{
  //char serDevName[32];
  int baudRate;
  int serialFd;
  struct termios dcb;
} UART_cntxt_s;


UART_cntxt_s UART_cntxt =
{
  //port,  // "/dev/ttyUSB1",
  B9600,
  -1,
};

unsigned char verbose = 1;

int __listenFd = -1;

pthread_t  __serverThreadID;

unsigned short __latestLevelInfoInMM = 65000;

unsigned int __levelInfoMsgRxCnt = 0;


/*
 ********************************************************************
 *
 *
 *
 *
 ********************************************************************
 */
void UTIL_htons(unsigned char *buff_p, const unsigned short val)
{
   buff_p[0] = (val >> 8) & 0xff;
   buff_p[1] = (val) & 0xff;
}


/*
 ********************************************************************
 *
 *
 *
 *
 ********************************************************************
 */
int __writePort(unsigned char *buff_p, unsigned int cnt)
{
   int rc, bytesLeft = cnt, bytesWritten = 0;

   if (verbose)
       printf("\n<%s> cnt<%d> \n", __FUNCTION__, cnt);

   while (bytesLeft > 0)
   {
      rc = write(UART_cntxt.serialFd, buff_p + bytesWritten, bytesLeft);
      if (rc <= 0)
          return -1;
      else
      {
          bytesLeft -= rc;
          bytesWritten += rc;
      }
   }

   return 1;
}


/*
 ********************************************************************
 *
 *
 *
 *
 ********************************************************************
 */
int __readPort(unsigned char *buff_p, unsigned int len)
{
   int rdLen, readLeft = len, totRead = 0;

   while (readLeft > 0)
   {
      rdLen = read(UART_cntxt.serialFd, buff_p + totRead, readLeft);

      if (verbose)
          printf("\n<%s> rdLen<%d> \n", __FUNCTION__, rdLen);

      if (rdLen > 0)
      {
          totRead += rdLen;
          readLeft -= rdLen;
      }
      else
      {
          printf("\n<%s> read() failed  - %d !! \n", __FUNCTION__, rdLen);
          return rdLen;
      }
   }

   return totRead;
}


#ifdef __CYGWIN__
/*
 ********************************************************************
 *
 *
 *
 *
 ********************************************************************
 */
int __cfgUARTPort(const char* port)
{
   struct termios newtio;
   struct termios oldtio;
   struct termios latesttio;
   UART_cntxt_s *serDevCx_p = &UART_cntxt;
   int rc;

   printf("<%s> Opening serial dev <%s> \n",
          __FUNCTION__, port);

   serDevCx_p->serialFd = open(port, O_RDWR | O_NOCTTY );
   if (serDevCx_p->serialFd < 0)
   {
       printf("<%s> Failed to open serial device <%s> - errno<%d> !!\n",
              __FUNCTION__, port, errno);
       return -1;
   }

   printf("<%s> Opened serial device <%s> \n",
          __FUNCTION__, port);

#if 0
   rc = tcgetattr(serDevCx_p->serialFd, &oldtio); /* save current port settings */
   if (rc < 0)
   {
       printf("\n tcgetattr() failed !! - rc<%d>, errno<%d> \n", rc, errno);
       return -1;
   }
#endif

   bzero(&newtio, sizeof(newtio));

   rc = cfsetspeed(&newtio, serDevCx_p->baudRate);
   if (rc < 0)
   {
       printf("\n cfsetspeed() failed !! - rc<%d>, errno<%d> \n", rc, errno);
       return -1;
   }

   newtio.c_cflag = CS8 | CLOCAL | CREAD;
   newtio.c_iflag = IGNPAR;
   newtio.c_oflag = 0;

#if 0
   if ((rc = fcntl(serDevCx_p->serialFd, F_SETOWN, getpid())) < 0)
   {
       printf("\n fcntl failed !! - rc<%d>, errno<%d> \n", rc, errno);
       return -1;
   }
#endif

   /* set input mode (non-canonical, no echo,...) */
   newtio.c_lflag = 0;

   newtio.c_cc[VTIME]    = 0;   /* inter-character timer unused */
   newtio.c_cc[VMIN]     = 1;   /* blocking read until 10 chars received */

   rc = tcsetattr(serDevCx_p->serialFd, TCSANOW, &newtio);
   if (rc < 0)
   {
       printf("\n tcsetattr() failed !! - rc<%d> / errno<%d> \n", rc, errno);
       return -1;
   }

   rc = tcflush(serDevCx_p->serialFd, TCIFLUSH);
   if (rc < 0)
   {
       printf("\n tcflush() failed !! - rc<%d> \n", rc);
       return -1;
   }

   tcgetattr(serDevCx_p->serialFd, &latesttio);
   if (rc < 0)
   {
       printf("\n tcgetattr() failed !! - rc<%d> \n", rc);
       return -1;
   }

   return 1;
}

#else

/*
 ********************************************************************
 *
 *
 *
 *
 ********************************************************************
 */
int __cfgUARTPort(const char* port)
{
   UART_cntxt_s *serDevCx_p = &UART_cntxt;
   int rc;

   printf("<%s> Opening serial dev <%s> \n",
          __FUNCTION__, port);

   serDevCx_p->serialFd = open(port, O_RDWR | O_NOCTTY | O_NDELAY);
   if (serDevCx_p->serialFd < 0)
   {
       printf("\n<%s> open(%s) failed !! - errno<%d> \n",  __FUNCTION__, port, errno);
       return -1;
   }

   // Zero out port status flags
   if (fcntl(serDevCx_p->serialFd, F_SETFL, 0) != 0x0)
   {
       return -1;
   }

   bzero(&(serDevCx_p->dcb), sizeof(serDevCx_p->dcb));

   // serDevCx_p->dcb.c_cflag |= serDevCx_p->baudRate;  // Set baud rate first time
   serDevCx_p->dcb.c_cflag |= serDevCx_p->baudRate;  // Set baud rate first time
   serDevCx_p->dcb.c_cflag |= CLOCAL;  // local - don't change owner of port
   serDevCx_p->dcb.c_cflag |= CREAD;  // enable receiver

   // Set to 8N1
   serDevCx_p->dcb.c_cflag &= ~PARENB;  // no parity bit
   serDevCx_p->dcb.c_cflag &= ~CSTOPB;  // 1 stop bit
   serDevCx_p->dcb.c_cflag &= ~CSIZE;  // mask character size bits
   serDevCx_p->dcb.c_cflag |= CS8;  // 8 data bits

   // Set output mode to 0
   serDevCx_p->dcb.c_oflag = 0;

   serDevCx_p->dcb.c_lflag &= ~ICANON;  // disable canonical mode
   serDevCx_p->dcb.c_lflag &= ~ECHO;  // disable echoing of input characters
   serDevCx_p->dcb.c_lflag &= ~ECHOE;

   // Set baud rate
   cfsetispeed(&serDevCx_p->dcb, serDevCx_p->baudRate);
   cfsetospeed(&serDevCx_p->dcb, serDevCx_p->baudRate);

   serDevCx_p->dcb.c_cc[VTIME] = 0;  // timeout = 0.1 sec
   serDevCx_p->dcb.c_cc[VMIN] = 1;

   if ((tcsetattr(serDevCx_p->serialFd, TCSANOW, &(serDevCx_p->dcb))) != 0)
   {
       printf("\ntcsetattr(%s) failed !! - errno<%d> \n",
              port, errno);
       close(serDevCx_p->serialFd);
       return -1;
   }

   // flush received data
   tcflush(serDevCx_p->serialFd, TCIFLUSH);
   tcflush(serDevCx_p->serialFd, TCOFLUSH);

   return 1;
}

#endif


/*
 ********************************************************************
 *
 *
 *
 ********************************************************************
 */
unsigned char PFWLS_calcMsgCS(unsigned char *msg_p,
                              const unsigned char msgLen)
{
   unsigned int cs = 65536;
   unsigned char idx;

   for (idx=0; idx<msgLen; idx++)
        cs -= msg_p[idx];

   return (unsigned char)cs;
}


/*
 ********************************************************************
 *
 *
 *
 *
 ********************************************************************
 */
int __waitForClientConnects(void)
{
    int clientSockFd = -1;
    struct sockaddr_in clientSockAddr;
    unsigned int clientAddrLen = sizeof(clientSockAddr);

    printf("<%s> Entry ... \n", __FUNCTION__);

    /* Wait for a client to connect */
    clientSockFd = accept(__listenFd,
                          (struct sockaddr *)&clientSockAddr,
                          &clientAddrLen);
    if (clientSockFd < 0)
    {
        printf("accept() failed !! - errno<%d> \n",  errno);
        return -1;
    }

    printf("<%s> Rcvd connection from client <%s> \n",
           __FUNCTION__, inet_ntoa(clientSockAddr.sin_addr));

    return clientSockFd;
}


/*
 ********************************************************************
 *
 *
 *
 *
 ********************************************************************
 */
int __setUpTCPListenSocket(void)
{
    struct sockaddr_in servAddr;

    /* Create socket for incoming connections */
    __listenFd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (__listenFd < 0)
    {
        printf("socket() failed !! - errno<%d> \n",  errno);
        return -1;
    }

    /* Construct local address structure */
    memset(&servAddr, 0, sizeof(servAddr));   /* Zero out structure */
    servAddr.sin_family = AF_INET;    /* Internet address family */
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);   /* Any incoming interface */
    servAddr.sin_port = htons(GW_SERVER_PORT);

    /* Bind to the local address */
    if (bind(__listenFd, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0)
    {
        printf("bind() failed !! - errno<%d> \n",  errno);
        return -2;
    }

    if (listen(__listenFd, 10) < 0)
    {
        printf("listen() failed !! - errno<%d> \n",  errno);
        return -3;
    }

    return 1;
}


/*
 ********************************************************************
 *
 *
 *
 ********************************************************************
 */
int __readTCPPort(const int sockFd,
                  unsigned char *buff_p,
                  const unsigned int len)
{
   int rdLen, readLeft = len, totRead = 0;

   while (readLeft > 0)
   {
      /*
       * The recv() call is normally used only on a connected socket (see connect(2)).
       */
      rdLen = recv(sockFd, buff_p + totRead, readLeft, 0);

      if (verbose)
          printf("\n<%s> rdLen<%d> \n", __FUNCTION__, rdLen);

      if (rdLen > 0)
      {
          totRead += rdLen;
          readLeft -= rdLen;
      }
      else
      {
          printf("\n<%s> read() failed  - %d !! \n", __FUNCTION__, rdLen);
          return rdLen;
      }
   }

   return totRead;
}


/*
 ********************************************************************
 *
 *
 *
 ********************************************************************
 */
void __readProcCameraIntfMsg(const int sockFd)
{
   int rc, rxByteCnt = 0, off = 0, pyldLen = 0;
   unsigned char rxCS, byte, rxBuff[PFWLS_UART_MSG_MAX_LEN];

   do
   {
      int err = 0;

      if (rxByteCnt == 0)
      {
          pyldLen = 0;
          printf("\n--------------------------------------------------\n");
      }

      printf("<%s> rxByteCnt <%d> \n", __FUNCTION__, rxByteCnt);

      rc = __readTCPPort(sockFd, &byte, 1);

      printf("<%s> rxByteCnt <%d> / byte<0x%02x> \n",
             __FUNCTION__, rxByteCnt, (unsigned int)byte);

      rxBuff[rxByteCnt] = byte;

      if (rc == 1)
      {
          switch (rxByteCnt)
          {
             case 0:
                  if (byte == PFWLS_UART_MSG_START_DELIM)
                      rxByteCnt ++;
                  break;

             case 1:
                  if (byte == PFWLS_UART_MSG_TYPE_LATEST_MEAS_VALUE)
                      rxByteCnt ++;
                  else
                      rxByteCnt = 0;
                  break;

             case 2:
                  pyldLen = byte;
                  printf("Payload Length is %d \n", pyldLen);
                  rxByteCnt ++;
                  break;

             case 3:
                  rxCS = byte;
                  rxByteCnt ++;
                  break;

             default:
                  rxByteCnt ++;
                  if (rxByteCnt != (PFWLS_UART_MSG_MIN_LEN + pyldLen))
                      break;

                  if (byte == PFWLS_UART_MSG_END_DELIM)
                  {
                      int idx;
                      unsigned char calcCS;

                      printf("Rcvd Msg of len<%d> : ", rxByteCnt);
                      for (idx=0; idx<rxByteCnt; idx++)
                           printf(" 0x%02x ", rxBuff[idx]);
                      printf("\n\n");
                      rxBuff[3] = 0x0;
                      calcCS = PFWLS_calcMsgCS(rxBuff, rxByteCnt);
                      printf("<%s> cksum rx<0x%02x>/calc<0x%02x> \n",
                             __FUNCTION__, rxCS, calcCS);
                      if (rxCS == calcCS)
                      {
                          switch (rxBuff[1])
                          {
                             case PFWLS_UART_MSG_TYPE_LATEST_MEAS_VALUE:
                                  {
                                     __latestLevelInfoInMM = rxBuff[4];
                                     __latestLevelInfoInMM <<= 8;
                                     __latestLevelInfoInMM |= rxBuff[5];

                                     __levelInfoMsgRxCnt ++;

                                     printf("<%s> #[%u] Latest Level Info <%u> \n",
                                            __FUNCTION__,
                                            __levelInfoMsgRxCnt,
                                            (unsigned int)__latestLevelInfoInMM);
                                  }
                                  break;

                             default:
                                  {

                                  }
                                  break;
                          }
                      }
                      else
                      {
                          printf("<%s> cksum mis-match !! \n", __FUNCTION__);
                      }

                      rxByteCnt = 0;
                  }
                  break;
          }
      }
      else
      {
          printf("\n<%s> read() failed  - %d !! \n", __FUNCTION__, rc);
          err = 1;
      }

      if (err)
          break;

   } while (1);

   return;
}



/*
 ********************************************************************
 *
 *
 *
 *
 ********************************************************************
 */
void *__serverThreadFn(void *args_p)
{
    printf("<%s> Entry ... \n", __FUNCTION__);

    __setUpTCPListenSocket();

    do
    {
       int clientSockFd = __waitForClientConnects( );
       if (clientSockFd < 0)
           break;
       else
       {
           __readProcCameraIntfMsg(clientSockFd);
       }
    } while (1);

    printf("<%s> Exiting !! \n", __FUNCTION__);

    exit(1);
}


/*
 ********************************************************************
 *
 *
 *
 *
 ********************************************************************
 */
void __readProcRadioIf(void)
{
   int rc, rxByteCnt = 0, off = 0;
   unsigned char rxCS, byte, rxBuff[PFWLS_UART_MSG_MIN_LEN];
   unsigned char txBuff[PFWLS_UART_MSG_MIN_LEN + PFWLS_LEVEL_MEAS_VALUE_SZ];

   txBuff[off++] = PFWLS_UART_MSG_START_DELIM;
   txBuff[off++] = PFWLS_UART_MSG_TYPE_LATEST_MEAS_VALUE;
   txBuff[off++] = PFWLS_LEVEL_MEAS_VALUE_SZ;  // pyld len
   txBuff[off++] = 0;  // cs
   txBuff[off++] = 0;  // level MSB
   txBuff[off++] = 0;  // level LSB
   txBuff[off++] = PFWLS_UART_MSG_END_DELIM;

   do
   {
      int err = 0;

      if (rxByteCnt == 0)
          printf("\n--------------------------------------------------\n");
      printf("<%s> rxByteCnt <%d> \n", __FUNCTION__, rxByteCnt);

      rc = __readPort(&byte, 1);

      printf("<%s> rxByteCnt <%d> / byte<0x%02x> \n",
             __FUNCTION__, rxByteCnt, (unsigned int)byte);

      rxBuff[rxByteCnt] = byte;

      if (rc == 1)
      {
          switch (rxByteCnt)
          {
             case 0:
                  if (byte == PFWLS_UART_MSG_START_DELIM)
                      rxByteCnt ++;
                  break;

             case 1:
                  if (byte == PFWLS_UART_MSG_TYPE_GET_LATEST_MEAS)
                      rxByteCnt ++;
                  else
                      rxByteCnt = 0;
                  break;

             case 2:
                  if (byte == 0)
                      rxByteCnt ++;
                  else
                      rxByteCnt = 0;
                  break;

             case 3:
                  rxCS = byte;
                  rxByteCnt ++;
                  break;

             case 4:
                  if (byte == PFWLS_UART_MSG_END_DELIM)
                  {
                      int idx;
                      unsigned char calcCS;

                      rxByteCnt ++;
                      printf("Rcvd Msg of len<%d> : ", rxByteCnt);
                      for (idx=0; idx<rxByteCnt; idx++)
                           printf(" 0x%02x ", rxBuff[idx]);
                      printf("\n\n");
                      rxBuff[3] = 0x0;
                      calcCS = PFWLS_calcMsgCS(rxBuff, rxByteCnt);
                      printf("<%s> cksum rx<0x%02x>/calc<0x%02x> \n",
                             __FUNCTION__, rxCS, calcCS);
                      if (rxCS == calcCS)
                      {
                          printf("<%s> cksum matches \n", __FUNCTION__);

                          UTIL_htons(txBuff + 4, __latestLevelInfoInMM);
                          txBuff[3] = 0x0;
                          txBuff[3] = PFWLS_calcMsgCS(txBuff, sizeof(txBuff));

                          printf("Sending Response: ");
                          for (idx=0; idx< sizeof(txBuff); idx++)
                               printf(" 0x%02x ", txBuff[idx]);
                          printf("\n\n");

                          rc = __writePort(txBuff, sizeof(txBuff));
                          printf("<%s> writePort(%d) - rc<%d> \n",
                                 __FUNCTION__, sizeof(txBuff), rc);

                          if (rc != 1)
                          {
                              err = 1;
                              printf("\n<%s> write() failed !! \n", __FUNCTION__);
                          }
                          else
                          {
                              printf("\n<%s> response sent to RFD \n", __FUNCTION__);
                          }
                      }
                      else
                      {
                          printf("<%s> cksum mis-match !! \n", __FUNCTION__);
                      }

                      rxByteCnt = 0;
                  }
                  break;

             default:
                  rxByteCnt = 0;
                  break;
          }
      }
      else
      {
          printf("\n<%s> read() failed  - %d !! \n", __FUNCTION__, rc);
          err = 1;
      }

      if (err)
          break;

   } while (1);

   return;
}


/*
 ********************************************************************
 *
 *
 *
 *
 ********************************************************************
 */
int main(int argc, const char* argv[] )
{
   int rc = 0;


   rc = __cfgUARTPort(argv[1]);
   if (rc < 0)
   {
       rc = 2;
       printf("<%s> UART cfg failed !! \n", __FUNCTION__);
       goto _end;
   }
   else
   {
       printf("<%s> UART cfg done \n", __FUNCTION__);
   }

   if (pthread_create(&__serverThreadID, NULL, __serverThreadFn, NULL) != 0)
   {
       rc = 3;
       printf("<%s> pthread_create() failed !! - errno<%d> \n", __FUNCTION__, errno);
   }
   else
   {
       printf("<%s> server thread created ... \n", __FUNCTION__);
       rc = 0;
   }

   __readProcRadioIf();

_end:
   return rc;
}
