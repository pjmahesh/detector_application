#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <strings.h>
#include <string.h>
#include <sys/socket.h> /* for socket(), bind(), and connect() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_ntoa() */
#include <pthread.h>
#include <termios.h>
#include <unistd.h>

int serialFd = -1;


int cfgPort(char *serDevName_p, int baudRate)
{
   int rc;
   struct termios dcb;

   printf("Opening serial port dev <%s> \n", serDevName_p);

   serialFd = open((char *)serDevName_p, O_RDWR | O_NOCTTY | O_NDELAY);
   if (serialFd < 0)
   {
       printf("\n<%s> open(%s) failed !! - errno<%d> \n",
              __FUNCTION__, serDevName_p, errno);
       return -1;
   }

   // Zero out port status flags
   if (fcntl(serialFd, F_SETFL, 0) != 0x0)
   {
       return -1;
   }

   bzero(&(dcb), sizeof(dcb));

   // serialCntxt_p->dcb.c_cflag |= serialCntxt_p->baudRate;  // Set baud rate first time
   dcb.c_cflag |= baudRate;  // Set baud rate first time
   dcb.c_cflag |= CLOCAL;  // local - don't change owner of port
   dcb.c_cflag |= CREAD;  // enable receiver

   // Set to 8N1
   dcb.c_cflag &= ~PARENB;  // no parity bit
   dcb.c_cflag &= ~CSTOPB;  // 1 stop bit
   dcb.c_cflag &= ~CSIZE;  // mask character size bits
   dcb.c_cflag |= CS8;  // 8 data bits

   // Set output mode to 0
   dcb.c_oflag = 0;

   dcb.c_lflag &= ~ICANON;  // disable canonical mode
   dcb.c_lflag &= ~ECHO;  // disable echoing of input characters
   dcb.c_lflag &= ~ECHOE;

   // Set baud rate
   cfsetispeed(&dcb, baudRate);
   cfsetospeed(&dcb, baudRate);

   dcb.c_cc[VTIME] = 0;  // timeout = 0.1 sec
   dcb.c_cc[VMIN] = 1;

   if ((tcsetattr(serialFd, TCSANOW, &(dcb))) != 0)
   {
       printf("\ntcsetattr(%s) failed !! - errno<%d> \n",
              serDevName_p, errno);
       close(serialFd);
       return -1;
   }

   // flush received data
   tcflush(serialFd, TCIFLUSH);
   tcflush(serialFd, TCOFLUSH);

   return 1;
}




int __readPort(int fd, unsigned char *buff_p, unsigned int len)
{
   int rdLen, readLeft = len, totRead = 0;

   while (readLeft > 0)
   {
      rdLen = read(fd, buff_p + totRead, 1);

      if (rdLen > 0)
      {
          printf("Read byte %d  0x%02x \n", totRead, (unsigned int)buff_p[totRead]);
          totRead += rdLen;
          readLeft -= rdLen;
      }
      else
      {
          printf("read() failed  - %d !! \n", rdLen);
          return 0;
      }
   }

   printf("%s read %d bytes \n", __FUNCTION__, totRead);

   return totRead;
}



int __readImageFromUart(int fd, unsigned char *buff_p, int bytesToRead)
{
   int idx = 0;

   for (idx=0; idx<bytesToRead; idx++)
   {
      int rc;

      // printf("--------------------------------------------------------\n");
      // printf("Reading byte # %d from the ESP32 \n", idx);

      rc = read(fd, buff_p + idx, 1);
      if (rc !=  1)
      {
          printf("Error reading from the uart port !! %d \n", rc);
          break;
      }

      if ((idx % 10000) == 0)
          printf("Rcvd %d bytes so far ... \n", idx+1);

      // printf("#%d 0x%02x\n", idx, (unsigned int)buff_p[idx]);

      // printf("Looping byte back to the ESP32 ... \n");
      rc = write(fd, buff_p + idx, 1);
      if (rc !=  1)
      {
          printf("Error writing to the uart port !! %d \n", rc);
          break;
      }
   }

   if (idx == bytesToRead)
       printf("Rcvd the full file of %d bytes ... \n", idx);

   return idx;
}



int __readImageSzFromESP32(const int fd)
{
   int rc;
   int fileSz = 0;
   char *cmd = "capture";

   printf("Sending cmd capture to the EPS32 ... \n");

   rc = write(fd, cmd, strlen(cmd));
   if (rc == strlen(cmd))
   {
       unsigned char fileSzBuff[8];
       printf("Reading %d bytes from ESP32 \n", sizeof(fileSzBuff));

       rc = __readPort(fd, fileSzBuff, sizeof(fileSzBuff));
       if (rc != (sizeof(fileSzBuff)))
       {
           printf("__readPort(6) failed !! - %d \n", rc);
           return 0;
       }

       printf("ESP32 sent %s \n", fileSzBuff);

       if (fileSzBuff[0] != '<'
           || fileSzBuff[sizeof(fileSzBuff) - 1] != '>')
           return 0;

       fileSzBuff[sizeof(fileSzBuff) - 1] = '\0';

       if (sscanf(fileSzBuff + 1, "%d", &fileSz) != 1)
           return 0;

       if (fileSz <= 0 || fileSz >= 400000)
           return 0;
   }
   else
   {
       printf("failed to send cmd to ESP32 !! - %d \n", rc);
       fileSz = 0;
   }

   return fileSz;
}


int __writeFileToFS(unsigned char *buff_p, int fileSz, int fd)
{
   int rc, bytesLeft = fileSz, bytesWritten = 0;

   while (bytesLeft > 0)
   {
      rc = write(fd, buff_p + bytesWritten, bytesLeft);
      printf("\n<%s> rc<%d> \n", __FUNCTION__, rc);
      if (rc <= 0)
          return 0;
      else
      {
          bytesLeft -= rc;
          bytesWritten += rc;
      }
   }

   return 1;
}


int main(int argc, char **argv)
{
   int rc = 1;

   if (argc >= 3)
   {
       rc = cfgPort(argv[1], B500000);
       if (rc >= 0)
       {
           int fileSz = __readImageSzFromESP32(serialFd);

           if (fileSz > 0)
           {
               unsigned char *buff_p;

               printf("File size is %d bytes \n", fileSz);

               int outFileFd = open(argv[2], O_WRONLY | O_CREAT, 0644);
               if (outFileFd < 0)
               {
                   printf("Failed to create file %s for writing !! - %d \n", argv[2], outFileFd);
               }
               else
               {
                   buff_p = (unsigned char *)malloc(fileSz);
                   if (buff_p != NULL)
                   {
                       rc = __readImageFromUart(serialFd, buff_p, fileSz);
                       if (rc == fileSz)
                       {
                           printf("Received complete file from ESP32 ... writing to FS \n");
                           rc = __writeFileToFS(buff_p, fileSz, outFileFd);
                           if (rc)
                               printf("written file to FS \n");
                           else
                               printf("failed to write file to FS !! \n");
                           close(outFileFd);
                           sleep(1);
                           printf("Done ...... exiting .... \n");
                       }
                       else
                       {
                           printf("Failed to get complete file from ESP32 !! \n");
                       }
                   }
                   else
                   {
                       printf("malloc(%d) ) failed  !! \n", fileSz);
                   }
               }
           }
           else
           {
               printf("failed to get file size from ESP32 .. !! \n");
           }

           close(serialFd);
       }
   }
   else
   {
       printf("Enter serial dev name and image file name !! \n");
   }

   return rc;
}
