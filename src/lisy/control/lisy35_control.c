/*
 socketserver for lisy35 webinterface
 bontango 07.2017
*/

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <wiringPi.h>
#include <pthread.h>
#include "../lisy35.h"
#include "../lisy_home.h"
#include "../fileio.h"
#include "../hw_lib.h"
#include "../displays.h"
#include "../coils.h"
#include "../switches.h"
#include "../utils.h"
#include "../eeprom.h"
#include "../sound.h"
#include "../lisy.h"
#include "../lisyversion.h"
#include "../fadecandy.h"
#include "../externals.h"


//the version
#define LISY35control_SOFTWARE_MAIN    0
#define LISY35control_SOFTWARE_SUB     102

//fake definiton needed in lisy_w
void core_setSw(int myswitch, unsigned char action) {  };


//fake definiton needed in lisy1
void cpunum_set_clockscale(int cpu, float clockscale) {  };

//fake definiton needed in lisy80
typedef struct {
 struct {
    unsigned int  soundBoard;
  } hw;
} core_tGameData;

core_tGameData *core_gameData;

//fake definiton needed for mame functions
typedef struct
{
 unsigned char lampMatrix[2];
} t_coreGlobals;
t_coreGlobals coreGlobals;
void lisy_nvram_write_to_file( void ) {  }
void sound_stream_update(int *dum ) {  }
unsigned char sound_stream = 0;
unsigned char  sound_enabled = 0;
const char* sndbrd_typestr(int board) {  }


//48 switches, keep it easy by using 49 elements
unsigned char Switches_LISY35[49] = { 0,0,0,0,0,0,0,0,0,0,
				      0,0,0,0,0,0,0,0,0,0,
				      0,0,0,0,0,0,0,0,0,0,
				      0,0,0,0,0,0,0,0,0,0,
				      0,0,0,0,0,0,0,0 };

//global vars
char switch_description_line1[80][80];
char switch_description_line2[80][80];
char lamp_description_line1[80][80];
char lamp_description_line2[80][80];
char lamp2_description_line1[80][80];
char lamp2_description_line2[80][80];
char coil_description_line1[20][80];
char coil_description_line2[20][80];
//global var for internal game_name structure, set by  lisy35_file_get_gamename in main
t_stru_lisy1_games_csv lisy1_game;
t_stru_lisy35_games_csv lisy35_game;
t_stru_lisy80_games_csv lisy80_game;
//global var for all contnious solenoids
unsigned char cont_sol[5];
//global var for all lamps
unsigned char lamp[80];
unsigned char lamp2[80];
unsigned char active_lampdriver_board = 0;
//global var for all sounds
unsigned char sound[32];
//global vars for all displays 6-digit
char display_D0[7]="";
char display_D1[7]="";
char display_D2[7]="";
char display_D3[7]="";
char display_D4[7]="";
char display_D5[7]="";
char display_D6[7]="";
//global vars for all displays 7-digit
char display_D0A[8]="";
char display_D1A[8]="";
char display_D2A[8]="";
char display_D3A[8]="";
char display_D4A[8]="";
char display_D5A[8]="";
//hostname settings
char hostname[10]=" "; //' ' indicates that there is no new hostname
//hostname settings
char update_path[255]=" "; //' ' indicates that there is no update file
//hostname settings
char ext_sound_no[10]=" "; //' ' indicates that there is no new extended sound to be send

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

//short form of write command
void sendit( int sockfd, char *buffer)
{
   int n;

//   printf("%s",buffer);

   n = write(sockfd,buffer,strlen(buffer));
   if (n < 0) error("ERROR writing to socket");
}


//basic infos, called from many places
void send_basic_infos( int sockfd)
{
     char buffer[256];

   sprintf(buffer,"Selected game is %s, internal number %d<br>\n",lisy35_game.long_name,lisy35_game.gamenr);
   sendit( sockfd, buffer);
   sprintf(buffer,"Special cfg:%d, AUX Board Variant:%d Soundboard variant:%d<br><br>\n",lisy35_game.special_cfg,lisy35_game.aux_lamp_variant,lisy35_game.soundboard_variant);
   sendit( sockfd, buffer);
}

//set sound and update internal vars
//simple soundcard version, second parameter to lisy35_coil_sound_set is 0
void do_sound_set( char *buffer)
{

 int sound_no,i;
 char wav_file_name[80];

 //the format here is 'Sxx' 
 //we trust ASCII values
 sound_no = (10 * (buffer[1]-48)) + buffer[2]-48;

 //set again same sound means set to zero
 if ( sound[sound_no] == 1)
  {
   lisy35_sound_std_sb_set( 0 );
   //set internal var (remember)
   for(i=0; i<=31; i++) sound[i] = 0;
   sound[0] = 1;
   return;
  }


 //set internal var (remember)
 for(i=0; i<=31; i++) sound[i] = 0;
 sound[sound_no] = 1;

 //JustBoom Sound? we may want to play wav files here
 if ( ls80opt.bitv.JustBoom_sound )
 {
  //construct the filename, according to game_nr
  sprintf(wav_file_name,"%s%03d/%d.wav",LISY35_SOUND_PATH,lisy35_game.gamenr,sound_no);
  sprintf(debugbuf,"/usr/bin/aplay %s",wav_file_name);
  system(debugbuf);
 }

 //now set sound
 lisy35_sound_std_sb_set( sound_no );

}


//set options for soundcard control
void do_soption_set( char *buffer)
{

 int selection, i;
 static int sound;

 //we trust ASCII values
 //the format here is 'Px_'
 selection = buffer[1]-48;

 switch(selection)
  {
	case 1:  //say it
	 if ( buffer[3] != '\0') {
		 sprintf(debugbuf,"/bin/echo \"%s\" | /usr/bin/festival --tts",&buffer[3]);
                 system(debugbuf);
		}
	break;
	case 2:  //play the sound
	 if ( buffer[3] != '\0') 
		{
		  lisy35_play_wav(atoi(&buffer[3]));
 		  sound = atoi(&buffer[3]); //store the sound
		}
	break;
	case 3:  //play the soundrange
	 if ( buffer[3] != '\0')
	   { 
		for(i=sound+1; i<=atoi(&buffer[3]); i++) 
		   {
		    sleep(2);
		    lisy35_play_wav(i);
		   }
		}
	break;

  }
}



//do an update of lisy
void do_updatepath_set( char *buffer)
{

 //we trust ASCII values
 //the format here is 'U_'
 sprintf(update_path,"%s",&buffer[2]);

 printf("update path: %s\n",&buffer[2]);

}

//do an update of lisy, local file
void do_update_local( int sockfd, char *what)
{

 char *real_name,*tmp_name;
 char *line;
 char buffer[255];

 //we trust ASCII values
 //the format here is 'Y'
 line = &what[1];

 real_name = strtok(line, ";");
 tmp_name = strtok(NULL, ";");

 sprintf(buffer,"<a href=\"./index.php\">Back to LISY Homepage</a><br><br>");
 sendit( sockfd, buffer);

 sprintf(buffer,"WE WILL NOW do the System update<br><br>\n");
 sendit( sockfd, buffer);

 //set mode to read - write
 sprintf(buffer,"setting system mode to read/write<br><br>\n");
 sendit( sockfd, buffer);
 system("/bin/mount -o remount,rw /boot");
 system("/bin/mount -o remount,rw /");

 //just unpack the lisy_update.tgz and execute install.sh from within
 sprintf(buffer,"try to get extract the update file<br><br>\n");
 sendit( sockfd, buffer);
 sprintf(buffer,"/bin/tar -xzf %s -C /home/pi/update",tmp_name);
 system(buffer);

 sprintf(buffer,"try to execute install.sh from within update pack<br><br>\n");
 sendit( sockfd, buffer);
 sprintf(buffer,"/bin/bash /home/pi/update/install.sh");
 system(buffer);

 sprintf(buffer,"update done, you may want to reboot now<br><br>\n");
 sendit( sockfd, buffer);

}

//set the hostname of the system and reboot
void do_hostname_set( char *buffer)
{

 //we trust ASCII values
 //the format here is 'H_'
 sprintf(hostname,"%s",&buffer[2]);

}

//set the command for the soundcard
void do_ext_sound_set( char *buffer)
{

 //we trust ASCII values
 //the format here is 'E_'
 sprintf(ext_sound_no,"%s",&buffer[2]);

}



//set the display according to buffer message
void do_display_set( char *buffer)
{
 int display_no;
 char display_str[21];

 //we trust ASCII values
 display_no = buffer[1]-48;

 //the format here is 'Dx_' 'DxA_'
 if (buffer[2] == '_') //6-digit
 {
  switch( display_no )
  {
   case 0: 
		sprintf(display_str,"%-6s",&buffer[3]);
 		display35_show_str( 0, display_str);
	        strcpy(display_D0,display_str);
		break;
   case 1: 
		sprintf(display_str,"%-6s",&buffer[3]);
 		display35_show_str( 1, display_str);
	        strcpy(display_D1,display_str);
		break;
   case 2: 
		sprintf(display_str,"%-6s",&buffer[3]);
 		display35_show_str( 2, display_str);
	        strcpy(display_D2,display_str);
		break;
   case 3: 
		sprintf(display_str,"%-6s",&buffer[3]);
 		display35_show_str( 3, display_str);
	        strcpy(display_D3,display_str);
		break;
   case 4: 
		sprintf(display_str,"%-6s",&buffer[3]);
 		display35_show_str( 4, display_str);
	        strcpy(display_D4,display_str);
		break;
   case 5: 
		sprintf(display_str,"%-6s",&buffer[3]);
 		display35_show_str( 5, display_str);
	        strcpy(display_D5,display_str);
		break;
   case 6: 
		sprintf(display_str,"%-6s",&buffer[3]);
 		display35_show_str( 6, display_str);
	        strcpy(display_D6,display_str);
		break;
  }
 }//6-digit
 else if (buffer[2] == 'A') //7-digit
 {
  switch( display_no )
  {
   case 0:
                sprintf(display_str,"%-7s",&buffer[4]);
                display35_show_str( 0, display_str);
                strcpy(display_D0A,display_str);
                break;
   case 1:
                sprintf(display_str,"%-7s",&buffer[4]);
                display35_show_str( 1, display_str);
                strcpy(display_D1A,display_str);
                break;
   case 2:
                sprintf(display_str,"%-7s",&buffer[4]);
                display35_show_str( 2, display_str);
                strcpy(display_D2A,display_str);
                break;
   case 3:
                sprintf(display_str,"%-7s",&buffer[4]);
                display35_show_str( 3, display_str);
                strcpy(display_D3A,display_str);
                break;
   case 4:
                sprintf(display_str,"%-7s",&buffer[4]);
                display35_show_str( 4, display_str);
                strcpy(display_D4A,display_str);
                break;
   case 5:
                sprintf(display_str,"%-7s",&buffer[4]);
                display35_show_str( 5, display_str);
                strcpy(display_D5A,display_str);
                break;
  }
 }//7-digit
printf("buffer is %s\n",buffer);
}


//pulse specific momentary solenoid
void do_mom_solenoid_set( char *buffer)
{
 int solenoid;

 //the format here is 'Cxx' or 'Cxx'
 //we trust ASCII values
 solenoid = (10 * (buffer[1]-48)) + buffer[2]-48;

 //pulse the coil
 lisy35_coil_set( solenoid, 2);

}

//pulse specific continous solenoid
void do_cont_solenoid_set( char *buffer)
{
 int solenoid;
 int action;

 //the format here is 'Oxx_on' or 'Oxx_off'
 //we trust ASCII values
 solenoid = (10 * (buffer[1]-48)) + buffer[2]-48;

 //cont solenoids have an offset of 15
 solenoid += 15;

 //on or off?
 if ( buffer[5] == 'f') action=0; else  action=1;

 //set the coil pulse the coil
 lisy35_coil_set( solenoid, action);
 cont_sol[solenoid-15] = action;

}


//read all the set dip switch settings
//and store to filesystem
void do_dip_set( char *buffer)
{
 int dip_no,i;
 char dip_setting[10];
 static char part1[32][80];
 static char part2[32][257];
 char line[512];

 //remember number of settings received so far
 static int no_settings = 0;

 //the format here is ' V_DIPxx_ON' or ' V_DIPxx_OFF'
 //or ' K_DIPxx_'comment' 
 //we trust ASCII values
 dip_no = (10 * (buffer[5]-48)) + buffer[6]-48;

 //Note: we do NOT trust the order of the settings
 //as sometimes the browser does mix things up
 //so we collect all settings an write them all at the end
 //which is after receiving 64 entries

 //dip setting?
 if ( buffer[0] == 'V')
  {
   //construct first part of the line
   if ( buffer[9] == 'N') strcpy(dip_setting,"ON"); else strcpy(dip_setting,"OFF");
   sprintf(part1[dip_no-1],"%d;%s;",dip_no,dip_setting);
   no_settings++;
  }
 //no, it is a comment
 else
  {
   //construct second part of the line
   strcpy( part2[dip_no-1],&buffer[8]);
   //construct the line
   no_settings++;
  }

 //once all collected, we write them to the file
 if ( no_settings >=64 )
 {
   //dip number 1 ( 0 as we start with 0 ) means we need to open/create the file: mode = 0
   sprintf(line,"%s%s\n",part1[0],part2[0]);
   if ( lisy35_file_write_dipfile( 0, line ) < 0) syserr( "problems dip setting file", 77, 1);

   //now write settings 2..31 with mode 1
   for ( i=1; i<=30; i++)
    {   
      sprintf(line,"%s%s\n",part1[i],part2[i]);
      if ( lisy35_file_write_dipfile( 1, line ) < 0) syserr( "problems dip setting file", 77, 1);
    }

   //dip number 32 (31)  means we need to close the file after writing, mode = 2
   sprintf(line,"%s%s\n",part1[31],part2[31]);
   if ( lisy35_file_write_dipfile( 2, line ) < 0) syserr( "problems dip setting file", 77, 1);

   //reset number of settings
   no_settings = 0;
 }

}//do_dip_set

// LAMPS


//the blinking thread
void *do_lamp_blink(void *myarg)
{
 int i,nu;
 int action = 1;

 pthread_setcancelstate (PTHREAD_CANCEL_ENABLE, NULL);
 pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

 nu = (int *)myarg;

 while(1) //run untilmainfunction send cancel
 {
 if (action == 0) action = 1; else action = 0;
 for(i=0; i<=nu; i=i+2) lisy35_lamp_set( active_lampdriver_board, i, action);  

 if (action == 0) action = 1; else action = 0;
 for(i=1; i<=nu; i=i+2) lisy35_lamp_set( active_lampdriver_board, i, action);  
 
 sleep(1);
 if (action == 0) action = 1; else action = 0;
 }
 return NULL;

}


//set lamp and update internal vars
void do_lamp_set( char *buffer)
{

 int lamp_no;
 int action;
 int i,nu;

 //for the blinking thread
 static pthread_t p;
 int myarg;

 //the format here is 'Lxx_on' or 'Lxx_off'
 //we trust ASCII values
 lamp_no = (10 * (buffer[1]-48)) + buffer[2]-48;
 
 //on or off?
 if ( buffer[5] == 'f') action=0; else  action=1;

 //how many lamps
   if ( active_lampdriver_board == 1 )
   {
    switch ( lisy35_game.aux_lamp_variant )
      {
        case AS_2518_43_12_LAMPS: nu = 11; break;
        case AS_2518_52_28_LAMPS: nu = 27; break;
        case AS_2518_23_60_LAMPS: nu = 59; break;
        default: nu=0; break;
      }
   }
   else nu=59;


 //special lamp 77 means ALL lamps
 if ( lamp_no == 77)
 {
  for ( i=0; i<=nu; i++)
   {
     lisy35_lamp_set( active_lampdriver_board, i, action);  
     lamp[i] = action;
     lamp[77] = action;
   } 
 }
 //special lamp 78 means blinking lamps with pthread
 else if ( lamp_no == 78)
 {
  myarg = nu;
  //start thread here
  lamp[lamp_no] = action;
  if (action) //start thread
    pthread_create (&p, NULL, do_lamp_blink, (void *)myarg);
  else //cancel thread
   {
    pthread_cancel (p);
    pthread_join (p, NULL);
  //and set all lamps to OFF
  for ( i=0; i<=nu; i++)
   {
     lisy35_lamp_set( active_lampdriver_board, i, action);  
     lamp[i] = action;
   } 
  }
 }
 else
 {
  lisy35_lamp_set( active_lampdriver_board, lamp_no, action); 
  lamp[lamp_no] = action;
 }

}

//read the lamp descriptions from the file
#define LISY35_LAMPS_PATH "/boot/lisy/lisy35/control/lamp_descriptions/"
#define LISY35_LAMPS_FILE "_lisy35_lamps.csv"
void get_lamp_descriptions(void)
{

   FILE *fstream;
   char lamp_file_name[80];
   char buffer[1024];
   char *line,*desc;
   int first_line = 1;
   int lamp_no;




 //construct the filename; using global var lisy80_gamenr
 sprintf(lamp_file_name,"%s%03d%s",LISY35_LAMPS_PATH,lisy35_game.gamenr,LISY35_LAMPS_FILE);

 //try to read the file with game nr
 fstream = fopen(lamp_file_name,"r");
   if(fstream != NULL)
   {
      fprintf(stderr,"LISY35 Info: lamp descriptions according to %s\n\r",lamp_file_name);
   }
   else
   {
    //second try: to read the file with default
    //construct the new filename; using 'default'
    sprintf(lamp_file_name,"%sdefault%s",LISY35_LAMPS_PATH,LISY35_LAMPS_FILE);
    fstream = fopen(lamp_file_name,"r");
      if(fstream != NULL)
      {
      fprintf(stderr,"LISY35 Info: lamp descriptions according to %s\n\r",lamp_file_name);
      }
    }//second try

  //check if first or second try where successfull
  if(fstream == NULL)
      {
        fprintf(stderr,"LISY35 Info: lamp descriptions not found \n\r");
        return ;
        }
  //now assign teh descriptions
   while( ( line=fgets(buffer,sizeof(buffer),fstream)) != NULL)
   {
     if (first_line) { first_line=0; continue; } //skip first line (Header)
     //interpret the line
     lamp_no = atoi(strtok(line, ";"));
     if (lamp_no <60)  //max 60 lamps
        {
	  if (( desc = strtok(NULL, ";") ) != NULL )
	      {
	  	strcpy ( lamp_description_line1[lamp_no], desc);
	  	//remove trailing CR/LF
		lamp_description_line1[lamp_no][strcspn(lamp_description_line1[lamp_no], "\r\n")] = 0;
	  	//if we have an empty string, put one blank 
		if( strlen( lamp_description_line1[lamp_no]) == 0) strcpy(  lamp_description_line1[lamp_no]," ");
		}
	  else  strcpy ( lamp_description_line1[lamp_no], " ");

	  if (( desc = strtok(NULL, ";") ) != NULL )
	      {
	  	strcpy ( lamp_description_line2[lamp_no], desc);
	  	//remove trailing CR/LF
		lamp_description_line2[lamp_no][strcspn(lamp_description_line2[lamp_no], "\r\n")] = 0;
	  	//if we have an empty string, put one blank 
		if( strlen( lamp_description_line2[lamp_no]) == 0) strcpy(  lamp_description_line2[lamp_no]," ");
		}
	  else  strcpy ( lamp_description_line2[lamp_no], " ");
        }
	else fprintf(stderr,"LISY35 Info: Lamp descriptions wrong info \n\r");
   }
}

//read the lamp descriptions from the file (second board)
//#define LISY35_LAMPS_PATH "/boot/lisy/lisy35/control/lamp_descriptions/"
#define LISY35_LAMPS2_FILE "_lisy35_lamps2.csv"
void get_lamp2_descriptions(void)
{

   FILE *fstream;
   char lamp_file_name[80];
   char buffer[1024];
   char *line,*desc;
   int first_line = 1;
   int lamp_no;




 //construct the filename; using global var lisy80_gamenr
 sprintf(lamp_file_name,"%s%03d%s",LISY35_LAMPS_PATH,lisy35_game.gamenr,LISY35_LAMPS2_FILE);

 //try to read the file with game nr
 fstream = fopen(lamp_file_name,"r");
   if(fstream != NULL)
   {
      fprintf(stderr,"LISY35 Info: lamp descriptions (sec. board)  according to %s\n\r",lamp_file_name);
   }
   else
   {
    //second try: to read the file with default
    //construct the new filename; using 'default'
    sprintf(lamp_file_name,"%sdefault%s",LISY35_LAMPS_PATH,LISY35_LAMPS2_FILE);
    fstream = fopen(lamp_file_name,"r");
      if(fstream != NULL)
      {
      fprintf(stderr,"LISY35 Info: lamp descriptions (sec. board)  according to %s\n\r",lamp_file_name);
      }
    }//second try

  //check if first or second try where successfull
  if(fstream == NULL)
      {
        fprintf(stderr,"LISY35 Info: lamp descriptions (sec. board)  not found \n\r");
        return ;
        }
  //now assign teh descriptions
   while( ( line=fgets(buffer,sizeof(buffer),fstream)) != NULL)
   {
     if (first_line) { first_line=0; continue; } //skip first line (Header)
     //interpret the line
     lamp_no = atoi(strtok(line, ";"));
     if (lamp_no <60)  //max 60 lamps
        {
	  if (( desc = strtok(NULL, ";") ) != NULL )
	      {
	  	strcpy ( lamp2_description_line1[lamp_no], desc);
	  	//remove trailing CR/LF
		lamp2_description_line1[lamp_no][strcspn(lamp2_description_line1[lamp_no], "\r\n")] = 0;
	  	//if we have an empty string, put one blank 
		if( strlen( lamp2_description_line1[lamp_no]) == 0) strcpy(  lamp2_description_line1[lamp_no]," ");
		}
	  else  strcpy ( lamp2_description_line1[lamp_no], " ");

	  if (( desc = strtok(NULL, ";") ) != NULL )
	      {
	  	strcpy ( lamp2_description_line2[lamp_no], desc);
	  	//remove trailing CR/LF
		lamp2_description_line2[lamp_no][strcspn(lamp2_description_line2[lamp_no], "\r\n")] = 0;
	  	//if we have an empty string, put one blank 
		if( strlen( lamp2_description_line2[lamp_no]) == 0) strcpy(  lamp2_description_line2[lamp_no]," ");
		}
	  else  strcpy ( lamp2_description_line2[lamp_no], " ");
        }
	else fprintf(stderr,"LISY35 Info: Lamp descriptions wrong info \n\r");
   }
}

//read the switch descriptions from the file
#define LISY35_SWITCHES_PATH "/boot/lisy/lisy35/control/switch_descriptions/"
#define LISY35_SWITCHES_FILE "_lisy35_switches.csv"
void get_switch_descriptions(void)
{

   FILE *fstream;
   char switch_file_name[80];
   char buffer[1024];
   char *line,*desc;
   int first_line = 1;
   int switch_no;




 //construct the filename; using global var lisy80_gamenr
 sprintf(switch_file_name,"%s%03d%s",LISY35_SWITCHES_PATH,lisy35_game.gamenr,LISY35_SWITCHES_FILE);

 //try to read the file with game nr
 fstream = fopen(switch_file_name,"r");
   if(fstream != NULL)
   {
      fprintf(stderr,"LISY35 Info: switch descriptions according to %s\n\r",switch_file_name);
   }
   else
   {
    //second try: to read the file with default
    //construct the new filename; using 'default'
    sprintf(switch_file_name,"%sdefault%s",LISY35_SWITCHES_PATH,LISY35_SWITCHES_FILE);
    fstream = fopen(switch_file_name,"r");
      if(fstream != NULL)
      {
      fprintf(stderr,"LISY35 Info: switch descriptions according to %s\n\r",switch_file_name);
      }
    }//second try

  //check if first or second try where successfull
  if(fstream == NULL)
      {
        fprintf(stderr,"LISY35 Info: DIP switch descriptions not found \n\r");
        return ;
        }
  //now assign teh descriptions
   while( ( line=fgets(buffer,sizeof(buffer),fstream)) != NULL)
   {
     if (first_line) { first_line=0; continue; } //skip first line (Header)
     //interpret the line
     switch_no = atoi(strtok(line, ";"));
     if (switch_no <80)
        {
          if (( desc = strtok(NULL, ";") ) != NULL )
              {
                strcpy ( switch_description_line1[switch_no], desc);
                //remove trailing CR/LF
                switch_description_line1[switch_no][strcspn(switch_description_line1[switch_no], "\r\n")] = 0;
                }
          else  strcpy ( switch_description_line1[switch_no], "");

          if (( desc = strtok(NULL, ";") ) != NULL )
              {
                strcpy ( switch_description_line2[switch_no], desc);
                //remove trailing CR/LF
                switch_description_line2[switch_no][strcspn(switch_description_line2[switch_no], "\r\n")] = 0;
                }
          else  strcpy ( switch_description_line2[switch_no], "");
        }
        else fprintf(stderr,"LISY35 Info: Switch descriptions wrong info \n\r");
   }

}


//SOLENOIDS

//read the coil descriptions from the file
#define LISY35_COILS_PATH "/boot/lisy/lisy35/control/coil_descriptions/"
#define LISY35_COILS_FILE "_lisy35_coils.csv"
void get_coil_descriptions(void)
{

   FILE *fstream;
   char coil_file_name[80];
   char buffer[1024];
   char *line,*desc;
   int first_line = 1;
   int coil_no;




 //construct the filename; using global var lisy80_gamenr
 sprintf(coil_file_name,"%s%03d%s",LISY35_COILS_PATH,lisy35_game.gamenr,LISY35_COILS_FILE);

 //try to read the file with game nr
 fstream = fopen(coil_file_name,"r");
   if(fstream != NULL)
   {
      fprintf(stderr,"LISY35 Info: coil descriptions according to %s\n\r",coil_file_name);
   }
   else
   {
    //second try: to read the file with default
    //construct the new filename; using 'default'
    sprintf(coil_file_name,"%sdefault%s",LISY35_COILS_PATH,LISY35_COILS_FILE);
    fstream = fopen(coil_file_name,"r");
      if(fstream != NULL)
      {
      fprintf(stderr,"LISY35 Info: coil descriptions according to %s\n\r",coil_file_name);
      }
    }//second try

  //check if first or second try where successfull
  if(fstream == NULL)
      {
        fprintf(stderr,"LISY35 Info: coil descriptions not found \n\r");
        return ;
        }
  //now assign teh descriptions
   while( ( line=fgets(buffer,sizeof(buffer),fstream)) != NULL)
   {
     if (first_line) { first_line=0; continue; } //skip first line (Header)
     //interpret the line
     coil_no = atoi(strtok(line, ";"));
     if (coil_no <=19)
        {
          if (( desc = strtok(NULL, ";") ) != NULL )
              {
                strcpy ( coil_description_line1[coil_no], desc);
                //remove trailing CR/LF
                coil_description_line1[coil_no][strcspn(coil_description_line1[coil_no], "\r\n")] = 0;
	  	//if we have an empty string, put one blank 
		if( strlen( coil_description_line1[coil_no]) == 0) strcpy(  coil_description_line1[coil_no]," ");
                }
          else  strcpy ( switch_description_line1[coil_no], "");

          if (( desc = strtok(NULL, ";") ) != NULL )
              {
                strcpy ( coil_description_line2[coil_no], desc);
                //remove trailing CR/LF
                coil_description_line2[coil_no][strcspn(coil_description_line2[coil_no], "\r\n")] = 0;
	  	//if we have an empty string, put one blank 
		if( strlen( coil_description_line2[coil_no]) == 0) strcpy(  coil_description_line2[coil_no]," ");
                }
          else  strcpy ( coil_description_line2[coil_no], "");
        }
        else fprintf(stderr,"LISY35 Info: coil descriptions wrong info \n\r");
   }

}


//send all the infos about the solenoids
void send_mom_solenoid_infos( int sockfd )
{
  int i,j,coil_no;
  char colorcode[80],buffer[512];

     //basic info, header line
     send_basic_infos(sockfd);
     sprintf(buffer,"push button to PULSE specific solenoid<br><br>\n");
     sendit( sockfd, buffer);
     //the color and style
     strcpy(colorcode,"style=\'BACKGROUND-COLOR:powderblue; width: 125px; margin:auto; height: 5em;\'");

  //15 momenary solenoids for Bally
  for(j=0; j<=2; j++)
  {
   for(i=1; i<=5; i++)
    {
     coil_no = j * 5 + i;
     // matiou: replaced <input> with <button> to allow 2 lines of text to describe solenoid
     //sprintf(buffer,"<form action=\'\' method=\'post\'><input type=\'submit\' name=\'C%02d\' %s value=\'%s\n%s\' /> </form>\n",coil_no,colorcode,coil_description_line1[coil_no],coil_description_line2[coil_no]);
     sprintf(buffer,"<form action=\'\' method=\'post\'><button type=\'submit\' name=\'C%02d\' %s>%s<BR/>%s</button></form>\n",coil_no,colorcode,coil_description_line1[coil_no],coil_description_line2[coil_no]);
     sendit( sockfd, buffer);
    }
  sprintf(buffer,"<br>\n");
  sendit( sockfd, buffer);
  }

}

//send all the infos about the sound
void send_sound_infos( int sockfd )
{

  int i,j,k,sound_no;
  char colorcode[80],buffer[256];
  unsigned char lsb,msb;


     //basic info, header line
   send_basic_infos(sockfd);


if( lisy35_game.soundboard_variant == LISY35_SB_CHIMES)
{
     sprintf(buffer,"this game uses Chimes, please use Solenoids to test sound<br><br>\n");
     sendit( sockfd, buffer);
}
else if( lisy35_game.soundboard_variant == LISY35_SB_STANDARD) //standard SB
{
     sprintf(buffer,"push button to send specific sound<br><br>\n");
     sendit( sockfd, buffer);

i = 0;
for(j=0; j<=7; j++)
  {
  for(k=0; k<=3; k++)
    {
     if (sound[i]) strcpy(colorcode,"style=\'BACKGROUND-COLOR:yellow; width: 125px; height: 5em;\'");
                 else  strcpy(colorcode,"style=\'BACKGROUND-COLOR:powderblue; width: 125px; height: 5em;\'");
     sprintf(buffer,"<form action=\'\' method=\'post\'><input type=\'submit\' name=\'S%02d\' %s value=\'Sound\n%02d\' /> </form>\n",i,colorcode,i);
     sendit( sockfd, buffer);
    //next sound
    i++;
    }
   sprintf(buffer,"<br>\n");
   sendit( sockfd, buffer);
  }
}
else if( lisy35_game.soundboard_variant == LISY35_SB_EXTENDED)
{

  if ( ext_sound_no[0] != ' ')  //there was a setting of soundcard command
  {

   //calculate soundnumber/command ( max=255 from html input type)
   sound_no = atoi(ext_sound_no);

   //calculate lsb.msb for output
   lsb = sound_no%16;
   msb = sound_no/16;

   //send it
   lisy35_sound_ext_sb_set(lsb); //LSB
   lisy35_sound_ext_sb_set(msb); //MSB

   sprintf(buffer,"a command with value %d ( lsb:%d msb:%d) has been send to the soundcard<br><br>\n",sound_no,lsb,msb);
   sendit( sockfd, buffer);

   //reset command setting to prevent endless loop
   ext_sound_no[0] = ' ';

  //start with some header
  sprintf(buffer,"send another command (sound) to the soundcard<br><br>\n");
  sendit( sockfd, buffer);

  sprintf(buffer,"<p>  Command: <input type=\"number\" max=\"255\" name=\"E\" size=\"3\" maxlength=\"3\" value=\"\" /></p>");
  sendit( sockfd, buffer);

  sprintf(buffer,"<p><input type=\"submit\" /></p> ");
  sendit( sockfd, buffer);

  }
  else  //normal header
  {

  //start with some header
  sprintf(buffer,"send one command (sound) to the soundcard<br><br>\n");
  sendit( sockfd, buffer);

  sprintf(buffer,"<p>  Command: <input type=\"number\" max=\"255\" name=\"E\" size=\"3\" maxlength=\"3\" value=\"\" /></p>");
  sendit( sockfd, buffer);

  sprintf(buffer,"<p><input type=\"submit\" /></p> ");
  sendit( sockfd, buffer);
  }

}


}


void send_dipswitch_infos( int sockfd )
{

 int i;
 unsigned char dipvalue; 
 char dip_comment[256];
 char filename[80];
 char buffer[512];

 //basic info, header line
   send_basic_infos(sockfd);
 //dummy read to init read csv routine
 dipvalue = lisy35_file_get_onedip( 1, dip_comment, filename, 1 );
 //and the source where it came from
 sprintf(buffer,"DIP switch settings according to %s<br><br>\n",filename);
 sendit( sockfd, buffer);


 //now send the status
 sprintf(buffer,"<form action='' method='post'>");
 sendit( sockfd, buffer);

 for( i=1; i<=32; i++)
  {
    dipvalue = lisy35_file_get_onedip( i, dip_comment, filename, 0 );
    sprintf(buffer,"<tr>\nSwitch No:%02d -\n",i);
    sendit( sockfd, buffer);
    if ( dipvalue )
     {
       sprintf(buffer,"<td><input id=\"on\" type=\"radio\" name=\"dip%02d_v\" value=\"DIP%02d_ON\" checked></td> ON",i,i);
       sendit( sockfd, buffer);
       sprintf(buffer,"<td><input id=\"off\" type=\"radio\" name=\"dip%02d_v\" value=\"DIP%02d_OFF\"></td> OFF",i,i);
       sendit( sockfd, buffer);
     }
    else
     {
       sprintf(buffer,"<td><input id=\"on\" type=\"radio\" name=\"dip%02d_v\" value=\"DIP%02d_ON\"></td> ON",i,i);
       sendit( sockfd, buffer);
       sprintf(buffer,"<td><input id=\"off\" type=\"radio\" name=\"dip%02d_v\" value=\"DIP%02d_OFF\" checked></td> OFF",i,i);
       sendit( sockfd, buffer);
     }

    sprintf(buffer," --- comment --- ");
    sendit( sockfd, buffer);
    sprintf(buffer,"<input type=\"text\" name=\"dip%02d_k\" size=\"200\" maxlength=\"256\" value=\"%s\" ><br>\n</tr>\n",i,dip_comment);
    sendit( sockfd, buffer);
  }


 //basic info, footer line
 sprintf(buffer,"<br><input type='submit' name='setdip' style='BACKGROUND-COLOR:powderblue; width: 125px; height: 5em;' value='set values'><form>");
 sendit( sockfd, buffer);
 sprintf(buffer," Note: In case of default values this will create a specific definition file for your pin<br>");
 sendit( sockfd, buffer);



} 

void send_lamp2_infos( int sockfd )
{
  int i,j,lamp_no;
  char colorcode[80],buffer[512],name[10];
  int rows, col;

  //colorcodes
  char *code_yellow = "style=\'BACKGROUND-COLOR:yellow; width: 125px; margin:auto; height: 5em;\'";
  char *code_blue = "style=\'BACKGROUND-COLOR:powderblue; width: 125px; margin:auto; height: 5em;\'";

  //set the active lampdriver
  active_lampdriver_board = 1;

     //basic info, header line
   send_basic_infos(sockfd);
     sprintf(buffer,"push button to switch lamp OFF or ON  Yellow lamps are ON<br><br>\n");
     sendit( sockfd, buffer);

    //do we have a secondary lampdriverboard 
    if ( lisy35_game.aux_lamp_variant == NO_AUX_BOARD )
      {
        sprintf(buffer,"This game has no secondary lampdriver board<br><br>\n");
        sendit( sockfd, buffer);
	return;
      }

   //special lamp 77, set all lamps
   lamp_no = 77;
   if (lamp[lamp_no]) strcpy(colorcode,code_yellow); else  strcpy(colorcode,code_blue);
   if (lamp[lamp_no]) sprintf(name,"L%02d_off",lamp_no); else sprintf(name,"L%02d_on",lamp_no);
   sprintf(buffer,"<form action=\'\' method=\'post\'><input type=\'submit\' name=\'%s\' %s value=\'\n%s\n%s\' /> </form>\n" \
	,name,colorcode,"set ALL lamps","");
     sendit( sockfd, buffer);
   //special lamp 78, blinking lamps via thread
   lamp_no = 78;
   if (lamp[lamp_no]) strcpy(colorcode,code_yellow); else  strcpy(colorcode,code_blue);
   if (lamp[lamp_no]) sprintf(name,"L%02d_off",lamp_no); else sprintf(name,"L%02d_on",lamp_no);
   sprintf(buffer,"<form action=\'\' method=\'post\'><input type=\'submit\' name=\'%s\' %s value=\'\n%s\n%s\' /> </form>\n" \
        ,name,colorcode,"ALL lamps blink","");
     sendit( sockfd, buffer);
   sprintf(buffer,"<br>\n");
   sendit( sockfd, buffer);

   //how many lamps do we have
   switch ( lisy35_game.aux_lamp_variant )
   {
        case AS_2518_43_12_LAMPS: rows = 2; col = 6; break;
        case AS_2518_52_28_LAMPS: rows = 4; col = 7; break;
        case AS_2518_23_60_LAMPS: rows = 6; col = 10; break;
   }

   //send all the lamps together with the status
   //Name for Bally 35 starts with zero
   for(i=0; i<rows; i++)
   {
    for(j=0; j<col; j++)
    {
     lamp_no=i*col+j;
     if (lamp[lamp_no]) strcpy(colorcode,code_yellow); else  strcpy(colorcode,code_blue);
     if (lamp[lamp_no]) sprintf(name,"L%02d_off",lamp_no); else sprintf(name,"L%02d_on",lamp_no);
     // matiou: replaced <input> with <button> to allow 2 lines of text to describe lamp
     //sprintf(buffer,"<form action=\'\' method=\'post\'><input type=\'submit\' name=\'%s\' %s value=\'L%02d\n%s\n%s\' /> </form>\n",name,colorcode,lamp_no,lamp2_description_line1[lamp_no],lamp2_description_line2[lamp_no]);
     sprintf(buffer,"<form action=\'\' method=\'post\'><button type=\'submit\' name=\'%s\' %s >L%02d<BR/>%s<BR/>%s</button></form>\n",name,colorcode,lamp_no,lamp2_description_line1[lamp_no],lamp2_description_line2[lamp_no]);
     sendit( sockfd, buffer);
    }
   sprintf(buffer,"<br>\n");
   sendit( sockfd, buffer);
   }

}

void send_lamp_infos( int sockfd )
{
  int i,j,lamp_no;
  char colorcode[80],buffer[512],name[10];

  //colorcodes
  char *code_yellow = "style=\'BACKGROUND-COLOR:yellow; width: 125px; margin:auto; height: 5em;\'";
  char *code_blue = "style=\'BACKGROUND-COLOR:powderblue; width: 125px; margin:auto; height: 5em;\'";

  //set the active lampdriver
  active_lampdriver_board = 0;

     //basic info, header line
   send_basic_infos(sockfd);
     sprintf(buffer,"push button to switch lamp OFF or ON  Yellow lamps are ON<br><br>\n");
     sendit( sockfd, buffer);

   //special lamp 77, set all lamps
   lamp_no = 77;
   if (lamp[lamp_no]) strcpy(colorcode,code_yellow); else  strcpy(colorcode,code_blue);
   if (lamp[lamp_no]) sprintf(name,"L%02d_off",lamp_no); else sprintf(name,"L%02d_on",lamp_no);
   sprintf(buffer,"<form action=\'\' method=\'post\'><input type=\'submit\' name=\'%s\' %s value=\'\n%s\n%s\' /> </form>\n" \
	,name,colorcode,"set ALL lamps","");
     sendit( sockfd, buffer);
   //special lamp 78, blinking lamps via thread
   lamp_no = 78;
   if (lamp[lamp_no]) strcpy(colorcode,code_yellow); else  strcpy(colorcode,code_blue);
   if (lamp[lamp_no]) sprintf(name,"L%02d_off",lamp_no); else sprintf(name,"L%02d_on",lamp_no);
   sprintf(buffer,"<form action=\'\' method=\'post\'><input type=\'submit\' name=\'%s\' %s value=\'\n%s\n%s\' /> </form>\n" \
	,name,colorcode,"ALL lamps blink","");
     sendit( sockfd, buffer);
   sprintf(buffer,"<br>\n");
   sendit( sockfd, buffer);



   //send all the lamps together with the status
   //Name for Bally 35 starts with zero
   // we have 60 lamps 0...59
   for(i=0; i<=5; i++)
   {
    for(j=0; j<=9; j++)
    {
     lamp_no=i*10+j;
     if (lamp[lamp_no]) strcpy(colorcode,code_yellow); else  strcpy(colorcode,code_blue);
     if (lamp[lamp_no]) sprintf(name,"L%02d_off",lamp_no); else sprintf(name,"L%02d_on",lamp_no);
     // matiou: replaced <input> with <button> to allow 2 lines of text to describe lamp
     //sprintf(buffer,"<form action=\'\' method=\'post\'><input type=\'submit\' name=\'%s\' %s value=\'L%02d\r\n%s\n%s\' /> </form>\n",name,colorcode,lamp_no,lamp_description_line1[lamp_no],lamp_description_line2[lamp_no]);
     sprintf(buffer,"<form action=\'\' method=\'post\'><button type=\'submit\' name=\'%s\' %s >L%02d<BR/>%s<BR/>%s</button></form>\n",name,colorcode,lamp_no,lamp_description_line1[lamp_no],lamp_description_line2[lamp_no]);
     sendit( sockfd, buffer);
    }
   sprintf(buffer,"<br>\n");
   sendit( sockfd, buffer);
   }

}


void send_cont_solenoid_infos( int sockfd )
{
  int sol_no;
  char colorcode[80],buffer[512],name[20];

  //colorcodes
  char *code_yellow = "style=\'BACKGROUND-COLOR:yellow; width: 125px; margin:auto; height: 5em;\'";
  char *code_blue = "style=\'BACKGROUND-COLOR:powderblue; width: 125px; margin:auto; height: 5em;\'";

     //basic info, header line
   send_basic_infos(sockfd);
     sprintf(buffer,"push button to switch solenoid OFF or ON  Yellow solenoids are ON<br><br>\n");
     sendit( sockfd, buffer);
     sprintf(buffer,"Note: numbering is 16 .. 19 here, which might be different from what you as numbering in bally solenoid test<br><br>\n");
     sendit( sockfd, buffer);

   //send all the continuous solenoids together with the status
   //Name for LISY35 starts with 1 rather then zero, we have only four
    for(sol_no=1; sol_no<=4; sol_no++)
    {
     if (cont_sol[sol_no]) strcpy(colorcode,code_yellow); else  strcpy(colorcode,code_blue);
     if (cont_sol[sol_no]) sprintf(name,"O%02d_off",sol_no); else sprintf(name,"O%02d_on",sol_no);
     // matiou: replaced <input> with <button> to allow 2 lines of text to describe solenoid
     //sprintf(buffer,"<form action=\'\' method=\'post\'><input type=\'submit\' name=\'%s\' %s value=\'%02d \n%s\n%s\' /> </form>\n",name,colorcode,sol_no+15,coil_description_line1[sol_no+15],coil_description_line2[sol_no+15]);
     sprintf(buffer,"<form action=\'\' method=\'post\'><button type=\'submit\' name=\'%s\' %s >%02d<BR/>%s<BR/>%s</button></form>\n",name,colorcode,sol_no+15,coil_description_line1[sol_no+15],coil_description_line2[sol_no+15]);
     sendit( sockfd, buffer);
    }


}


void send_soption_infos( int sockfd )
{
  char buffer[512];
  char str[256];
  char message1[80];
  char message2[80];
  int i;
  FILE *fp;
  static int first = 1;


  //we do this only once
  if(first)
  {

  //first try to read sound opts, as we NEED them
  if ( lisy35_file_get_soundopts() < 0 )
     sprintf(message1,"error: no sound opts file; sound init failed<br>\n");
  else
     sprintf(message1,"info: sound opt file read OK<br>\n");
  //now open soundcard, and init soundstream
  if ( lisy35_sound_stream_init() < 0 )
     sprintf(message2,"error: sound init failed, sound emulation disabled<br>\n");
  else
   sprintf(message2,"info: sound init done<br>\n");

  // matiou: added line below to make sure init code runs only once
  first = 0;

  }//first call


  //start with some header
  // lets aplay give us the soundcards known to the system
  fp = popen("/usr/bin/aplay -l", "r");

   while ( fgets(str, sizeof(str)-1, fp) != NULL)
   {
     sprintf(buffer,"%s<br>\n",str);
     sendit( sockfd, buffer);
   }
   pclose(fp);

  //start with some header
  sendit( sockfd, "<br>");
  sendit( sockfd, message1);
  sendit( sockfd, message2);
  //set volume according to poti
  sprintf(buffer,"<br>info: Volume set to %d percent<br><br>\n",lisy_adjust_volume());
  sendit( sockfd, buffer);

  sprintf(buffer,"<p>  Say this text: <input type=\"text\" name=\"P1\" size=\"100\" maxlength=\"250\" value=\"\" /></p>");
  sendit( sockfd, buffer);
  sprintf(buffer,"<p>  Play from sound (decimal soundnumber): <input type=\"number\" max=\"255\" name=\"P2\" size=\"3\" maxlength=\"3\" value=\"\" /></p>");
  sendit( sockfd, buffer);
  sprintf(buffer,"<p>  to sound (decimal soundnumber): <input type=\"number\" max=\"255\" name=\"P3\" size=\"3\" maxlength=\"3\" value=\"\" /></p>");
  sendit( sockfd, buffer);
  sprintf(buffer,"<p><input type=\"submit\" /></p> ");
  sendit( sockfd, buffer);

  //print the sound mapping
  sendit( sockfd, "<br>these are the sound mappings: [hex][dec]<br>\n");
     for(i=1; i<=255; i++)
     {
       if ( lisy35_sound_stru[i].soundnumber != 0 )
       {
       sprintf(buffer,"[0x%02x] [%d]: %s/%s.wav %d %s<br>",i,i,lisy35_sound_stru[i].path,
                        lisy35_sound_stru[i].name,
                        lisy35_sound_stru[i].option,
                        lisy35_sound_stru[i].comment);
       sendit( sockfd, buffer);
       }
     }
}

void send_update_infos( int sockfd )
{
  char buffer[512];
  int ret_val;

  if ( update_path[0] != ' ')  //there was a setting of the update path
  {
    sprintf(buffer,"WE WILL NOW do the update<br><br>\n");
    sendit( sockfd, buffer);

    //set mode to read - write
    sprintf(buffer,"setting system mode to read/write<br><br>\n");
    sendit( sockfd, buffer);
    system("/bin/mount -o remount,rw /boot");
    system("/bin/mount -o remount,rw /");

    //clean up /home/pi/update
    sprintf(buffer,"clean up update dir<br><br>\n");
    sendit( sockfd, buffer);
    sprintf(buffer,"/bin/rm -rf /home/pi/update/*");
    ret_val = system(buffer);

    //try to get the update file with wget
    sprintf(buffer,"try to get the update file with wget<br><br>\n");
    sendit( sockfd, buffer);
    sprintf(buffer,"/usr/bin/wget %s -O /home/pi/update/lisy_update.tgz",update_path);
    ret_val = system(buffer);

    if (ret_val != 0)
    {
     sprintf(buffer,"return value of wget was %d<br><br>",ret_val);
     sendit( sockfd, buffer);
     sprintf(buffer,"update failed<br><br>");
     sendit( sockfd, buffer);
    }
    else  //just unpack the lisy_update.tgz and execute install.sh from within
    {
      sprintf(buffer,"try to get extract the update file<br><br>\n");
      sendit( sockfd, buffer);
      sprintf(buffer,"/bin/tar -xzf /home/pi/update/lisy_update.tgz -C /home/pi/update");
      ret_val = system(buffer);

      sprintf(buffer,"try to execute install.sh from within update pack<br><br>\n");
      sendit( sockfd, buffer);
      sprintf(buffer,"/bin/bash /home/pi/update/install.sh");
      ret_val = system(buffer);

      sprintf(buffer,"update done, you may want to reboot now<br><br>\n");
      sendit( sockfd, buffer);

    }

    //reset update path to prevent endless loop
    update_path[0] = ' ';


   }
  else  //normal header
  {

  //start with some header
  sprintf(buffer,"do an update, with the URL specified<br><br>\n");
  sendit( sockfd, buffer);
  sprintf(buffer,"NOTE: do only use URLs form lisy80.com<br><br>\n");
  sendit( sockfd, buffer);
  sprintf(buffer,"for latest update use: http://www.flipperkeller.de/lisy/lisy_update.tgz <br><br>\n");
  sendit( sockfd, buffer);

  sprintf(buffer,"<p>  Update path: <input type=\"text\" name=\"U\" size=\"100\" maxlength=\"250\" value=\"\" /></p>");
  sendit( sockfd, buffer);

  sprintf(buffer,"<p><input type=\"submit\" /></p> ");
  sendit( sockfd, buffer);
  }

}



void send_hostname_infos( int sockfd )
{
  char buffer[256];

  if ( hostname[0] != ' ')  //there was a setting of the hostname
  {
    sprintf(buffer,"WE WILL NOW SET the hostname of the system<br><br>\n");
    sendit( sockfd, buffer);

    //set mode to read - write
    sprintf(buffer,"setting system mode to read/write<br><br>\n");
    sendit( sockfd, buffer);
    system("/bin/mount -o remount,rw /boot");
    system("/bin/mount -o remount,rw /");

    //create new /etc/hosts
    sprintf(buffer,"creating a new /etc/hosts<br><br>\n");
    sendit( sockfd, buffer);
    system("/usr/bin/printf \"127.0.0.1\tlocalhost\n\" >/etc/hosts");
    sprintf(buffer,"/usr/bin/printf \"127.0.0.1\t%s\" >>/etc/hosts",hostname);
    system(buffer);

    //create new /etc/hostname
    sprintf(buffer,"creating a new /etc/hosts<br><br>\n");
    sendit( sockfd, buffer);
    sprintf(buffer,"/usr/bin/printf \"%s\n\" >/etc/hostname",hostname);
    system(buffer);

    //create new /etc/hostname
    sprintf(buffer,"now sync and rebooting<br><br>\n");
    sendit( sockfd, buffer);
    system("/bin/sync");
    sleep(2);
    system("/sbin/reboot");

    //reset hostname setting to prevent endless loop
    hostname[0] = ' ';
  }
  else  //normal header
  {

  //start with some header
  sprintf(buffer,"set the hostname of the system<br><br>\n");
  sendit( sockfd, buffer);
  sprintf(buffer,"NOTE: system will be rebooted afterwards!!<br><br>\n");
  sendit( sockfd, buffer);

  sprintf(buffer,"<p>  Hostname: <input type=\"text\" name=\"H\" size=\"32\" maxlength=\"32\" value=\"\" /></p>");
  sendit( sockfd, buffer);

  sprintf(buffer,"<p><input type=\"submit\" /></p> ");
  sendit( sockfd, buffer);
  }

}


void send_display_infos( int sockfd )
{
  char buffer[256];

  //start with some header
   send_basic_infos(sockfd);

 //see how many display we have
 switch ( lisy35_game.special_cfg)
 {
  case 0:     //6-digit
   sprintf(buffer,"This game has 5 display with 6 digits<br>\n");
   sendit( sockfd, buffer);
   //send 
   sprintf(buffer,"<p>  Status Display: <input type=\"text\" name=\"D0\" size=\"6\" maxlength=\"6\" value=\"%s\" /></p>",display_D0);
   sendit( sockfd, buffer);
   sprintf(buffer,"<p>Display Player 1: <input type=\"text\" name=\"D1\" size=\"6\" maxlength=\"6\" value=\"%s\" /></p>",display_D1);
   sendit( sockfd, buffer);
   sprintf(buffer,"<p>Display Player 2: <input type=\"text\" name=\"D2\" size=\"6\" maxlength=\"6\" value=\"%s\" /></p>",display_D2);
   sendit( sockfd, buffer);
   sprintf(buffer,"<p>Display Player 3: <input type=\"text\" name=\"D3\" size=\"6\" maxlength=\"6\" value=\"%s\" /></p>",display_D3);
   sendit( sockfd, buffer);
   sprintf(buffer,"<p>Display Player 4: <input type=\"text\" name=\"D4\" size=\"6\" maxlength=\"6\" value=\"%s\" /></p>",display_D4);
   sendit( sockfd, buffer);
	break;
  case 1:     //7-digit
  case 3:
  case 7:
   sprintf(buffer,"This game has 5 display with 7 digits<br>\n");
   sendit( sockfd, buffer);
   //send 
   sprintf(buffer,"<p>  Status Display: <input type=\"text\" name=\"D0A\" size=\"7\" maxlength=\"7\" value=\"%s\" /></p>",display_D0A);
   sendit( sockfd, buffer);
   sprintf(buffer,"<p>Display Player 1: <input type=\"text\" name=\"D1A\" size=\"7\" maxlength=\"7\" value=\"%s\" /></p>",display_D1A);
   sendit( sockfd, buffer);
   sprintf(buffer,"<p>Display Player 2: <input type=\"text\" name=\"D2A\" size=\"7\" maxlength=\"7\" value=\"%s\" /></p>",display_D2A);
   sendit( sockfd, buffer);
   sprintf(buffer,"<p>Display Player 3: <input type=\"text\" name=\"D3A\" size=\"7\" maxlength=\"7\" value=\"%s\" /></p>",display_D3A);
   sendit( sockfd, buffer);
   sprintf(buffer,"<p>Display Player 4: <input type=\"text\" name=\"D4A\" size=\"7\" maxlength=\"7\" value=\"%s\" /></p>",display_D4A);
   sendit( sockfd, buffer);
	break;
  case 2:     //6-digit 6-players
   sprintf(buffer,"This game has 7 display with 6 digits<br>\n");
   sendit( sockfd, buffer);
   //send 
   sprintf(buffer,"<p>  Status Display: <input type=\"text\" name=\"D0\" size=\"6\" maxlength=\"6\" value=\"%s\" /></p>",display_D0);
   sendit( sockfd, buffer);
   sprintf(buffer,"<p>Display Player 1: <input type=\"text\" name=\"D1\" size=\"6\" maxlength=\"6\" value=\"%s\" /></p>",display_D1);
   sendit( sockfd, buffer);
   sprintf(buffer,"<p>Display Player 2: <input type=\"text\" name=\"D2\" size=\"6\" maxlength=\"6\" value=\"%s\" /></p>",display_D2);
   sendit( sockfd, buffer);
   sprintf(buffer,"<p>Display Player 3: <input type=\"text\" name=\"D3\" size=\"6\" maxlength=\"6\" value=\"%s\" /></p>",display_D3);
   sendit( sockfd, buffer);
   sprintf(buffer,"<p>Display Player 4: <input type=\"text\" name=\"D4\" size=\"6\" maxlength=\"6\" value=\"%s\" /></p>",display_D4);
   sendit( sockfd, buffer);
   sprintf(buffer,"<p>Display Player 5: <input type=\"text\" name=\"D5\" size=\"6\" maxlength=\"6\" value=\"%s\" /></p>",display_D5);
   sendit( sockfd, buffer);
   sprintf(buffer,"<p>Display Player 6: <input type=\"text\" name=\"D6\" size=\"6\" maxlength=\"6\" value=\"%s\" /></p>",display_D6);
   sendit( sockfd, buffer);
	break;
  case 4:     //7-digit 5 players (extra status)
  case 5:
  case 6:
   sprintf(buffer,"This game has 6 display with 7 digits<br>\n");
   sendit( sockfd, buffer);
   //send 
   sprintf(buffer,"<p>  Status Display: <input type=\"text\" name=\"D0A\" size=\"7\" maxlength=\"7\" value=\"%s\" /></p>",display_D0A);
   sendit( sockfd, buffer);
   sprintf(buffer,"<p>Display Player 1: <input type=\"text\" name=\"D1A\" size=\"7\" maxlength=\"7\" value=\"%s\" /></p>",display_D1A);
   sendit( sockfd, buffer);
   sprintf(buffer,"<p>Display Player 2: <input type=\"text\" name=\"D2A\" size=\"7\" maxlength=\"7\" value=\"%s\" /></p>",display_D2A);
   sendit( sockfd, buffer);
   sprintf(buffer,"<p>Display Player 3: <input type=\"text\" name=\"D3A\" size=\"7\" maxlength=\"7\" value=\"%s\" /></p>",display_D3A);
   sendit( sockfd, buffer);
   sprintf(buffer,"<p>Display Player 4: <input type=\"text\" name=\"D4A\" size=\"7\" maxlength=\"7\" value=\"%s\" /></p>",display_D4A);
   sendit( sockfd, buffer);
   sprintf(buffer,"<p>Extra Status: <input type=\"text\" name=\"D5A\" size=\"7\" maxlength=\"7\" value=\"%s\" /></p>",display_D5A);
   sendit( sockfd, buffer);
	break;
 }
}



void send_switch_infos( int sockfd )
{
  int ret,i,j,switch_no;
  unsigned char action;
  char colorcode[80],buffer[512];

  //colorcodes
  char *code_red = "<td align=center style=\"background-color:red;\">";
  char *code_green = "<td align=center>";


  //update internal switch matrix with buffer from switch pic
  //swMatrix[0] is pinmame internal (sound?)
  //swMatrix[1..6] is bally,
  //swMatrix[7] is  'special switches' bit7:Test; bit6:S33;
 do
    {
     delay(1); // 1 millisecond delay from wiringpi library
               // for giving PIC some time to send switchcodes

     ret = lisy35_switch_reader( &action );

     //we need to add 1, as 0 is switch one(1)
     ret++;

     if (ret < 80) //ret is switchnumber: NOTE: Bally  8*6==48 switches in maximum, counting 01..48
        Switches_LISY35[ret] = action;
     }while( ret < 80);

     //now send whole matrix back together with some header
   send_basic_infos(sockfd);


     //printf 6 lines with 8 switches each (48 switches)
     for(i=0; i<=5; i++) //this is the line 
     {
      sprintf(buffer,"<tr style=\"background-color:lawngreen;\" border=\"1\">\n");
      sendit( sockfd, buffer);
       for(j=1; j<=8; j++) //this is the switch, we start with 1 
       { 
	switch_no = i*8 + j;
 	//assign color, red is closed, green is open, default fo table is green
	if ( !Switches_LISY35[switch_no]) strcpy(colorcode,code_green); else  strcpy(colorcode,code_red);
        sprintf(buffer,"%sSwitch %02d<br>%s<br>%s</td>\n",colorcode,switch_no,switch_description_line1[switch_no],switch_description_line2[switch_no]);

        sendit( sockfd, buffer);
	}
      sprintf(buffer,"</tr>\n");
      sendit( sockfd, buffer);
     }
}


//send infos for the homepage
void send_home_infos( int sockfd )
{
     char buffer[256];

   sprintf(buffer,"<h2>LISY35 Webeditor Home Page</h2> \n");
   sendit( sockfd, buffer);

   sprintf(buffer,"This is LISY35control version %d.%d<br>\n",LISY35control_SOFTWARE_MAIN,LISY35control_SOFTWARE_SUB);
   sendit( sockfd, buffer);
   send_basic_infos(sockfd);
   sprintf(buffer,"<p>\n<a href=\"./lisy35_switches.php\">Switches</a><br><br> \n");
   sendit( sockfd, buffer);
   sprintf(buffer,"<p>\n<a href=\"./lisy35_lamps.php\">Lamps (first board)</a><br><br> \n");
   sendit( sockfd, buffer);
   sprintf(buffer,"<p>\n<a href=\"./lisy35_lamps2.php\">Lamps (secondary board)</a><br><br> \n");
   sendit( sockfd, buffer);
   sprintf(buffer,"<p>\n<a href=\"./lisy35_mom_solenoids.php\">Momentary Solenoids</a><br><br> \n");
   sendit( sockfd, buffer);
   sprintf(buffer,"<p>\n<a href=\"./lisy35_cont_solenoids.php\">Continuous Solenoids</a><br><br> \n");
   sendit( sockfd, buffer);
   sprintf(buffer,"<p>\n<a href=\"./lisy35_displays.php\">Displays</a><br><br> \n");
   sendit( sockfd, buffer);
   sprintf(buffer,"<p>\n<a href=\"./lisy35_dipswitches.php\">DIP Switches</a><br><br> \n");
   sendit( sockfd, buffer);
if( lisy35_game.soundboard_variant == 2)
   sprintf(buffer,"<p>\n<a href=\"./lisy35_sound_ext.php\">Sound</a><br><br> \n");
else  sprintf(buffer,"<p>\n<a href=\"./lisy35_sound.php\">Sound</a><br><br> \n");
   sendit( sockfd, buffer);
   sprintf(buffer,"<p>\n<a href=\"./lisy_soundoption.php\">Control (optional) LISY onboard soundcard</a><br><br> \n");
   sendit( sockfd, buffer);
   sprintf(buffer,"<p>\n<a href=\"./lisy1_nvram.php\">NVRAM Information</a><br><br> \n");
   sendit( sockfd, buffer);
   sprintf(buffer,"<p>\n<a href=\"./lisy1_software.php\">Software installed</a><br><br> \n");
   sendit( sockfd, buffer);
   sprintf(buffer,"<p>\n<a href=\"./hostname.php\">Set the hostname of the system</a><br><br> \n");
   sendit( sockfd, buffer);
   sprintf(buffer,"<p>\n<a href=\"./update.php\">update System via internet </a><br><br> \n");
   sendit( sockfd, buffer);
   sprintf(buffer,"<p>\n<a href=\"./update_local.html\">update System with local tgz file</a><br><br> \n");
   sendit( sockfd, buffer);
   sprintf(buffer,"<p>\n<a href=\"./upload_35.html\">upload new lamp, coil or switch configuration files</a><br><br> \n");
   sendit( sockfd, buffer);
   sprintf(buffer,"<p>\n<a href=\"./lisy1_exit.php\">Exit and reboot</a><br><br> \n");
   sendit( sockfd, buffer);
}





//send software version(s)
void send_software_infos( int sockfd )
{
     char buffer[512];
     char versionstring[256];
     int dum;
     FILE *fp;

   //get installed version of lisy
   // Open the command for reading.
  fp = popen("/usr/local/bin/lisy -lisyversion", "r");
  if (fp == NULL) {
   sprintf(buffer,"LISY Version: unknown(internal error)\n");
   sendit( sockfd, buffer);
  }
  else
  {
   fgets(versionstring, sizeof(versionstring)-1, fp);
   sprintf(buffer,"LISY Version: %s<br>\n",versionstring);
   sendit( sockfd, buffer);
   pclose(fp);
  }

   //init switch pic and get Software version
   dum = lisy80_switch_pic_init();
   sprintf(buffer,"Software Switch PIC has version %d.%02d<br>\n",dum/100, dum%100);
   sendit( sockfd, buffer);

   //get Software version Display PIC
   dum = display_get_sw_version();
   sprintf(buffer,"Software Display PIC has version %d.%02d<br>\n",dum/100, dum%100);
   sendit( sockfd, buffer);

  //get Software version, Coil PIC
   dum = coil_get_sw_version();
   sprintf(buffer,"Software Coil PIC has version %d.%02d<br>\n",dum/100, dum%100);
   sendit( sockfd, buffer);

}

//send info from nvram
void send_nvram_infos( int sockfd )
{
     char buffer[256];
     int i;
     eeprom_block_t myblock;

   //give back the stats from the nvram
   myblock = lisy_eeprom_getstats();

 sprintf(buffer, "Content of second block of eeprom<br>\n\r");
 sendit( sockfd, buffer);
 sprintf(buffer, "Game number content first block: %d<br>\n\r",myblock.content.gamenr);
 sendit( sockfd, buffer);
 sprintf(buffer, "Number of starts: %d<br>\n\r",myblock.content.starts);
 sendit( sockfd, buffer);
 sprintf(buffer, "Number of starts with debug: %d<br>\n\r",myblock.content.debugs);
 sendit( sockfd, buffer);
 sprintf(buffer, "Number of starts, game specific if >0<br>\n\r");
 sendit( sockfd, buffer);
 for(i=0; i<=63; i++)
   {
     if(myblock.content.counts[i] > 0)
      {
       sprintf(buffer, "Number of starts Game No(%d): %d<br>\n\r",i,myblock.content.counts[i]);
       sendit( sockfd, buffer);
      }
   }
 sprintf(buffer, "Software version last used: %d.%03d<br>\n\r",myblock.content.Software_Main,myblock.content.Software_Sub);
 sendit( sockfd, buffer);

}

//do upload 'csv' files
void do_upload( int sockfd, char *what)
{

 char *destination, *source;
 char *line;
 char buffer[256];
 int ret_val;
 //we trust ASCII values
 //the format here is 'U_'

 line = &what[1];

 destination = strtok(line, ";");
 source = strtok(NULL, ";");

 sprintf(buffer,"<a href=\"./index.php\">Back to LISY35 Homepage</a><br><br>");
 sendit( sockfd, buffer);

 sprintf(buffer,"WE WILL NOW do the upload<br><br>\n");
 sendit( sockfd, buffer);

 //set mode to read - write
 sprintf(buffer,"setting system mode to read/write<br><br>\n");
 sendit( sockfd, buffer);
 system("/bin/mount -o remount,rw /boot");

 sprintf(buffer,"we copy file to %s<br><br>\n",destination);
 sendit( sockfd, buffer);

 sprintf(buffer,"/bin/cp %s %s",source,destination);
 ret_val = system(buffer);

 if (ret_val != 0)
    {
     sprintf(buffer,"return value of wget was %d<br><br>",ret_val);
     sendit( sockfd, buffer);
     sprintf(buffer,"upload failed<br><br>");
     sendit( sockfd, buffer);
    }
  else
    {
     sprintf(buffer,"upload done<br><br>");
     sendit( sockfd, buffer);
     sprintf(buffer,"we re-read the cofig files now<br><br>\n");
     sendit( sockfd, buffer);

    //re-read configurations
    get_switch_descriptions();
    get_lamp_descriptions();
    get_coil_descriptions();
    }

 //set mode to read only again
 sprintf(buffer,"setting system mode back to to read only<br><br>\n");
 sendit( sockfd, buffer);
 system("/bin/mount -o remount,ro /boot");

 sprintf(buffer,"all done<br><br>\n");
 sendit( sockfd, buffer);

}

 //dirty, dirty dirty
 //as we do not have a running pinmame
 //we need to identify the soundboard of the game
 //by searching the gamename in by35games.c
 void lisy35_set_soundboard_var(void)
 {

  FILE *fstream;
  char *line;
  char buffer[1024];  
  char *game_name_from_file;
  char *soundBoard_name_from_file;

  unsigned char no_match = 1;

  //lets filter the information we needed
  //with this cmmand we have a comma separated line
  //first field is gamename
  //sixt filed is Soundboard
  //filter games with INITGAME which are either with Chimes or normal soundbaord
  //system("/bin/grep 'INITGAME(' /home/pi/lisy/src/wpc/by35games.c |  awk -F'(' '{print $2$3}' >/tmp/bontango");
  //RTH new: prepared file in lisy35 folder on /boot


  //now open first filtered file
  fstream = fopen("/boot/lisy/lisy35/control/by35games","r");
   if(fstream == NULL)
   {
      fprintf(stderr,"\n LISY35_control: opening %s failed ","/boot/lisy/lisy35/control/by35games");
      core_gameData->hw.soundBoard = 0;
      return;
   }

   while( (line=fgets(buffer,sizeof(buffer),fstream))!=NULL)
   {
     game_name_from_file = strdup(strtok(line, ","));
     strdup(strtok(NULL, ",")); //skip
     strdup(strtok(NULL, ",")); //skip
     strdup(strtok(NULL, ",")); //skip
     strdup(strtok(NULL, ",")); //skip
     soundBoard_name_from_file = strdup(strtok(NULL, ","));
     //do we have a match?
     if ( strncmp( lisy35_game.gamename, game_name_from_file, strlen(lisy35_game.gamename)) == 0) 
       { 
         if ( strncmp( soundBoard_name_from_file, "0", 1) == 0) lisy35_game.soundboard_variant = LISY35_SB_CHIMES;
          else lisy35_game.soundboard_variant = LISY35_SB_STANDARD;
	 no_match = 0; 
	 break;
       }
   } //while
   fclose(fstream);

  //must be game with INITGAME2 which are with extended soundbaord
  if (no_match) lisy35_game.soundboard_variant = LISY35_SB_EXTENDED; 

  //debug?
   if ( ls80dbg.bitv.basic )
   {
    switch(lisy35_game.soundboard_variant)
    {
    case LISY35_SB_CHIMES:
     sprintf(debugbuf,"Info: LISY35 will use soundboard variant 0 (Chimes)");
     break;
    case LISY35_SB_STANDARD:
     sprintf(debugbuf,"Info: LISY35 will use soundboard variant 1 (standard SB)");
     break;
    case LISY35_SB_EXTENDED:
     sprintf(debugbuf,"Info: LISY35 will use soundboard variant 2 (EXTENDED SB)");
     break;
    }
    lisy80_debug(debugbuf);
   }
 }


//****** MAIN  ******


int main(int argc, char *argv[])
{
     int sockfd, newsockfd, portno;
     socklen_t clilen;
     char buffer[256];
     char ip_interface[10];
     struct sockaddr_in serv_addr, cli_addr, *myip;
     int i,n,res;
     int do_exit = 0;
     struct ifreq ifa;
     char *line;
     int tries = 0;
     char lisy_variant[20];
     char lisy_gamename[20];

     //init ars
     core_gameData = malloc(sizeof *core_gameData);

     //init switch description
     for (i=0; i<80; i++)
      {
         strcpy ( switch_description_line1[i], "NOT SET");
         strcpy ( switch_description_line2[i], "");
      }



     //check which pinball we are going to control
     //this will also call lisy_hw_init
     strcpy(lisy_variant,"lisy35");
     if ( (res = lisy_set_gamename(lisy_variant, lisy_gamename)) != 0)
           {
             fprintf(stderr,"LISY35: no matching game or other error\n\r");
             return (-1);
           }

    //use the init functions from lisy.c
    lisy_init();

    //dirty, dirty dirty
    lisy35_set_soundboard_var();

    //init lamps
    lisy35_lamp_init( );

    //init coils
    lisy35_coil_init( );

    //collect latest informations and start the lisy logger
    strcpy(lisy_env.variant,"LISY35_control");
    lisy_logger();

    //init internal lamp vars as well
    for(i=0; i<=59; i++) lamp[i] = 0;
    for(i=0; i<=59; i++) lamp2[i] = 0;

    //init internal continous solenoid vars as well
    for(i=0; i<=4; i++) cont_sol[i] = 0;

    //init sound
    //init internal sound vars as well
    for(i=0; i<=31; i++) sound[i] = 0;



    //read the descriptions for the switches
    get_switch_descriptions();
    //read the descriptions for the lamps
    get_lamp_descriptions();
    get_lamp2_descriptions();
    //read the descriptions for the coils
    get_coil_descriptions();


 // try say something about LISY80 if sound is requested
 if ( ls80opt.bitv.JustBoom_sound )
 {
  //set volume according to poti
  sprintf(debugbuf,"/bin/echo \"Welcome to LISY 35 Control Version 0.%d\" | /usr/bin/festival --tts",LISY35control_SOFTWARE_SUB);
  system(debugbuf);
 }

     sockfd = socket(AF_INET, SOCK_STREAM, 0);
     if (sockfd < 0) 
        error("ERROR opening socket");


     //try to find out our IP on eth0
     strcpy (ifa.ifr_name, "eth0");
     strcpy (ip_interface, "ETH0"); //upercase for message
     if((n=ioctl(sockfd, SIOCGIFADDR, &ifa)) != 0) 
      {
	//no IP on eth0, we try wlan0 now, 20 times
        strcpy (ifa.ifr_name, "wlan0");
        strcpy (ip_interface, "WLAN0"); //upercase for message
	do
        {
	  sleep(1);
          n=ioctl(sockfd, SIOCGIFADDR, &ifa);
	  tries++;
	  if ( ls80dbg.bitv.basic )
	   {
             sprintf(debugbuf,"get IP of wlan0: try number:%d",tries);
             lisy80_debug(debugbuf);
           }
        } while ( (n!=0) &( tries<20));
      }

     if(n) //no IP found
     {
        strcpy (ifa.ifr_name, "noip");
       //construct the message
        fprintf(stderr,"Bally NO IP");
        display35_show_str( 1, "000000");
        display35_show_str( 2, "000000");
        display35_show_str( 3, "000000");
        display35_show_str( 4, "000000");
     }
     else //we found an IP
     {
      myip = (struct sockaddr_in*)&ifa.ifr_addr;
	//get teh pouinter to teh Ip address
        line = inet_ntoa(myip->sin_addr);
	//split the ip to four displays and store value for display routine
        sprintf(buffer,"%-6s",strtok(line, "."));
        display35_show_str( 1, buffer); //strcpy(display_D1,buffer); strcpy(display_D1A,buffer);
       fprintf(stderr,"%s\n",buffer);
        sprintf(buffer,"%-6s",strtok(NULL, "."));
        display35_show_str( 2, buffer); //strcpy(display_D2,buffer); strcpy(display_D2A,buffer);
       fprintf(stderr,"%s\n",buffer);
        sprintf(buffer,"%-6s",strtok(NULL, "."));
        display35_show_str( 3, buffer); //strcpy(display_D3,buffer); strcpy(display_D3A,buffer);
       fprintf(stderr,"%s\n",buffer);
        sprintf(buffer,"%-6s",strtok(NULL, "."));
        display35_show_str( 4, buffer); //strcpy(display_D4,buffer); strcpy(display_D4A,buffer);
       fprintf(stderr,"%s\n",buffer);
     }


     //now set up the socketserver
     bzero((char *) &serv_addr, sizeof(serv_addr));
     portno = 5963;
     serv_addr.sin_family = AF_INET;
     serv_addr.sin_addr.s_addr = INADDR_ANY;
     serv_addr.sin_port = htons(portno);
     if (bind(sockfd, (struct sockaddr *) &serv_addr,
              sizeof(serv_addr)) < 0) 
              error("ERROR on binding");
     listen(sockfd,5);
     clilen = sizeof(cli_addr);

     //wait and listen
    do {
     newsockfd = accept(sockfd, 
                 (struct sockaddr *) &cli_addr, 
                 &clilen);
     if (newsockfd < 0) 
          error("ERROR on accept");
     bzero(buffer,256);
     n = read(newsockfd,buffer,255);
     if (n < 0) error("ERROR reading from socket");

     //the home screen woth all available commands
     if ( strcmp( buffer, "home") == 0) { send_home_infos(newsockfd); close(newsockfd); }
     //software used, send all the infos to teh webserver
     else if ( strcmp( buffer, "software") == 0) { send_software_infos(newsockfd); close(newsockfd); }
     //nvra stats, send all the infos to teh webserver
     else if ( strcmp( buffer, "nvram") == 0) { send_nvram_infos(newsockfd); close(newsockfd); }
     //overview & control switches, send all the infos to teh webserver
     else if ( strcmp( buffer, "switches") == 0) { send_switch_infos(newsockfd); close(newsockfd); }
     //overview & control lamps, send all the infos to teh webserver
     else if ( strcmp( buffer, "lamps") == 0) { send_lamp_infos(newsockfd); close(newsockfd); }
     //overview & control lamps, send all the infos to teh webserver (second lampdriverboard)
     else if ( strcmp( buffer, "lamps2") == 0) { send_lamp2_infos(newsockfd); close(newsockfd); }
     //overview and control solenoids, send all the infos to teh webserver
     else if ( strcmp( buffer, "mom_solenoids") == 0) { send_mom_solenoid_infos(newsockfd); close(newsockfd); }
     //overview and cobtrol sounds, send all the infos to teh webserver
     else if ( strcmp( buffer, "cont_solenoids") == 0) { send_cont_solenoid_infos(newsockfd); close(newsockfd); }
     //overview and cobtrol sounds, send all the infos to teh webserver
     else if ( strcmp( buffer, "sounds") == 0) { send_sound_infos(newsockfd); close(newsockfd); }
     //internal dip-switch emulation, send all the infos to teh webserver
     else if ( strcmp( buffer, "dipsswitches") == 0) { send_dipswitch_infos(newsockfd); close(newsockfd); }
     //overview & control displays, send all the infos to teh webserver
     else if ( strcmp( buffer, "displays") == 0) { send_display_infos(newsockfd); close(newsockfd); }
     //provide simple input field for setting hostname other then the default
     else if ( strcmp( buffer, "hostname") == 0) { send_hostname_infos(newsockfd); close(newsockfd); }
     //provide simple input field for initiating update
     else if ( strcmp( buffer, "update") == 0) { send_update_infos(newsockfd); close(newsockfd); }
     //provide simple input field for for controlling optional LISY soundcard
     else if ( strcmp( buffer, "soption") == 0) { send_soption_infos(newsockfd); close(newsockfd); }
     //should we exit?
     else if ( strcmp( buffer, "exit") == 0) do_exit = 1;
     //we interpret all Messages with an uppercase 'V' as dip settings
     else if (buffer[0] == 'V') do_dip_set(buffer);
     //we interpret all Messages with an uppercase 'K' as dip comment settings
     else if (buffer[0] == 'K') do_dip_set(buffer);
     //we interpret all Messages with an uppercase 'L' as lamp settings
     else if (buffer[0] == 'L') do_lamp_set(buffer);
     //we interpret all Messages with an uppercase 'S' as sound settings
     else if (buffer[0] == 'S') do_sound_set(buffer);
     //we interpret all Messages with an uppercase 'E' as Extended sound settings
     else if (buffer[0] == 'E') do_ext_sound_set(buffer);
     //we interpret all Messages with an uppercase 'C' as coil (solenoid) settings
     else if (buffer[0] == 'C') do_mom_solenoid_set(buffer);
     //we interpret all Messages with an uppercase 'C' as continuous coil (solenoid) settings
     else if (buffer[0] == 'O') do_cont_solenoid_set(buffer);
     //we interpret all Messages with an uppercase 'D' as display settings
     else if (buffer[0] == 'D') do_display_set(buffer);
     //with an uppercase 'H' we do setting a new hostname to the system and reboot
     else if (buffer[0] == 'H') do_hostname_set(buffer);
     //with an uppercase 'U' we do try to initiate an update of lisy
     else if (buffer[0] == 'U') do_updatepath_set(buffer);
    //with an uppercase 'X' we do try to initiate upload of csv files
     else if (buffer[0] == 'X') { do_upload(newsockfd,buffer);close(newsockfd); }
    //with an uppercase 'Y' we do update the system with clientfile
     else if (buffer[0] == 'Y') { do_update_local(newsockfd,buffer);close(newsockfd); }
    //with an uppercase 'P' we control the sound oPtion
     else if (buffer[0] == 'P') { do_soption_set(buffer); }
     //as default we print out what we got
     else fprintf(stderr,"Message: %s\n",buffer);


  } while (do_exit == 0);

     close(newsockfd);
     close(sockfd);
     //menu says: exit and reboot: so do it
     system("/sbin/reboot");
     return 0; 
}

