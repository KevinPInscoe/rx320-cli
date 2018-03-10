/*
 * Seven RX 320 commands
 */

/* for volume command (output selection value) */
#define RX320SPEAKER 1
#define RX320LINE 2
#define RX320BOTH 3

int rx320mode(char *mode);
int rx320filter(int filt);
int rx320volume(int output,int vol);
int rx320agc(char *agc);
int rx320frequency(float freq);
int rx320signal();
int rx320version();
