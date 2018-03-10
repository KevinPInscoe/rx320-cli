/*
 * Ten Tec RX 320 control.
 *
 * A. Maitland Bottoms <aa4hs@amrad.org>
 * November 2000
 * GPL
 */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <argp.h>
#include <math.h>
#include <time.h>
#include "rx320cli.h"

const char *argp_program_version="rx320cli 2.0";
const char *argp_program_bug_address="<kevin@inscoe.org>";
static char doc[]="rx320 control program";

int Mcor=0;
int Fcor=0;
int Cbfo=0;
FILE *rx320tty;
int sendcmd(char *cmd,int len);
int getresponse(char *buf,int n,char term);

/*
 * MODE: AM USB LSB CW
 */
int
rx320mode(char *mode)
{
  char mcmd[3];

  mcmd[0] = 'M';
  mcmd[2] = 0x0d;
  switch (tolower(mode[0])) {
  case 'u':
    Mcor = 1;  Cbfo = 0; mcmd[1]='1'; break;
  case 'l':
    Mcor = -1; Cbfo = 0; mcmd[1]='2'; break;
  case 'c':
    Mcor = -1; Cbfo = 1000; mcmd[1]='3'; break;
  default:
  case 'a':
    Mcor = 0; mcmd[1]='0'; break;
    break;
  }
  return(sendcmd(mcmd,3));
}

/*
 * FILTER: (34 of them)
 */
typedef struct {
  int bandwidth;
  int filter;
} FEntry;

FEntry Filters[] = {
  {8000, 33},
  {6000, 0},
  {5700, 1},
  {5400, 2},
  {5100, 3},
  {4800, 4},
  {4500, 5},
  {4200, 6},
  {3900, 7},
  {3600, 8},
  {3300, 9},
  {3000, 10},
  {2850, 11},
  {2700, 12},
  {2550, 13},
  {2400, 14},
  {2250, 15},
  {2100, 16},
  {1950, 17},
  {1800, 18},
  {1650, 19},
  {1500, 20},
  {1350, 21},
  {1200, 22},
  {1050, 23},
  {900, 24},
  {750, 25},
  {675, 26},
  {600, 27},
  {525, 28},
  {450, 29},
  {375, 30},
  {330, 31},
  {300, 32},
  {0, 32}
};

int
rx320filter(int filt)
{
  char fcmd[3];
  FEntry *fe;
  if (filt>34) {
    /* Allow selection by kHz as well as number */
    for (fe = Filters; fe->bandwidth>0; fe++)
      if (filt>fe->bandwidth) break;
    filt = fe->filter;
  };
  fcmd[0] = 'W';
  fcmd[1] = filt;
  fcmd[2] = 0x0d;

  /* Need to set Fcor */
  for (fe = Filters; filt!=fe->filter; fe++);
  Fcor=(fe->bandwidth/2)+200;
  return(sendcmd(fcmd,3));
}

int
listrx320filters()
{
   FEntry *fe;

   printf("Filter Number\tFilter Bandwidth\n");
   for (fe =Filters; fe->bandwidth>0; fe++)
	   printf("%d\t\t%d\n",fe->filter,fe->bandwidth);
   return(0);
}

/*
 * VOLUME: (speaker,line-out,both) 0-63
 */
int
rx320volume(int output,int vol)
{
  char vcmd[4];
  switch (output) {
  case RX320SPEAKER: vcmd[0] = 'V'; break;
  case RX320LINE: vcmd[0] = 'A'; break;
  case RX320BOTH:
  default:
    vcmd[0] = 'C'; break;
  }
  vcmd[1]=0; vcmd[3]=0x0d;
  if ((vol>=0) && (vol<64)) {
    vcmd[2] = vol;
    return(sendcmd(vcmd,4));
  }
  return(0);
}

/*
 * AGC: SLOW MEDIUM FAST
 */
int
rx320agc(char *agc)
{
  char agccmd[3];

  agccmd[0]='G';
  switch (tolower(agc[0])) {
  case 's': agccmd[1] = '1'; break;
  case 'm': agccmd[1] = '2'; break;
  case 'f': agccmd[1] = '3'; break;
  default:  agccmd[1] = '3'; break;
  }
  agccmd[2]=0x0d;
  return(sendcmd(agccmd,3));
}

/*
 * FREQUENCY: COARSE FINE BFO
 */
int rx320frequency(float freq)
{
  char fcmd[8];
  double intpart;
  float AdjTfreq; /* Adjusted Tuned Frequency */
  int Atf; /* Integer AdjTfreq */
  int Ctf; /* Coarse tuning factor */
  int Ftf; /* Fine Tuning Factor */
  int Btf; /* BFO Tuning factor */

  AdjTfreq = freq - 1.25 + (Mcor*(Fcor+Cbfo))/1000.0;
  Atf = AdjTfreq;
  Ctf = AdjTfreq/2.5+18000;
  Ftf = modf(AdjTfreq/2.5,&intpart)*2500*5.46;
  Btf = (Fcor+Cbfo+8000)*2.73;

  fcmd[0] = 'N';
  fcmd[1] = 0xFF&(Ctf>>8);
  fcmd[2] = 0xFF&Ctf;
  fcmd[3] = 0XFF&(Ftf>>8);
  fcmd[4] = 0xFF&Ftf;
  fcmd[5] = 0xFF&(Btf>>8);
  fcmd[6] = 0xFF&Btf;
  fcmd[7] = 0x0d;
  return(sendcmd(fcmd,8));
}

/*
 * SIGNAL STRENGTH
 */
int
rx320signal()
{
  char scmd[2];
  unsigned char response[4];
  unsigned int hi, lo, i;

  scmd[0] = 'X';
  scmd[1] = 0x0d;
  sendcmd(scmd,2);
  for (i=0; i<4; i++)
    response[i] = fgetc(rx320tty);
  response[i] = 0;
  if (response[0]=='X') {
    hi = response[1];
    lo = response[2];
  }
  return(256*hi+lo);
}

/*
 * FIRMWARE REVISION
 */
int
rx320version()
{
  char vcmd[2];
  char vers[80];

  vcmd[0]='?';
  vcmd[1]=0x0d;
  sendcmd(vcmd,2);
  getresponse(vers,80,0x0d);
  printf("%s\n",vers);
  return(0);
}

/*
 * Send command
 */
int
sendcmd(char *cmd,int len)
{
  int i;

  for (i=0;i<len;i++)
    fputc(cmd[i],rx320tty);
  fflush(rx320tty);
  return(0);
}

int
getresponse(char *buf,int n,char term)
{
  int i;

  for (i=0; i<n; i++) {
    buf[i] = fgetc(rx320tty);
    if (buf[i] == term) break;
  }
  buf[i] = 0;
  return(i);
}

FILE *
rx320serial(char *port)
{
  int fd;
  FILE *serport;
  struct termios ts;
  int off = 0;
  int modemlines;

  fd = open(port,O_RDWR|O_NOCTTY|O_NONBLOCK,0);
  if (!isatty(fd)) {
    fprintf(stderr,"Oops, need to connect to a tty\n");
    return(0); /* nothing to do */
  }
  if (tcgetattr(fd,&ts)==-1) {
    fprintf(stderr,"Oops, didn't get tty settings\n");
    return(0);
  }
  cfmakeraw(&ts);
  ts.c_iflag = IGNBRK; /* | IGNCR; */
  ts.c_oflag = 0;
  ts.c_cflag = CS8 | CREAD | CLOCAL;
  ts.c_lflag = 0;
  ts.c_cc[VMIN] = 1;
  ts.c_cc[VTIME] = 0;
  if (cfsetospeed(&ts, B1200)==-1) {
    perror("output speed");
    return(0);
  }
  if (cfsetispeed(&ts, B1200)==-1) {
    perror("input speed");
    return(0);
  }
  if (tcsetattr(fd,TCSAFLUSH,&ts)==-1) {
    perror("serial_port_setup");
    return(0);
  }
  /* Set the line back to blocking mode after setting CLOCAL. */
  if (ioctl(fd, FIONBIO, &off) < 0) {
    perror("setting blocking mode");
    return(0);
  } 
  modemlines = TIOCM_RTS; 
  if (ioctl(fd, TIOCMBIC, &modemlines)) {
    perror("clear RTS line");
    return(0);
  }
  serport = fdopen(fd,"r+");
  return(serport);
}

static char args_doc[] = "PORT";
static struct argp_option options[] = {
  {"mode", 'm', "MODE", 0, "AM, USB, LSB, CW" },
  {"bandwidth", 'b', "BANDWIDTH", 0, "Filter bandwidth in HZ, or number 0-33" },
  {"frequency", 'f', "FREQUENCY", 0, "Receive Frequency" },
  {"volume", 'v', "VOLUME", 0, "Audio output volume (both outputs)" },
  {"speaker", 's', "VOLUME", 0, "Audio output volume (speaker only)" },
  {"line", 'l', "VOLUME", 0, "Audio output volume (line only)" },
  {"agc", 'a', "AGC", 0, "Automatic Gain Control mode (slow, fast, medium)" },
  {"Version", 'V', 0, 0, "Report Firmware revision of receiver" },
  {"signal", 'S', 0, 0, "Report recieved signal strength" },
  {"meter", 'M', "DELAY", 0, "Repeatedly report received signal strength" },
  {"list-filters", 'L', 0, 0, "List filter numbers and bandwidths" },
  {"debug", 'd', 0, 0, "Debug - be verbose" },
  {0}
};

struct arguments {
  char *args[2];
  char *mode;
  int bandwidth;
  float frequency;
  int volume;
  int setvolume;
  int output_select;
  char *agc;
  int setagc;
  int setmode;
  int setfreq;
  int setfilt;
  int version;
  int signal;
  int dometer;
  int delay;
  int listfilters;
  int debug;
};

static error_t
parse_opt(int key, char *arg, struct argp_state *state)
{
  struct arguments *arguments = state->input;

  switch(key) {
  case 'm':
    arguments->mode = arg;
    arguments->setmode = 1;
    break;
  case 'b':
    arguments->bandwidth = atoi(arg);
    arguments->setfilt = 1;
    break;
  case 'f':
    arguments->frequency = atof(arg);
    arguments->setfreq = 1;
    break;
  case 'v': 
    arguments->volume = atoi(arg);
    arguments->output_select=RX320BOTH;
    arguments->setvolume=1;
    break;
  case 's':
    arguments->volume = atoi(arg);
    arguments->output_select=RX320SPEAKER;
    arguments->setvolume=1;
    break;
  case 'l':
    arguments->volume = atoi(arg);
    arguments->output_select=RX320LINE;
    arguments->setvolume=1;
    break;
  case 'a':
    arguments->agc = arg;
    arguments->setagc=1;
    break;
  case 'M':
    arguments->delay = atoi(arg);
    arguments->dometer=1;
    break;
  case 'd': arguments->debug = 1; break;
  case 'L': listrx320filters(); break;
  case 'V': arguments->version = 1; break;
  case 'S': arguments->signal = 1; break;
  case ARGP_KEY_ARG:
    if (state->arg_num>1) argp_usage(state);
    arguments->args[state->arg_num] = arg;
    break;
  case ARGP_KEY_END:
    if (state->arg_num<1) argp_usage(state);
  default:
    return(ARGP_ERR_UNKNOWN);
  }
  return(0);
}

static struct argp argp = {options,parse_opt, args_doc, doc};

int
main(int argc,char *argv[])
{
  time_t timestamp;
  struct arguments arguments;

  /* Default values */
  arguments.mode = "AM";
  arguments.bandwidth = 6000;
  arguments.frequency = 1500.0;
  arguments.volume = 0;
  arguments.output_select = RX320BOTH;
  arguments.agc = "SLOW";
  arguments.setvolume = 0;
  arguments.setagc = 0;
  arguments.setmode = 0;
  arguments.setfilt = 0;
  arguments.setfreq = 0;
  arguments.version = 0;
  arguments.signal = 0;
  arguments.dometer = 0;
  arguments.delay = 0;
  arguments.listfilters = 0;
  arguments.debug = 0;

  argp_parse(&argp,argc,argv,0,0,&arguments);
  if (arguments.debug) {
    fprintf(stderr,"PORT=%s\n",arguments.args[0]);
    if (arguments.setmode)
      fprintf(stderr,"mode=%s\n",arguments.mode);
    if (arguments.setfilt)
      fprintf(stderr,"filter bandwidth=%d\n",arguments.bandwidth);
    if (arguments.setfreq)
      fprintf(stderr,"frequency=%f\n",arguments.frequency);
    if (arguments.setvolume)
      fprintf(stderr,"volume=%d (%d)\n",arguments.volume,arguments.output_select);
    if (arguments.setagc)
      fprintf(stderr,"agc=%s\n",arguments.agc);
    if (arguments.dometer)
      fprintf(stderr,"meter delay=%d seconds\n",arguments.delay);
  }
  if (arguments.args[0][0] == '-')
    rx320tty=stdout;
  else
    rx320tty = rx320serial(arguments.args[0]);
  if (arguments.setfreq && arguments.setmode && arguments.setfilt ) {
    rx320mode(arguments.mode);
    rx320filter(arguments.bandwidth);
    rx320frequency(arguments.frequency);
  } else if (arguments.setfreq || arguments.setmode || arguments.setfilt )
    fprintf(stderr,"Need to specify all of (frequency, mode, filter bandwidth)\n");
  if (arguments.setagc) rx320agc(arguments.agc);
  if (arguments.setvolume) rx320volume(arguments.output_select,arguments.volume);
  if (arguments.version) rx320version();
  if (arguments.signal) printf("Signal strength %d\n",rx320signal());
  if (arguments.dometer)
    while(1) {
      timestamp=time(0);
      printf("%d",rx320signal());
      if (arguments.delay>60) {
	printf("\t%s",ctime(&timestamp));
      } else
	printf("\n");
      sleep(arguments.delay);
    }
  return(0);
}
