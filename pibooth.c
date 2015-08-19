#include "ws2811.h"

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <dirent.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <png.h>

#include <stdlib.h>
#include <nfc/nfc.h>

#define TARGET_FREQ	WS2811_TARGET_FREQ
#define GPIO_PIN	18
#define DMA		5

#define WIDTH		8
#define HEIGHT		8
#define LED_COUNT	(WIDTH * HEIGHT)


  // Poll for a ISO14443A (MIFARE) tag
  const nfc_modulation nmMifare = {
    .nmt = NMT_ISO14443A,
    .nbr = NBR_106,
  };

ws2811_t ledstring =
{
	.freq = TARGET_FREQ,
	.dmanum = DMA,
	.channel = 
	{
		[0] =
		{
			.gpionum    = GPIO_PIN,
			.count      = LED_COUNT,
			.invert     = 0,
			.brightness = 55,
		}
	}
};
typedef struct
{
	char file_name[75];
	int width, height;
	png_byte color_type;
	png_byte bit_depth;

	png_structp png_ptr;
	png_infop info_ptr;
	int number_of_passes;
	png_bytep * row_pointers;
} png_anim_t;

typedef struct 
{
	unsigned char *id;
	int id_len;
	int anim_num;	
} token_t;


	token_t tokens[3];
  png_anim_t anims[12];
  nfc_device *pnd;
  nfc_target nt;

  // Allocate only a pointer to nfc_context
  nfc_context *context;

static void
print_hex(const uint8_t *pbtData, const size_t szBytes)
{
  size_t  szPos;
printf(" bytes: %d - ",szBytes);
  for (szPos = 0; szPos < szBytes; szPos++) {
    printf("0x%02x  ", pbtData[szPos]);
  }
  printf("\n");
}


void setBrightness(float b)
{
	return;
}

void setPixelColorRGB(int pixel, int r, int g, int b)
{
	ledstring.channel[0].leds[pixel] = (r << 16) | (g << 8) | b;
	return;
}

void clearLEDBuffer(void){
	int i;
	for(i=0; i<LED_COUNT;i++){
		setPixelColorRGB(i,0,0,0);
	}
}

/*
  Remap an x/y coordinate to a pixel index
*/
int getPixelPosition(int x, int y){

	int map[8][8] = {
		{7 ,6 ,5 ,4 ,3 ,2 ,1 ,0 },
		{8 ,9 ,10,11,12,13,14,15},
		{23,22,21,20,19,18,17,16},
		{24,25,26,27,28,29,30,31},
		{39,38,37,36,35,34,33,32},
		{40,41,42,43,44,45,46,47},
		{55,54,53,52,51,50,49,48},
		{56,57,58,59,60,61,62,63}
	};

	return map[x][y];
}

void show(){
	ws2811_render(&ledstring);
}

void abort_(const char * s, ...)
{
        va_list args;
        va_start(args, s);
        vfprintf(stderr, s, args);
        fprintf(stderr, "\n");
        va_end(args);
        abort();
}

int x, y;


int anim_delay = 50;

void read_png_file(png_anim_t *anim, char* file_name)
{
        char header[8];    // 8 is the maximum size that can be checked

        strcpy(anim->file_name, file_name);
        /* open file and test for it being a png */
        FILE *fp = fopen(file_name, "rb");
        if (!fp)
                abort_("[read_png_file] File %s could not be opened for reading", file_name);
        fread(header, 1, 8, fp);
        if (png_sig_cmp(header, 0, 8))
                abort_("[read_png_file] File %s is not recognized as a PNG file", file_name);


        /* initialize stuff */
        anim->png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

        if (!anim->png_ptr)
                abort_("[read_png_file] png_create_read_struct failed");

        anim->info_ptr = png_create_info_struct(anim->png_ptr);
        if (!anim->info_ptr)
                abort_("[read_png_file] png_create_info_struct failed");

        if (setjmp(png_jmpbuf(anim->png_ptr)))
                abort_("[read_png_file] Error during init_io");

        png_init_io(anim->png_ptr, fp);
        png_set_sig_bytes(anim->png_ptr, 8);

        png_read_info(anim->png_ptr, anim->info_ptr);

        anim->width = png_get_image_width(anim->png_ptr, anim->info_ptr);
        anim->height = png_get_image_height(anim->png_ptr, anim->info_ptr);
        anim->color_type = png_get_color_type(anim->png_ptr, anim->info_ptr);
        anim->bit_depth = png_get_bit_depth(anim->png_ptr, anim->info_ptr);

        anim->number_of_passes = png_set_interlace_handling(anim->png_ptr);
        png_read_update_info(anim->png_ptr, anim->info_ptr);


        /* read file */
        if (setjmp(png_jmpbuf(anim->png_ptr)))
                abort_("[read_png_file] Error during read_image");

        anim->row_pointers = (png_bytep*) malloc(sizeof(png_bytep) * anim->height);
        for (y=0; y<anim->height; y++)
                anim->row_pointers[y] = (png_byte*) malloc(png_get_rowbytes(anim->png_ptr,anim->info_ptr));

        png_read_image(anim->png_ptr, anim->row_pointers);
        fclose(fp);
}

void process_file(png_anim_t anim)
{
	int currentFrame;

	printf("Animation: %s \tWidth: %d \tHeight: %d\n",anim.file_name, anim.width, anim.height);
	for (currentFrame=0; currentFrame<(anim.height/8); currentFrame++){
        for (y=0; y<8; y++) {
                png_byte* row = anim.row_pointers[y + (8*currentFrame)];
                for (x=0; x<anim.width; x++) {
                        png_byte* ptr = &(row[x*3]);

			setPixelColorRGB(getPixelPosition(x,y), ptr[0], ptr[1], ptr[2]);

                }
        }
	show();
	usleep(anim_delay*1000);
	}
}

void h2rgb(float h, float *r, float *g, float *b){

	int i;
	float f, p, q, t, s, v;

	s = 1.0;
	v = 1.0;

	// Wrap hue
	if(h < 0.0 || h > 1.0){
		h=fabsf(fmodf(h,1.0));	
	}

	h *= 360.0;
	h /= 60.0;

	i = floor( h );
	f = h - i;
	p = (v * ( 1 - s ));
	q = (v * ( 1 - s * f ));
	t = (v * ( 1 - s * ( 1 - f ) ));

	switch( i ){
		case 0:
			*r = v;
			*g = t;
			*b = p;
			break;
		case 1:
			*r = q;
			*g = v;
			*b = p;
			break;
		case 2:
			*r = p;
			*g = v;
			*b = t;
			break;
		case 3:
			*r = p;
			*g = q;
			*b = v;
			break;
		case 4:
			*r = t;
			*g = p;
			*b = v;
			break;
		default:
			*r = v;
			*g = p;
			*b = q;
			break;
		
	}

}

void makeRGB(float *r, float *g, float *b, 
		float f1, float f2, float f3,
		float p1, float p2, float p3,
		float c, float w, float pos){


	*r = (( sin(f1 * pos + p1) * w ) + c) / 255;
	*g = (( sin(f2 * pos + p2) * w ) + c) / 255;
	*b = (( sin(f3 * pos + p3) * w ) + c) / 255;

}

void transformPixel(float *x, float *y, float angle){

	float px, py, cs, sn;

	cs = cos(angle);
	sn = sin(angle);
	
	px = *x * cs - *y * sn;
	py = *x * sn + *y * cs;

	*x = px;
	*y = py;
}


void shadePixel(double t, int pixel, float x, float y){

	float r, g, b;

	float angle = fmod( (double)(t)/10, (double)360);

	angle /= 57.2957795;

	float px, py, cs, sn;
	
	// Move origin to center
	x-=0.5;
	y-=0.5;

	// Pan the X/Y position on a sine wave
	x+=sin(t/10000.0);
	y+=sin(y/10000.0);

	// Rotate the pixels
	cs = cos(angle);
	sn = sin(angle);

	px = (x * cs) - (y * sn);
	py = (y * cs) + (x * sn);

	// Convert hue to RGB
	float hue = (((px+py)/8) + t / 10000.0);
	h2rgb(hue, &r, &g, &b);

	// Clamp max value
	if(r>1.0) r=1.0;
	if(g>1.0) g=1.0;
	if(b>1.0) b=1.0;

	setPixelColorRGB(pixel, (int)(r*255), (int)(g*255), (int)(b*255));

}

/*
  Loop through every pixel in the display and call shadePixel with its
  coordinates, its index and the current time vector.
*/
void run_shader(void){

	struct timeval tv;
	double t;
	while(1){
		gettimeofday(&tv, NULL);
		t = (tv.tv_sec) * 1000 + (tv.tv_usec) / 1000;
		for(y=0; y<8; y++){
			for(x=0; x<8; x++){
				int pixel = getPixelPosition(x,y);
				shadePixel(t, pixel, x/7.0, y/7.0);
			}
		}
		show();
		usleep(1);
	}

}

void loop_shader(int num){

	struct timeval tv;
	double t;
	int n=0;
	while(n<num){
		gettimeofday(&tv, NULL);
		t = (tv.tv_sec) * 1000 + (tv.tv_usec) / 1000;
		for(y=0; y<8; y++){
			for(x=0; x<8; x++){
				int pixel = getPixelPosition(x,y);
				shadePixel(t, pixel, x/7.0, y/7.0);
			}
		}
		show();
		usleep(1);
		n++;
	}

}

void init_nfc() {
	// Initialize libnfc and set the nfc_context
  nfc_init(&context);
  if (context == NULL) {
    printf("Unable to init libnfc (malloc)\n");
    exit(EXIT_FAILURE);
  }

  // Display libnfc version
  const char *acLibnfcVersion = nfc_version();
  
  printf(" uses libnfc %s\n",  acLibnfcVersion);

  // Open, using the first available NFC device which can be in order of selection:
  //   - default device specified using environment variable or
  //   - first specified device in libnfc.conf (/etc/nfc) or
  //   - first specified device in device-configuration directory (/etc/nfc/devices.d) or
  //   - first auto-detected (if feature is not disabled in libnfc.conf) device
  pnd = nfc_open(context, NULL);

  if (pnd == NULL) {
    printf("ERROR: %s\n", "Unable to open NFC device.");
    exit(EXIT_FAILURE);
  }
  // Set opened NFC device to initiator mode
  if (nfc_initiator_init(pnd) < 0) {
    nfc_perror(pnd, "nfc_initiator_init");
    exit(EXIT_FAILURE);
  }

  printf("NFC reader: %s opened\n", nfc_device_get_name(pnd));

}

/*
  Clear the display and exit gracefully
*/
void unicorn_exit(int status){
	int i;
	for (i = 0; i < 64; i++){
		setPixelColorRGB(i,0,0,0);
	}
	ws2811_render(&ledstring);
	ws2811_fini(&ledstring);
	
		  // Close NFC device
	  nfc_close(pnd);
	  // Release the context
	  nfc_exit(context);
	exit(status);
}

void set_token(token_t * token, int anim_num, const unsigned char token_id[]){
	token->id_len = sizeof(token_id);
	token->id = malloc(token->id_len * sizeof(unsigned char));
	memcpy(token->id, token_id,  token->id_len * sizeof(unsigned char));
	token->anim_num = anim_num;
}

int main(int argc, char **argv) {
	int shader = 0;
	int i;

	const unsigned char card[] = { 0x04, 0xa4, 0xbb,  0x3a,  0x40,  0x3e,  0x80 };
	const unsigned char card2[] = { 0xa7,  0x08,  0x3c,  0xf2 };
	const unsigned char card3[] = { 0x93,  0x34,  0xc6,  0x2c };
	set_token(&tokens[0], 0, card);
	set_token(&tokens[1], 5, card2);
	set_token(&tokens[2], 6, card3);

	char *cam_envp[] = { NULL };
 	char *cam_argv[] = { "/usr/bin/raspistill",  "-s", "-o", "test.jpg", "-awb", "auto", "-ifx",  "-mm",  "-q", "75", "-e", "jpg", NULL };



	if (argc >= 2){
		if(sscanf (argv[1], "%i", &anim_delay)!=1){
			printf ("Error, delay must be an integer \n");
			return 0;
		}
	}

	int newbrightness = 0;
	if (argc >= 3){
		if(sscanf (argv[2], "%i", &newbrightness)!=1){
			printf ("Error, brightness must be an integer \n");
			return 0;

		}else{
			setBrightness(newbrightness/100.0);
		}
	}
 


	for (i = 0; i < 64; i++) {
		struct sigaction sa;
		memset(&sa, 0, sizeof(sa));
		sa.sa_handler = unicorn_exit;
		sigaction(i, &sa, NULL);
	}

	setvbuf(stdout, NULL, _IONBF, 0);

	init_nfc();

	if (board_info_init() < 0)
	{
		return -1;
	}
	if(ws2811_init(&ledstring))
	{
		return -1;
	}	

	clearLEDBuffer();


	read_png_file(&anims[0], "./anim/boom.png");
read_png_file(&anims[1], "./anim/hypnotoad.png");
read_png_file(&anims[2], "./anim/nyan.png");
read_png_file(&anims[3], "./anim/off.png");
read_png_file(&anims[4], "./anim/photon.png");
read_png_file(&anims[5], "./anim/rainbow.png");
read_png_file(&anims[6], "./anim/rainbowspin.png");
read_png_file(&anims[7], "./anim/redblue.png");
read_png_file(&anims[8], "./anim/smokering.png");
read_png_file(&anims[9], "./anim/stars.png");
read_png_file(&anims[10], "./anim/trip.png");
read_png_file(&anims[11], "./anim/umbrella.png");

		while(1) {
			if (nfc_initiator_select_passive_target(pnd, nmMifare, NULL, 0, &nt) > 0) {
			    printf("The following (NFC) ISO14443A tag was found:\n");
			    printf("       UID (NFCID%c): ", (nt.nti.nai.abtUid[0] == 0x08 ? '3' : '1'));
			    print_hex(nt.nti.nai.abtUid, nt.nti.nai.szUidLen);
			   
			    
			    for (i = 0; i < 6; i++) {
			    	//printf("       Card: ");
			    	print_hex(tokens[i].id, tokens[i].id_len);
			   		int min_len = 0;
			   		if (tokens[i].id_len < nt.nti.nai.szUidLen) {
			   			min_len = tokens[i].id_len;
			   		} else {
			   			min_len = nt.nti.nai.szUidLen;
			   		}
			   		//printf("        - MemCMP: %d\n", memcmp(tokens[i].id, nt.nti.nai.abtUid, min_len));
			   		
			    	if (memcmp(tokens[i].id, nt.nti.nai.abtUid, min_len) == 0 ) {
			    		printf("        - Match Found!\n");
			    		pid_t pID = vfork();
		    		   if (pID == 0)                // child
					   {
					      // Code only executed by child process
					   		int ret = execve(cam_argv[0], cam_argv, cam_envp);
					   		
					      
					    }
					    else if (pID < 0)            // failed to fork
					    {
					        //cerr << "Failed to fork" << endl;
					        //exit(1);
					        // Throw exception
					    }
			    		process_file(anims[tokens[i].anim_num]);
			    		process_file(anims[tokens[i].anim_num]);
			    		kill(pID, SIGUSR1);
			    	}
			    }

			    //loop_shader(500);

				for (i = 0; i < 64; i++){
					setPixelColorRGB(i,0,0,0);
				}
				ws2811_render(&ledstring);
			  }


		}
		  // Close NFC device
	  nfc_close(pnd);
	  // Release the context
	  nfc_exit(context);
	unicorn_exit(0);

	  exit(EXIT_SUCCESS);
	return 0;
}
