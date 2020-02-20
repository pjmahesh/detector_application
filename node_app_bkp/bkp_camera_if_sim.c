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


#define RADIO_IF_SERVER_PORT  45671


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
int __connectToRadioIfServer(void)
{
    int rc, sockFd = -1;
    struct sockaddr_in serverAddr;

    sockFd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockFd < 0)
    {
        printf("socket() failed !! - errno<%d> \n",  errno);
        return -1;
    }

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family      = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serverAddr.sin_port        = htons(RADIO_IF_SERVER_PORT);

    /* Establish the connection to the echo server */
    rc = connect(sockFd, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
    if (rc < 0)
    {
       printf("connect(%d) failed !! - errno<%d> \n", sockFd, errno);
       close(sockFd);
       return -1;
    }

    return sockFd;
}


/*
 ********************************************************************
 *
 *
 *
 *
 ********************************************************************
 */
void UTIL_htons(unsigned char *buff_p, unsigned short val)
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
int __writeTCPPort(int sockFd, unsigned char *buff_p, int cnt)
{
   int rc, bytesLeft = cnt, bytesWritten = 0;

   while (bytesLeft > 0)
   {
      /*
       * The send() call may be used only when the socket is in a connected state
       * (so that the intended recipient is known). The only difference between send()
       * and write(2) is the presence of flags.
       * No indication of failure to deliver is implicit in a send(). Locally detected
       * errors are indicated by a return value of -1.
       * On success, these calls return the number of characters sent. On error, -1 is
       * returned, and errno is set appropriately.
       */
      rc = send(sockFd, buff_p + bytesWritten, bytesLeft, 0);
      // printf("<%s> %d / %d \n", __FUNCTION__, bytesLeft,  rc);
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
int main(int argc, const char* argv[] )
{
   int sockFd, levelInMM;
   unsigned char txBuff[PFWLS_UART_MSG_MIN_LEN + PFWLS_LEVEL_MEAS_VALUE_SZ];

   sockFd = __connectToRadioIfServer();
   if (sockFd >= 0)
   {
       do
       {
          printf("Enter level (in mm) .. \n");
          scanf("%d", &levelInMM);
          printf("You entered ... %d \n", levelInMM);
          if (levelInMM >= 0)
          {
              int rc, off = 0;
   
              txBuff[off++] = PFWLS_UART_MSG_START_DELIM;
              txBuff[off++] = PFWLS_UART_MSG_TYPE_LATEST_MEAS_VALUE;
              txBuff[off++] = PFWLS_LEVEL_MEAS_VALUE_SZ;  // pyld len
              txBuff[off++] = 0;  // cs
              UTIL_htons(txBuff + off, levelInMM);
              off += 2;
              txBuff[off++] = PFWLS_UART_MSG_END_DELIM;
              txBuff[3] = PFWLS_calcMsgCS(txBuff, sizeof(txBuff));
   
              // Sending level info to radio-if server
              rc = __writeTCPPort(sockFd, txBuff, sizeof(txBuff));
              if (rc != 1)
              {
                  printf("write to TCP socket failed !! \n");
                  return 1;
              }
              else
                  printf("sent level info %d to radio-if-server \n", levelInMM);
          }
          else
              break; 
       } while (1);
   }
   else
   {
       printf("<%s> connect() failed !! \n", __FUNCTION__);
       return 2;
   }

   return 0;
}
