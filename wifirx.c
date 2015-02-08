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

int GetQuality( const char * interface )
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
		perror("Could not create simple datagram socket");
		//exit(EXIT_FAILURE);
		return -1;
	}

	/* Perform the ioctl */
	if(ioctl(sockfd, SIOCGIWSTATS, &req) == -1) {
		perror("Error performing SIOCGIWSTATS");
		close(sockfd);
		return -1;
	}

	close(sockfd);

	return stats.qual.level;
}

int min = 5800;
int max = 6600;

int get_color( int nr )
{
	float v = nr - min;
	v /= (max-min);
	v *= .66666666;
	if( v > 0.66666666 ) v = 0.66666666;
	if( v < 0 ) v = 0;
	return HSVtoHEX( v, 1, 0.3 );
}

#define POWERHISTORY 1024
int powers[POWERHISTORY];
short screenx, screeny;

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

	printf( "MIN: %d / MAX: %d\n", min, max );

	CNFGBGColor = 0x800000;
	CNFGDialogColor = 0x444444;
	CNFGSetup( "WifiRX", 640, 480 );

	while(1)
	{
		int i, j = 0;
		for( i = 0; i < 30; i++ )
		{
			j += GetQuality( argv[1] );
			usleep(2000);
		}
		printf( "%d\n", j );
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
			CNFGTackSegment( i, screeny, i, screeny - powers[k]/30 );
		}
		CNFGColor( 0xffffff );

		CNFGSwapBuffers();

	}

	return 0;
}

