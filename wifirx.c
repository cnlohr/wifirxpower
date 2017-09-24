#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/wireless.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "DrawFunctions.h"


void HandleKey( int keycode, int bDown )
{
	if( keycode == 65307 ) exit( 0 );
//	printf( "Key: %d -> %d\n", keycode, bDown );
}

void HandleButton( int x, int y, int button, int bDown )
{
//	printf( "Button: %d,%d (%d) -> %d\n", x, y, button, bDown );
}

void HandleMotion( int x, int y, int mask )
{
//	printf( "Motion: %d,%d (%d)\n", x, y, mask );
}


unsigned long HSVtoHEX( float hue, float sat, float value )
{

	float pr = 0;
	float pg = 0;
	float pb = 0;

	short ora = 0;
	short og = 0;
	short ob = 0;

	float ro = fmod( hue * 6, 6. );

	float avg = 0;

	ro = fmod( ro + 6 + 1, 6 ); //Hue was 60* off...

	if( ro < 1 ) //yellow->red
	{
		pr = 1;
		pg = 1. - ro;
	} else if( ro < 2 )
	{
		pr = 1;
		pb = ro - 1.;
	} else if( ro < 3 )
	{
		pr = 3. - ro;
		pb = 1;
	} else if( ro < 4 )
	{
		pb = 1;
		pg = ro - 3;
	} else if( ro < 5 )
	{
		pb = 5 - ro;
		pg = 1;
	} else
	{
		pg = 1;
		pr = ro - 5;
	}

	//Actually, above math is backwards, oops!
	pr *= value;
	pg *= value;
	pb *= value;

	avg += pr;
	avg += pg;
	avg += pb;

	pr = pr * sat + avg * (1.-sat);
	pg = pg * sat + avg * (1.-sat);
	pb = pb * sat + avg * (1.-sat);

	ora = pr * 255;
	og = pb * 255;
	ob = pg * 255;

	if( ora < 0 ) ora = 0;
	if( ora > 255 ) ora = 255;
	if( og < 0 ) og = 0;
	if( og > 255 ) og = 255;
	if( ob < 0 ) ob = 0;
	if( ob > 255 ) ob = 255;

	return (ob<<16) | (og<<8) | ora;
}

int first = 0;

#define TX_BITRATE_LINE 14
#define RX_BITRATE_LINE 15
#define MBIT_OFFSET		12

void GetBitRates(const char * command, int *tx_bitrate, int *rx_bitrate)
{
	FILE *fp;
	char line[1035];
	int i = 0;

	/* Open the command for reading. */
	fp = popen(command, "r");
	if (fp == NULL) {
		printf("Failed to run command\n");
		exit(1);
	}

	/* Read the output a line at a time*/
	while (fgets(line, sizeof(line) - 1, fp) != NULL)
	{
		if ((i == TX_BITRATE_LINE))
		{
			//printf("%s", line);
			*tx_bitrate = strtol(line+MBIT_OFFSET,0,10);
		}

		if (i == RX_BITRATE_LINE)
		{
			//printf("%s", line);
			*rx_bitrate = strtol(line+MBIT_OFFSET,0,10);
		}
		i++;
	}

	pclose(fp);
}

int GetQuality( const char * interface, int * noise )
{
	int sockfd;
	struct iw_statistics stats;
	struct iwreq req;


	memset(&stats, 0, sizeof(stats));
	memset(&req, 0, sizeof(struct iwreq));
	strcpy( req.ifr_name, interface );
	req.u.data.pointer = &stats;
	req.u.data.length = sizeof(struct iw_statistics);
#ifdef CLEAR_UPDATED
	req.u.data.flags = 1;
#endif

	/* Any old socket will do, and a datagram socket is pretty cheap */
	if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		if( first ) perror("Could not create simple datagram socket");
		first = 0;
		//exit(EXIT_FAILURE);
		return -1;
	}

	/* Perform the ioctl */
	if(ioctl(sockfd, SIOCGIWSTATS, &req) == -1) {
		if( first ) perror("Error performing SIOCGIWSTATS");
		first = 0;
		close(sockfd);
		return -1;
	}

	close(sockfd);

	first = 0;
	if( noise ) *noise = -256+stats.qual.noise;

	return -256+stats.qual.level;
}

#define TITER 30
double min = -90;
double max = -50;

int get_color( int nr )
{
	float v = nr - min;
	v /= (max-min);
	v *= .66666666;
	if( v > 0.66666666 ) v = 0.66666666;
	if( v < 0 ) v = 0;
	return HSVtoHEX( v, 1, 1.0 );
}

#define POWERHISTORY 1024
double powers[POWERHISTORY];
short screenx, screeny;
unsigned char iw_command_buff[100];

int main( int argc, char ** argv )
{
	int ws_socket;
	struct sockaddr_in servaddr;

	int pl = 0;
	if( argc < 2 || argc > 5 )
	{
		fprintf( stderr, "Error: usage: wifirx [interafce] [optional, ip of WS receiver] [optional, min #] [optional, max #]\n" );
		return -1;
	}

	if( argc > 2 )
	{
		ws_socket=socket(AF_INET,SOCK_DGRAM,0);

		bzero(&servaddr,sizeof(servaddr));
		servaddr.sin_family = AF_INET;
		servaddr.sin_addr.s_addr=inet_addr(argv[2]);
		servaddr.sin_port=htons(7777);
	}
	else
	{
		ws_socket = 0;
	}

	if( argc > 2 )
	{
		min = atoi( argv[3] );
	}
	if( argc > 3 )
	{
		max = atoi( argv[4] );
	}

	printf( "MIN: %f / MAX: %f\n", min, max );

	sprintf(iw_command_buff,"iw dev %s station dump", argv[1]);

	CNFGBGColor = 0x800000;
	CNFGDialogColor = 0x444444;
	CNFGSetup( "WifiRX", 640, 480 );

	while(1)
	{
		int i;
		double j = 0;
		int noise;
		double noisetot = 0;
		first = 1;
		int tx_bitrate;
		int rx_bitrate;

		for( i = 0; i < 30; i++ )
		{
			j += GetQuality( argv[1], &noise );
			noisetot += noise;
			usleep(2000);
		}
		j/=TITER;
		noise/=TITER;

		GetBitRates(iw_command_buff, &tx_bitrate, &rx_bitrate);

		printf( "%4.1f %4.1f TX: %d RX: %d\n", j, noisetot, tx_bitrate, rx_bitrate);

		powers[pl] = j;
		pl++;
		if( pl >= POWERHISTORY ) pl = 0;

		if( ws_socket )
		{
			char buffer[10*3];
			int color = get_color( j );
			for( i = 0; i < sizeof(buffer)/3; i++ )
			{
				buffer[0+i*3] = ( color >> 8 ) & 0xff;
				buffer[1+i*3] = ( color >> 0 ) & 0xff;
				buffer[2+i*3] = ( color >> 16 ) & 0xff;
			}
			sendto(ws_socket,buffer,sizeof(buffer),0,
	             (struct sockaddr *)&servaddr,sizeof(servaddr));			
		}

		CNFGHandleInput();

		CNFGClearFrame();
		CNFGColor( 0xFFFFFF );
		CNFGGetDimensions( &screenx, &screeny );

		for( i = 0; i < screenx; i++ )
		{
			int k = pl - i - 1;
			while( k < 0 )
				k += POWERHISTORY;
			CNFGColor( get_color( powers[k] ) );
			CNFGTackSegment( i, 0, i, -(powers[k]+20)*(screeny/(100.0-20)) );
		}
		CNFGColor( 0xffffff );

		CNFGSwapBuffers();

	}

	return 0;
}

