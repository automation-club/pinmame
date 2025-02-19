// license:BSD-3-Clause

#include "driver.h"
#include "machine/6821pia.h"
#include "cpu/m6800/m6800.h"
#include "cpu/m68000/m68000.h"
#include "cpu/m6809/m6809.h"
#include "sound/tms5220.h" // uses TMS5200
#include "core.h"
#include "sndbrd.h"
#include "by35.h"
#include "by35snd.h"

// sample support added for by32 / by50 by Oliver Kaegi (08/27/2004)
//
// sample table for both 82s123 prom from the by32 / by50 sound card...
//
static const char *u318_sample_names[] =
{
	"*s3250u3",
	"w1801.wav",
	"w1802.wav",
	"w1803.wav",
	"w1804.wav",
	"w1805.wav",
	"w1806.wav",
	"w1807.wav",
	"w1808.wav",
	"w1809.wav",
	"w1810.wav",
	"w1811.wav",
	"w1812.wav",
	"w1813.wav",
	"w1814.wav",
	"w1815.wav",
	"w1815.wav",
	"w1817.wav",
	"w1818.wav",
	"w1819.wav",
	"w1820.wav",
	"w1821.wav",
	"w1822.wav",
	"w1823.wav",
	"w1824.wav",
	"w1825.wav",
	"w1826.wav",
	"w1827.wav",
	"w1828.wav",
	"w1829.wav",
	"w1830.wav",
	"w1831.wav",
	"w1831.wav",
	"w5101.wav",
	"w5102.wav",
	"w5103.wav",
	"w5104.wav",
	"w5105.wav",
	"w5106.wav",
	"w5107.wav",
	"w5108.wav",
	"w5109.wav",
	"w5110.wav",
	"w5111.wav",
	"w5112.wav",
	"w5113.wav",
	"w5114.wav",
	"w5115.wav",
	"w5115.wav",
	"w5117.wav",
	"w5118.wav",
	"w5119.wav",
	"w5120.wav",
	"w5121.wav",
	"w5122.wav",
	"w5123.wav",
	"w5124.wav",
	"w5125.wav",
	"w5126.wav",
	"w5127.wav",
	"w5128.wav",
	"w5129.wav",
	"w5130.wav",
	"w5131.wav",
	"w5131.wav",
	0   /* end of array */
};

struct Samplesinterface u318_samples_interface =
{
	3,
	50,	/* volume */
	u318_sample_names,
	"Bally sounds"
};


/*----------------------------------------
/              -32, -50 sound
/-----------------------------------------*/
static void by32_init(struct sndbrdData *brdData);
static WRITE_HANDLER(by32_data_w);
static WRITE_HANDLER(by32_ctrl_w);
static WRITE_HANDLER(by32_manCmd_w);
static int by32_sh_start(const struct MachineSound *msound);
static void by32_sh_stop(void);

/*-------------------
/ exported interface
/--------------------*/
const struct sndbrdIntf by32Intf = {
  "BY32", by32_init, NULL, NULL, by32_manCmd_w, by32_data_w, NULL, by32_ctrl_w, NULL, SNDBRD_NODATASYNC|SNDBRD_NOCTRLSYNC
};
static struct CustomSound_interface by32_custInt = {by32_sh_start, by32_sh_stop};

MACHINE_DRIVER_START(by32)
  MDRV_SOUND_ADD(CUSTOM, by32_custInt)
  MDRV_SOUND_ADD(SAMPLES, samples_interface)
  MDRV_SOUND_ADD_TAG("BY_32_50", SAMPLES, u318_samples_interface)
MACHINE_DRIVER_END


static struct {
  struct sndbrdData brdData;
  int  startit, lastCmd,  strobe, sampleoff;
} by32locals;


static int by32_sh_start(const struct MachineSound *msound)  {
  mixer_allocate_channel(0);
  return 0;
}


static void by32_sh_stop(void) {
}



// table to play samples

//by32_ctrl_w         	by32_data_w
//    cb2 e  		a-d    		prev-cb2    prev-a-d   		result (dont play if 1111)
// a) 0   X   		 		X		  		fill prev cb2
// b) 1   X            			0        	0001      	play X0001
// c)        		1100    	1       	0001       	play X1100 fill prev a-d
// d)     		0001		0		XXXX 		fill prev a-d
// e) 1   x                             1                               ignore
// X -> dont care

static int lastchan = 0;
static void playsam(int cmd) {
   int i;
  if ((cmd != by32locals.lastCmd) && ((cmd & 0x0f) != 0x0f)) {

       int samplex;
        samplex = by32locals.sampleoff + (cmd & 0x1f);
//  logerror("%04x: samplestart cmd %02x alstcmd %02x %d \n", activecpu_get_previouspc(), cmd,by32locals.lastCmd,samplex);

// 	if (by32locals.startit == 0)
// 	{
// 		sample_start(0,samplex,0);
// 		by32locals.startit = 1;
// 	}
// 	else
// 	{
		lastchan++;
		if (lastchan > 2) lastchan = 0;
  		sample_start(6+lastchan,samplex,0);
		//  		by32locals.startit = 0;
//	}
//       sample_start(0,samplex,0);
  } else   if ((cmd & 0x0f) == 0x0f)     {
	   for (i = 0;i < 3;i++) {
           if (i != lastchan) sample_stop(6+i);
        }
         }
  by32locals.lastCmd = cmd;
}

static WRITE_HANDLER(by32_data_w) {
      	logerror("%04x: by_data_w data %02x \n", activecpu_get_previouspc(), data);
      	if (~by32locals.strobe & 0x01)
 	{
 		by32locals.lastCmd =	(by32locals.lastCmd & 0x10) | (data & 0x0f); // case d
 	}
 	else
 	{
       		playsam((by32locals.lastCmd & 0x10) | (data & 0x0f)); 		// case c
       	}
}
static WRITE_HANDLER(by32_ctrl_w) {
int i;
  if (~by32locals.strobe & 0x01)
	{
 //        playsam((by32locals.lastCmd & 0x0f) | ((data & 0x02) ? 0x10 : 0x00)); // case b
// sound e bit is swaped !!!!
          playsam((by32locals.lastCmd & 0x0f) | ((data & 0x02) ? 0x00 : 0x10)); // case b
	}
  else
    	if (~data & 0x01)
           {
// 	   by32locals.lastCmd = (by32locals.lastCmd & 0x0f) | ((data & 0x02) ? 0x10 : 0x00); // case a
  	   by32locals.lastCmd = (by32locals.lastCmd & 0x0f) | ((data & 0x02) ? 0x00 : 0x10); // case a
	   for (i = 0;i < 3;i++) {
           if (i != lastchan) sample_stop(6+i);
        }

           }

  logerror("%04x: by_ctrl32_w data %02x startit %d\n", activecpu_get_previouspc(), data,by32locals.startit);
  by32locals.strobe = data;	// case e
}

static WRITE_HANDLER(by32_manCmd_w) {
  by32_data_w(0, data); by32_ctrl_w(0,0); by32_ctrl_w(0,((data & 0x10)>>3)|0x01);
}
static void by32_init(struct sndbrdData *brdData) {
  int w;
   memset(&by32locals, 0, sizeof(by32locals));
  by32locals.brdData = *brdData;
  w = core_revbyte(*(by32locals.brdData.romRegion));
    if (w == 0xfe)
    {
    	by32locals.sampleoff	= 18;		// 751-18 game rom detected (star trek,playboy...)
    }
    else
    {
    	by32locals.sampleoff	= 32 + 18;		// 751-51 game rom detected (harlem, dolly...)
    }
    logerror("%04x: by_ctrl32_int %02x \n", activecpu_get_previouspc(),w );
}


/*----------------------------------------
/            Sounds Plus -51
/            Sounds Plus -56 & Vocalizer -57
/-----------------------------------------*/
/*
/  U3  CPU 6802/6808
/      3.58MHz
/
/  U4  ROM f000-ffff (8000-8fff)
/U1-U8 ROM 8000-ffff (vocalizer board)
/  U10 RAM 0000-007f
/  U2  PIA 0080-0083 (PIA0)
/      A  : 8910 DA
/      B0 : 8910 BC1
/      B1 : 8910 BDIR
/      B6 : Speech clock
/      B7 : Speech data
/      CA1: SoundEnable
/      CB1: fed by 555 timer (not equipped?)
/      CA2: ? (volume circuit)
/      CB2: ? (volume circuit)
/      IRQ: CPU IRQ
/  U1  AY-3-8910
/      IOA0-IOA4 = ~SoundA-E
/      CLK = E
/
*/
#define SP_PIA0  2

static void sp_init(struct sndbrdData *brdData);
static void sp_diag(int button);
static WRITE_HANDLER(sp51_data_w);
static WRITE_HANDLER(sp51_ctrl_w);
static WRITE_HANDLER(sp51_manCmd_w);
static READ_HANDLER(sp_8910a_r);

/*-------------------
/ exported interface
/--------------------*/
const struct sndbrdIntf by51Intf = {
  "BY51", sp_init, NULL, sp_diag, sp51_manCmd_w, sp51_data_w, NULL, sp51_ctrl_w, NULL,
};

static struct AY8910interface   sp_ay8910Int  = { 1, 3579545/4, {20}, {sp_8910a_r} };

static MEMORY_READ_START(sp51_readmem)
  { 0x0000, 0x007f, MRA_RAM },
  { 0x0080, 0x00ff, pia_r(SP_PIA0) },
  { 0x1000, 0x1fff, MRA_ROM },
  { 0xf000, 0xffff, MRA_ROM },
MEMORY_END

static MEMORY_WRITE_START(sp_writemem)
  { 0x0000, 0x007f, MWA_RAM },
  { 0x0080, 0x00ff, pia_w(SP_PIA0) },
  { 0x1000, 0x1fff, MWA_ROM },
  { 0x8000, 0xffff, MWA_ROM },
MEMORY_END

MACHINE_DRIVER_START(by51)
  MDRV_CPU_ADD_TAG("scpu", M6802, 3579545./4.)
  MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
  MDRV_CPU_MEMORY(sp51_readmem, sp_writemem)
  MDRV_INTERLEAVE(500)
  MDRV_SOUND_ADD(AY8910, sp_ay8910Int)
MACHINE_DRIVER_END

static struct DACinterface sp_dacInt = { 1, { 20 }};

static MEMORY_READ_START(sp51N_readmem)
  { 0x0000, 0x007f, MRA_RAM },
  { 0x0080, 0x00ff, pia_r(SP_PIA0) },
  { 0xf000, 0xffff, MRA_ROM },
MEMORY_END

static MEMORY_WRITE_START(sp51N_writemem)
  { 0x0000, 0x007f, MWA_RAM },
  { 0x0080, 0x00ff, pia_w(SP_PIA0) },
  { 0x1000, 0x1000, DAC_0_data_w },
MEMORY_END

MACHINE_DRIVER_START(by51N)
  MDRV_IMPORT_FROM(by51)
  MDRV_CPU_MODIFY("scpu")
  MDRV_CPU_MEMORY(sp51N_readmem, sp51N_writemem)
  MDRV_SOUND_ADD(DAC, sp_dacInt)
MACHINE_DRIVER_END

static struct mc3417_interface sp_mc3417Int = { 1, {100}};

static MEMORY_READ_START(sp56_readmem)
  { 0x0000, 0x007f, MRA_RAM },
  { 0x0080, 0x00ff, pia_r(SP_PIA0) },
  { 0x8000, 0xffff, MRA_ROM },
MEMORY_END

MACHINE_DRIVER_START(by56)
  MDRV_IMPORT_FROM(by51)
  MDRV_CPU_MODIFY("scpu")
  MDRV_CPU_MEMORY(sp56_readmem, sp_writemem)
  MDRV_SOUND_ADD(MC3417, sp_mc3417Int)
MACHINE_DRIVER_END

static READ_HANDLER(sp_8910r);
static READ_HANDLER(sp_pia0b_r);
static WRITE_HANDLER(sp_pia0a_w);
static WRITE_HANDLER(sp_pia0b_w);
static WRITE_HANDLER(sp_pia0cb2_w);
static void sp_irq(int state);

static const struct pia6821_interface sp_pia = {
  /*i: A/B,CA/B1,CA/B2 */ sp_8910r, sp_pia0b_r, PIA_UNUSED_VAL(1), PIA_UNUSED_VAL(1), 0, 0,
  /*o: A/B,CA/B2       */ sp_pia0a_w, sp_pia0b_w, 0, sp_pia0cb2_w,
  /*irq: A/B           */ sp_irq, sp_irq
};

static struct {
  struct sndbrdData brdData;
  int pia0a, pia0b;
  UINT8 lastcmd, cmd[2], lastctrl;
} splocals;

static void sp_init(struct sndbrdData *brdData) {
  int i;
  splocals.brdData = *brdData;
  pia_config(SP_PIA0, PIA_STANDARD_ORDERING, &sp_pia);
  //if (splocals.brdData.subType == 1) { // -56 board
  //  mc3417_set_gain(0, 0.92);
  //}
  for (i=0; i < 0x80; i++) memory_region(BY51_CPUREGION)[i] = 0xff;
}
static void sp_diag(int button) {
  cpu_set_nmi_line(splocals.brdData.cpuNo, button ? ASSERT_LINE : CLEAR_LINE);
}
static READ_HANDLER(sp_8910r) {
  if (splocals.brdData.subType == 2) return ~splocals.lastcmd; // -51N
  if ((splocals.pia0b & 0x03) == 0x01) return AY8910Read(0);
  return 0;
}
static WRITE_HANDLER(sp_pia0a_w) {
  splocals.pia0a = data;
  if (splocals.pia0b & 0x02) AY8910Write(0, splocals.pia0b ^ 0x01, splocals.pia0a);
}
static READ_HANDLER(sp_pia0b_r) {
  return splocals.pia0b;
}
static WRITE_HANDLER(sp_pia0b_w) {
  splocals.pia0b = data;
  if (splocals.brdData.subType == 1) { // -56 board
    mc3417_digit_w(0,(data & 0x80)>0);
    mc3417_clock_w(0,(data & 0x40)>0);
  }
  if (splocals.pia0b & 0x02) AY8910Write(0, splocals.pia0b ^ 0x01, splocals.pia0a);
}
static WRITE_HANDLER(sp_pia0cb2_w) {
  logerror("Mute sound: %d\n", data);
  // spaceinv seems to use this feature at game start time but not anymore afterwards!?
  mixer_set_volume(0, data ? 75 : 100);
}

static WRITE_HANDLER(sp51_data_w) {
  if (splocals.brdData.subType == 2) { // -51N
    splocals.lastcmd = data & 0x0f;
    if ((splocals.lastctrl & 1) && data != 0x0f) {
      sp_irq(1); sp_irq(0);
    }
  } else {
    splocals.lastcmd = (splocals.lastcmd & 0x10) | (data & 0x0f);
  }
}
static WRITE_HANDLER(sp51_ctrl_w) {
  if (splocals.brdData.subType != 2) { // not -51N
    splocals.lastcmd = (splocals.lastcmd & 0x0f) | ((data & 0x02) << 3);
  }
  pia_set_input_ca1(SP_PIA0, data & 0x01);
  splocals.lastctrl = data;
}
static WRITE_HANDLER(sp51_manCmd_w) {
  splocals.lastcmd = data;  pia_set_input_ca1(SP_PIA0, 1); pia_set_input_ca1(SP_PIA0, 0);
  if (splocals.brdData.subType == 2) { // -51N
    sp_irq(1); sp_irq(0);
  }
}

static READ_HANDLER(sp_8910a_r) {
  return (0x1f & ~splocals.lastcmd) | 0x20;
}

static void sp_irq(int state) {
  cpu_set_irq_line(splocals.brdData.cpuNo, M6802_IRQ_LINE, state ? ASSERT_LINE : CLEAR_LINE);
}

/*----------------------------------------
/          Squawk n Talk -61
/-----------------------------------------*/
/*
/  RAM U6 0000-007f
/  ROM U2 e000-e7ff (c000-Cfff)
/  ROM U3 e800-efff (d000-dfff)
/  ROM U4 f000-f7ff (e000-efff)
/  ROM U5 f800-ffff (f000-ffff)
/
/  0800  DAC (J-A)
/  1000  DAC (J-C)
/
/ PIA0
/  A: 0-7 AY8912 Data
/     0-4 Sound cmd (J-EE)
/
/  B: 0   AY8912 BC1
/     1   AY8912 R/W
/     2   Vocalizer Clock
/     3   Vocalizer Data
/     4-7 Sound volume
/ CA1:    NC
/ CA2:    Self-test LED (+5V)
/ CB1:    Sound interrupt (assume it starts high)
/ CB2:	  ?
/ IRQA, IRQB: CPU IRQ
/
/ PIA1: 0090
/  A: 0-7 TMS5200 D7-D0
/  B: 0   TMS5200 ReadStrobe
/     1   TMS5200 WriteStrobe
/     2-3 J38
/     4-7 Speech volume
/ CA1:    NC
/ CA2:    TMS5200 Ready
/ CB1:    TMS5200 Int
/ CB2:    NC
/ IRQA, IRQB: CPU IRQ
*/
#define SNT_PIA0 2
#define SNT_PIA1 3
#define SNT2_PIA0 4
#define SNT2_PIA1 5

static void snt_init(struct sndbrdData *brdData);
static void snt_diag(int button);
static WRITE_HANDLER(snt_data_w);
static WRITE_HANDLER(snt_ctrl_w);
static WRITE_HANDLER(snt_manCmd_w);
static void snt_5220Irq(int state);
static void snt_5220Rdy(int state);
static READ_HANDLER(snt_8910a_r);

const struct sndbrdIntf by61Intf = {
  "BYSNT", snt_init, NULL, snt_diag, snt_manCmd_w, snt_data_w, NULL, snt_ctrl_w, NULL, 0//SNDBRD_NODATASYNC|SNDBRD_NOCTRLSYNC
};
static struct TMS5220interface snt_tms5220Int = { 640000, 75, snt_5220Irq, snt_5220Rdy };
static struct DACinterface     snt_dacInt = { 1, { 20 }};
static struct AY8910interface  snt_ay8910Int = { 1, 3579545/4, {25}, {snt_8910a_r}};

static MEMORY_READ_START(snt_readmem)
  { 0x0000, 0x007f, MRA_RAM },
  { 0x0080, 0x0083, pia_r(SNT_PIA0) },
  { 0x0090, 0x0093, pia_r(SNT_PIA1) },
  { 0x1000, 0x1000, MRA_NOP },
  { 0xc000, 0xffff, MRA_ROM },
MEMORY_END

static MEMORY_WRITE_START(snt_writemem)
  { 0x0000, 0x007f, MWA_RAM },
  { 0x0080, 0x0083, pia_w(SNT_PIA0) },
  { 0x0090, 0x0093, pia_w(SNT_PIA1) },
  { 0x1000, 0x1000, DAC_0_data_w },
  { 0xc000, 0xffff, MWA_ROM },
MEMORY_END

MACHINE_DRIVER_START(by61)
  MDRV_CPU_ADD(M6802, 3579545./4.)
  MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
  MDRV_CPU_MEMORY(snt_readmem, snt_writemem)
  MDRV_INTERLEAVE(500)
  MDRV_SOUND_ADD(TMS5220, snt_tms5220Int)
  MDRV_SOUND_ADD(DAC,     snt_dacInt)
  MDRV_SOUND_ADD(AY8910,  snt_ay8910Int)
MACHINE_DRIVER_END

static struct DACinterface     snt_dacInt2 = { 2, { 20, 20 }};

static MEMORY_READ_START(snt_readmem2)
  { 0x0000, 0x007f, MRA_RAM },
  { 0x0080, 0x0083, pia_r(SNT2_PIA0) },
  { 0x0090, 0x0093, pia_r(SNT2_PIA1) },
  { 0x1000, 0x1000, MRA_NOP },
  { 0xc000, 0xffff, MRA_ROM },
MEMORY_END

static MEMORY_WRITE_START(snt_writemem2)
  { 0x0000, 0x007f, MWA_RAM },
  { 0x0080, 0x0083, pia_w(SNT2_PIA0) },
  { 0x0090, 0x0093, pia_w(SNT2_PIA1) },
  { 0x1000, 0x1000, DAC_1_data_w },
  { 0xc000, 0xffff, MWA_ROM },
MEMORY_END

MACHINE_DRIVER_START(by61x2)
  MDRV_CPU_ADD(M6802, 3579545./4.)
  MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
  MDRV_CPU_MEMORY(snt_readmem, snt_writemem)

  MDRV_CPU_ADD(M6802, 3579545./4.)
  MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
  MDRV_CPU_MEMORY(snt_readmem2, snt_writemem2)
  MDRV_INTERLEAVE(500)

  MDRV_SOUND_ADD(TMS5220, snt_tms5220Int)
  MDRV_SOUND_ADD(DAC,     snt_dacInt2)
  MDRV_SOUND_ADD(AY8910,  snt_ay8910Int)
MACHINE_DRIVER_END

static READ_HANDLER(snt_pia0a_r);
static READ_HANDLER(snt_pia0b_r);
static WRITE_HANDLER(snt_pia0a_w);
static WRITE_HANDLER(snt_pia0b_w);
static WRITE_HANDLER(snt_pia0ca2_w);

static READ_HANDLER(snt_pia1a_r);
static READ_HANDLER(snt_pia1ca2_r);
static READ_HANDLER(snt_pia1cb1_r);
static WRITE_HANDLER(snt_pia1a_w);
static WRITE_HANDLER(snt_pia1b_w);

static READ_HANDLER(snt2_pia0a_r);
static WRITE_HANDLER(snt2_pia0ca2_w);

static void snt_irq(int state);
static void snt2_irq(int state);

static struct {
  struct sndbrdData brdData;
  int pia0a, pia0b, pia1a, pia1b, pia1cb1, pia1ca2;
  UINT8 cmd[2], lastcmd, lastctrl;
} sntlocals;

static const struct pia6821_interface snt_pia[] = {{
  /*i: A/B,CA/B1,CA/B2 */ snt_pia0a_r, snt_pia0b_r, PIA_UNUSED_VAL(1), PIA_UNUSED_VAL(1), 0, PIA_UNUSED_VAL(0),
  /*o: A/B,CA/B2       */ snt_pia0a_w, snt_pia0b_w, snt_pia0ca2_w, 0,
  /*irq: A/B           */ snt_irq, snt_irq
},{
  /*i: A/B,CA/B1,CA/B2 */ snt_pia1a_r, 0, PIA_UNUSED_VAL(1), snt_pia1cb1_r, snt_pia1ca2_r, PIA_UNUSED_VAL(0),
  /*o: A/B,CA/B2       */ snt_pia1a_w, snt_pia1b_w, 0, 0,
  /*irq: A/B           */ snt_irq, snt_irq
},{
  /*i: A/B,CA/B1,CA/B2 */ snt2_pia0a_r, 0, PIA_UNUSED_VAL(1), PIA_UNUSED_VAL(1), 0, PIA_UNUSED_VAL(0),
  /*o: A/B,CA/B2       */ 0, 0, snt2_pia0ca2_w, 0,
  /*irq: A/B           */ snt2_irq, snt2_irq
},{
  /*i: A/B,CA/B1,CA/B2 */ 0, 0, PIA_UNUSED_VAL(1), PIA_UNUSED_VAL(1), 0, PIA_UNUSED_VAL(0),
  /*o: A/B,CA/B2       */ 0, 0, 0, 0,
  /*irq: A/B           */ snt2_irq, snt2_irq
}};

static void snt_init(struct sndbrdData *brdData) {
  int i;
  sntlocals.brdData = *brdData;
  pia_config(SNT_PIA0, PIA_STANDARD_ORDERING, &snt_pia[0]);
  pia_config(SNT_PIA1, PIA_STANDARD_ORDERING, &snt_pia[1]);
  if (brdData->subType == 2) { // Mysterian, uses a 2nd board without any AY8910 or TMS speech chip, just the DAC is used
    pia_config(SNT2_PIA0, PIA_STANDARD_ORDERING, &snt_pia[2]);
    pia_config(SNT2_PIA1, PIA_STANDARD_ORDERING, &snt_pia[3]);
  }
  tms5220_reset();
  tms5220_set_variant(TMS5220_IS_5200);
  for (i=0; i < 0x80; i++) memory_region(BY61_CPUREGION)[i] = 0xff;
  if (core_gameData->hw.gameSpecific1 & BY35GD_REVERB) {
    tms5220_set_reverb_filter(0.25f, (float)core_getDip(4) * 0.05f);
    AY8910_set_reverb_filter(0, 0.25f, (float)core_getDip(4) * 0.05f);
    DAC_set_reverb_filter(0, 0.25f, (float)core_getDip(4) * 0.05f);
  }
}
static void snt_diag(int button) {
  cpu_set_nmi_line(sntlocals.brdData.cpuNo, button ? ASSERT_LINE : CLEAR_LINE);
}

static READ_HANDLER(snt_pia0a_r) {
  if (sntlocals.brdData.subType) return snt_8910a_r(0); // -61B
  if ((sntlocals.pia0b & 0x03) == 0x01) return AY8910Read(0);
  return 0;
}
static READ_HANDLER(snt_pia0b_r) {
  return sntlocals.pia0b;
}
static WRITE_HANDLER(snt_pia0a_w) {
  sntlocals.pia0a = data;
  if (sntlocals.pia0b & 0x02) AY8910Write(0, sntlocals.pia0b ^ 0x01, sntlocals.pia0a);
}
static WRITE_HANDLER(snt_pia0b_w) {
  sntlocals.pia0b = data;
  if (sntlocals.pia0b & 0x02) AY8910Write(0, sntlocals.pia0b ^ 0x01, sntlocals.pia0a);
}
static READ_HANDLER(snt_pia1a_r) { return sntlocals.pia1a; }
static WRITE_HANDLER(snt_pia1a_w) { sntlocals.pia1a = data; }
static WRITE_HANDLER(snt_pia1b_w) {
  if (sntlocals.pia1b & ~data & 0x01) { // read, overrides write command!
    sntlocals.pia1a = tms5220_status_r(0);
  } else if (sntlocals.pia1b & ~data & 0x02) { // write
    tms5220_data_w(0, sntlocals.pia1a);
  }
  sntlocals.pia1b = data;
  pia_set_input_ca2(SNT_PIA1, tms5220_ready_r());
}
static READ_HANDLER(snt_pia1ca2_r) {
  return sntlocals.pia1ca2;
}
static READ_HANDLER(snt_pia1cb1_r) {
  return sntlocals.pia1cb1;
}

static READ_HANDLER(snt2_pia0a_r) {
  return snt_8910a_r(0);
}

static WRITE_HANDLER(snt_data_w) {
  sntlocals.lastcmd = (sntlocals.lastcmd & 0x10) | (data & 0x0f);
}
static WRITE_HANDLER(snt_ctrl_w) {
  sntlocals.lastcmd = (sntlocals.lastcmd & 0x0f) | ((data & 0x02) ? 0x10 : 0x00);
  pia_set_input_cb1(SNT_PIA0, ~data & 0x01);
  if (sntlocals.brdData.subType == 2) {
    pia_set_input_cb1(SNT2_PIA0, ~data & 0x01);
  }
}

static int manualSoundcmd = 0; // only for sound command mode
static WRITE_HANDLER(snt_manCmd_w) {
  manualSoundcmd = 1;
  sntlocals.lastcmd = data;
  pia_set_input_cb1(SNT_PIA0, 1); pia_set_input_cb1(SNT_PIA0, 0);
  if (sntlocals.brdData.subType == 2) {
    pia_set_input_cb1(SNT2_PIA0, 1); pia_set_input_cb1(SNT2_PIA0, 0);
  }
}

static READ_HANDLER(snt_8910a_r) {
  if (!manualSoundcmd)
    return ~sntlocals.lastcmd;
  else // S&T needs special handling for sound command mode
  {
    static UINT8 first = 1;
    if (first) // first nibble/least significant 4-bits
    {
      const UINT8 cmd = sntlocals.lastcmd & 0x0f;
      first = 0;
      return ~cmd;
    }
    else // second nibble/most significant 4-bits
    {
      const UINT8 cmd = sntlocals.lastcmd >> 4;
      first = 1;
      manualSoundcmd = 0; // manual soundcommand finished
      return ~cmd;
    }
  }
}

static WRITE_HANDLER(snt_pia0ca2_w) { sndbrd_ctrl_cb(sntlocals.brdData.boardNo,data); } // diag led
static WRITE_HANDLER(snt2_pia0ca2_w) { sndbrd_ctrl_cb(sntlocals.brdData.boardNo, data << 1); } // diag led of 2nd board

static void snt_irq(int state) {
  cpu_set_irq_line(sntlocals.brdData.cpuNo, M6802_IRQ_LINE, state ? ASSERT_LINE : CLEAR_LINE);
}
static void snt2_irq(int state) {
  cpu_set_irq_line(sntlocals.brdData.cpuNo + 1, M6802_IRQ_LINE, state ? ASSERT_LINE : CLEAR_LINE);
}

static void snt_5220Irq(int state) { pia_set_input_cb1(SNT_PIA1, (sntlocals.pia1cb1 = !state)); }
static void snt_5220Rdy(int state) { pia_set_input_ca2(SNT_PIA1, (sntlocals.pia1ca2 = state)); }

/*----------------------------------------
/    Cheap Squeak  -45
/-----------------------------------------*/
static void cs_init(struct sndbrdData *brdData);
static void cs_diag(int button);
static WRITE_HANDLER(cs_cmd_w);
static WRITE_HANDLER(cs_ctrl_w);
static READ_HANDLER(cs_port2_r);
static WRITE_HANDLER(cs_port2_w);

const struct sndbrdIntf by45Intf = {
  "BY45", cs_init, NULL, cs_diag, NULL, cs_cmd_w, NULL, cs_ctrl_w, NULL, 0
};
static struct DACinterface cs_dacInt = { 1, { 20 }};
static MEMORY_READ_START(cs_readmem)
  { 0x0000, 0x001f, m6803_internal_registers_r },
  { 0x0080, 0x00ff, MRA_RAM },	/*Internal RAM*/
  { 0x8000, 0xffff, MRA_ROM },
MEMORY_END
static MEMORY_WRITE_START(cs_writemem)
  { 0x0000, 0x001f, m6803_internal_registers_w },
  { 0x0080, 0x00ff, MWA_RAM },	/*Internal RAM*/
  { 0x8000, 0xffff, MWA_NOP },
MEMORY_END
static PORT_READ_START(cs_readport)
  { M6803_PORT2, M6803_PORT2, cs_port2_r },
PORT_END
static PORT_WRITE_START(cs_writeport)
  { M6803_PORT1, M6803_PORT1, DAC_0_data_w },
  { M6803_PORT2, M6803_PORT2, cs_port2_w },
PORT_END

MACHINE_DRIVER_START(by45)
  MDRV_CPU_ADD(M6803, 3579545./4.)
  MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
  MDRV_CPU_MEMORY(cs_readmem, cs_writemem)
  MDRV_CPU_PORTS(cs_readport, cs_writeport)
  MDRV_INTERLEAVE(500)
  MDRV_SOUND_ADD(DAC, cs_dacInt)
MACHINE_DRIVER_END

static struct {
  struct sndbrdData brdData;
  int cmd, ctrl, p21;
} cslocals;

static void cs_init(struct sndbrdData *brdData) {
  cslocals.brdData = *brdData;
}

static void cs_diag(int button) {
  cpu_set_nmi_line(cslocals.brdData.cpuNo, button ? ASSERT_LINE : CLEAR_LINE);
}

static WRITE_HANDLER(cs_cmd_w) { cslocals.cmd = data; }
static WRITE_HANDLER(cs_ctrl_w) {
  cslocals.ctrl = ((data & 1) == cslocals.brdData.subType);
  cpu_set_irq_line(cslocals.brdData.cpuNo, M6803_TIN_LINE, (data & 1) ? ASSERT_LINE : CLEAR_LINE);
}

void by45snd_reset(void)
{
	cslocals.p21 = 1;
}

void by45_p21_w(int data)
{
	cslocals.p21 = 0;
}

static READ_HANDLER(cs_port2_r) {
	//static int last = 0xff;
	int data = cslocals.ctrl | (cslocals.cmd << 1);
	if (cslocals.p21) data |= 0x02;
#if 0
	if(last != data)
		printf("cs_port2_r = %x\n",data);
	last = data;
#endif
	return data;
}

static WRITE_HANDLER(cs_port2_w) {
	//printf("MPU: port write = %x\n",data);
	sndbrd_ctrl_cb(cslocals.brdData.boardNo,data & 0x01);
} // diag led

/*----------------------------------------
/    Turbo Cheap Squeak
/-----------------------------------------*/
#define TCS_PIA0  2
static void tcs_init(struct sndbrdData *brdData);
static void tcs_diag(int button);
static WRITE_HANDLER(tcs_cmd_w);
static WRITE_HANDLER(tcs_ctrl_w);
static READ_HANDLER(tcs_status_r);

const struct sndbrdIntf byTCSIntf = {
  "BYTCS", tcs_init, NULL, tcs_diag, NULL, tcs_cmd_w, tcs_status_r, tcs_ctrl_w, NULL, SNDBRD_NOCBSYNC
};
static struct DACinterface tcs_dacInt = { 1, { 20 }};
static MEMORY_READ_START(tcs_readmem)
  { 0x0000, 0x1fff, MRA_RAM },
  { 0x6000, 0x6003, pia_r(TCS_PIA0) },
  { 0x8000, 0xffff, MRA_ROM },
MEMORY_END
static MEMORY_WRITE_START(tcs_writemem)
  { 0x0000, 0x1fff, MWA_RAM },
  { 0x6000, 0x6003, pia_w(TCS_PIA0) },
  { 0x8000, 0xffff, MWA_ROM },
MEMORY_END
static MEMORY_READ_START(tcs2_readmem)
  { 0x0000, 0x07ff, MRA_RAM },
  { 0x0800, 0x0803, pia_r(TCS_PIA0) },
  { 0x0c00, 0xffff, MRA_ROM },
MEMORY_END
static MEMORY_WRITE_START(tcs2_writemem)
  { 0x0000, 0x07ff, MWA_RAM },
  { 0x0800, 0x0803, pia_w(TCS_PIA0) },
  { 0x0c00, 0xffff, MWA_ROM },
MEMORY_END

MACHINE_DRIVER_START(byTCS)
  MDRV_CPU_ADD_TAG("scpu", M6809, 2000000)
  MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
  MDRV_CPU_MEMORY(tcs_readmem, tcs_writemem)
  MDRV_INTERLEAVE(500)
  MDRV_SOUND_ADD(DAC, tcs_dacInt)
MACHINE_DRIVER_END

MACHINE_DRIVER_START(byTCS2)
  MDRV_IMPORT_FROM(byTCS)
  MDRV_CPU_MODIFY("scpu")
  MDRV_CPU_MEMORY(tcs2_readmem, tcs2_writemem)
MACHINE_DRIVER_END

static WRITE_HANDLER(tcs_pia0cb2_w);
static READ_HANDLER(tcs_pia0b_r);
static WRITE_HANDLER(tcs_pia0a_w);
static WRITE_HANDLER(tcs_pia0b_w);
static void tcs_pia0irq(int state);

static struct {
  struct sndbrdData brdData;
  int cmd, dacdata, status;
} tcslocals;
static const struct pia6821_interface tcs_pia = {
  /*i: A/B,CA/B1,CA/B2 */ 0, tcs_pia0b_r, PIA_UNUSED_VAL(1), PIA_UNUSED_VAL(1), 0, 0,
  /*o: A/B,CA/B2       */ tcs_pia0a_w, tcs_pia0b_w, 0, tcs_pia0cb2_w,
  /*irq: A/B           */ tcs_pia0irq, tcs_pia0irq
};
static void tcs_init(struct sndbrdData *brdData) {
  tcslocals.brdData = *brdData;
  pia_config(TCS_PIA0, PIA_ALTERNATE_ORDERING, &tcs_pia);
}
static WRITE_HANDLER(tcs_pia0cb2_w) { sndbrd_ctrl_cb(tcslocals.brdData.boardNo,data); }
static void tcs_diag(int button) { cpu_set_nmi_line(tcslocals.brdData.cpuNo, button ? ASSERT_LINE : CLEAR_LINE); }
static WRITE_HANDLER(tcs_cmd_w) { tcslocals.cmd = data; }
static WRITE_HANDLER(tcs_ctrl_w) { pia_set_input_ca1(TCS_PIA0, data & 0x01); }
static READ_HANDLER(tcs_status_r) { return tcslocals.status; }
static READ_HANDLER(tcs_pia0b_r) {
  int ret = tcslocals.cmd & 0x0f;
  tcslocals.cmd >>= 4;
  return ret;
}
static WRITE_HANDLER(tcs_pia0a_w) {
  tcslocals.dacdata = (tcslocals.dacdata & ~0x3fc) | (data << 2);
  DAC_signed_data_16_w(0, tcslocals.dacdata << 6);
}
static WRITE_HANDLER(tcs_pia0b_w) {
  tcslocals.dacdata = (tcslocals.dacdata & ~0x003) | (data >> 6);
  DAC_signed_data_16_w(0, tcslocals.dacdata << 6);
  tcslocals.status = (data>>4) & 0x03;
}
static void tcs_pia0irq(int state) {
  cpu_set_irq_line(tcslocals.brdData.cpuNo, M6809_IRQ_LINE, state ? ASSERT_LINE : CLEAR_LINE);
}

/*----------------------------------------
/    Sounds Deluxe
/-----------------------------------------*/
#define SD_PIA0 2
static void sd_init(struct sndbrdData *brdData);
static void sd_diag(int button);
static WRITE_HANDLER(sd_cmd_w);
static WRITE_HANDLER(sd_ctrl_w);
static WRITE_HANDLER(sd_man_w);
static READ_HANDLER(sd_status_r);

const struct sndbrdIntf bySDIntf = {
  "BYSD", sd_init, NULL, sd_diag, sd_man_w, sd_cmd_w, sd_status_r, sd_ctrl_w, NULL, 0//SNDBRD_NODATASYNC|SNDBRD_NOCBSYNC
};
static struct DACinterface sd_dacInt = { 1, { 80 }};
static MEMORY_READ16_START(sd_readmem)
  {0x00000000, 0x0003ffff, MRA16_ROM},		/*ROM (4 X 64K)*/
  {0x00060000, 0x00060007, pia_msb_r(SD_PIA0) },	/*PIA - CPU D8-15 connected to PIA D0-7*/
  {0x00070000, 0x00070fff, MRA16_RAM},		/*RAM*/
MEMORY_END
static MEMORY_WRITE16_START(sd_writemem)
  {0x00000000, 0x0003ffff, MWA16_ROM},		/*ROM (4 X 64K)*/
  {0x00060000, 0x00060007, pia_msb_w(SD_PIA0)},	/*PIA - CPU D8-15 connected to PIA D0-7*/
  {0x00070000, 0x00070fff, MWA16_RAM},		/*RAM*/
MEMORY_END

MACHINE_DRIVER_START(bySD)
  MDRV_CPU_ADD(M68000, 8000000)
  MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
  MDRV_CPU_MEMORY(sd_readmem, sd_writemem)
  MDRV_INTERLEAVE(500)
  MDRV_SOUND_ADD(DAC, sd_dacInt)
MACHINE_DRIVER_END

static WRITE_HANDLER(sd_pia0cb2_w);
static READ_HANDLER(sd_pia0b_r);
static WRITE_HANDLER(sd_pia0a_w);
static WRITE_HANDLER(sd_pia0b_w);
static void sd_pia0irq(int state);

static struct {
  struct sndbrdData brdData;
  UINT16 dacdata;
  int status, ledcount;
  UINT8 cmd, latch;
} sdlocals;

static const struct pia6821_interface sd_pia = {
  /*i: A/B,CA/B1,CA/B2 */ 0, sd_pia0b_r, PIA_UNUSED_VAL(0), PIA_UNUSED_VAL(1), 0, 0,
  /*o: A/B,CA/B2       */ sd_pia0a_w, sd_pia0b_w, 0, sd_pia0cb2_w,
  /*irq: A/B           */ sd_pia0irq, sd_pia0irq
};
static void sd_init(struct sndbrdData *brdData) {
  sdlocals.brdData = *brdData;
  sdlocals.ledcount = 0;
  pia_config(SD_PIA0, PIA_ALTERNATE_ORDERING, &sd_pia);
}
static WRITE_HANDLER(sd_pia0cb2_w) {
  if (!data) {
    sdlocals.ledcount++;
    logerror("SD LED: %d\n", sdlocals.ledcount);
    if (core_gameData->hw.gameSpecific1) { // hack for Blackwater 100 (main CPU boots up too fast)
      if (sdlocals.ledcount == 5) // suspend main cpu, soundboard not ready yet
        cpu_set_halt_line(0, 1);
      else if (core_gameData->hw.gameSpecific1 && sdlocals.ledcount == 6) // resume main cpu, soundboard ready
      cpu_set_halt_line(0, 0);
    }
  }
  sndbrd_ctrl_cb(sdlocals.brdData.boardNo,data);
}
static void sd_diag(int button) {
  cpu_set_irq_line(sdlocals.brdData.cpuNo, MC68000_IRQ_3, button ? ASSERT_LINE : CLEAR_LINE);
}
static WRITE_HANDLER(sd_man_w) {
  sd_cmd_w(0, data);
  sd_ctrl_w(0, 0);
  sd_ctrl_w(0, 1);
  sd_cmd_w(0, data >> 4);
}
static WRITE_HANDLER(sd_cmd_w) {
  logerror("SD cmd: %02x\n", data);
  sdlocals.latch = data;
}
static WRITE_HANDLER(sd_ctrl_w) {
  logerror("SD ctrl:%d\n", data);
  if (!(data & 0x01)) sdlocals.cmd = sdlocals.latch;
  pia_set_input_ca1(SD_PIA0, data & 0x01);
}
static READ_HANDLER(sd_status_r) { return sdlocals.status; }

static READ_HANDLER(sd_pia0b_r) {
  UINT8 val = 0x30 | (sdlocals.cmd & 0x0f);
  logerror("SD read:%02x\n", val);
  sdlocals.cmd >>= 4;
  return val;
}

static WRITE_HANDLER(sd_pia0a_w) {
  sdlocals.dacdata = (sdlocals.dacdata & 0x00ff) | (data << 8);
}
static WRITE_HANDLER(sd_pia0b_w) {
  sdlocals.dacdata = (sdlocals.dacdata & 0xff00) | (data & 0xc0);
  DAC_signed_data_16_w(0, sdlocals.dacdata);
  sdlocals.status = (data>>4) & 0x03;
}
static void sd_pia0irq(int state) {
  cpu_set_irq_line(sdlocals.brdData.cpuNo, MC68000_IRQ_4, state ? ASSERT_LINE : CLEAR_LINE);
}
