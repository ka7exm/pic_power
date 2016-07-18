
#ifndef INCLUDED_RLCD_H
#define INCLUDED_RLCD_H

struct lcd_pin_map {                 // This structure is overlayed
           boolean enable;           // on to an I/O port to gain
           boolean rs;               // access to the LCD pins.
           boolean unused1;          // The bits are allocated from
           boolean unused2;          // low order up.  ENABLE will
           int     data : 4;         // be pin B0.
        } lcd;



#if defined(__PCH__)
#if defined use_portb_lcd
   #byte lcd = 0xF81                   // This puts the entire structure
#else
   #byte lcd = 0xF83                   // This puts the entire structure
#endif
#else
#if defined use_portb_lcd
   #byte lcd = 6                  // on to port B (at address 6)
#else
   #byte lcd = 8                 // on to port D (at address 8)
#endif
#endif

#if defined use_portb_lcd
   #define set_tris_lcd(x) set_tris_b(x)
#else
   #define set_tris_lcd(x) set_tris_d(x)
#endif



#define lcd_type 2           // 0=5x7, 1=5x10, 2=2 lines
#define lcd_line_two 0x40    // LCD RAM address for the second line

char CONST LCD_STRING_BLANK[]            = {"                "};
char CONST LCD_STRING_MEMORY[]           = {"     MEMORY     "};
char CONST LCD_STRING_PIC_POWER_METER[]  = {"PIC  Power Meter"};
char CONST LCD_STRING_CALLSIGN[]         = {"Kanga US  V2.0.0"};
char CONST LCD_STRING_RESETTING_FLASH[]  = {"RESETTING  FLASH"};
char CONST LCD_STRING_RETRIEVING_FLASH[] = {"RETRIEVING FLASH"};

char CONST LCD_STRING_MENU_TIER_1[]      = {" ZERO   mW   TAP"};
char CONST LCD_STRING_MENU_TIER_2_SET[]  = {" min   max  both"};
char CONST LCD_STRING_MENU_TIER_2_CLEAR[]= {" min   max   clr"};
char CONST LCD_STRING_MENU_TIER_3[]      = {" calA calB  calC"};
char CONST LCD_STRING_MENU_TIER_4[]      = {" calD VSWR Volts"};
char CONST LCD_STRING_MENU_CALVIEW[]     = {" OK SET_DB ADJ_V"};
char CONST LCD_STRING_MENU_CALIBRATE[]   = {" SAVE DOWN    UP"};


byte CONST LCD_INIT_STRING[8] = {0x20 | (lcd_type << 2), 0xc, 1, 6, 0x0C};
                             // These bytes need to be sent to the LCD
                             // to start it up.


                             // The following are used for setting
                             // the I/O port direction register.

STRUCT lcd_pin_map const LCD_WRITE = {0,0,0,0,0}; // For write mode all pins are out
STRUCT lcd_pin_map const LCD_READ = {0,0,0,0,15}; // For read mode data pins are in

/* Function Prototypes */

void lcd_send_nibble( byte n );
void lcd_send_byte( byte address, byte n );
void lcd_init();
void lcd_gotoxy( byte x, byte y);
void lcd_putc( char c);

void lcd_put_long(long value);
void lcd_put_long_hex(long value);
void lcd_put_string(char *in);

#endif /* include */

