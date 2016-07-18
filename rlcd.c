
////////////////////////////////////////////////////////////////////////////
////                             RLCD.C                                 ////
////                     Driver for LCD modules                         ////
////                                                                    ////
////  lcd_init()   Must be called before any other function.            ////
////                                                                    ////
////  lcd_putc(c)  Will display c on the next position of the LCD.      ////
////                     The following have special meaning:            ////
////                      \f  Clear display                             ////
////                      \n  Go to start of second line                ////
////                      \b  Move back one position                    ////
////                                                                    ////
////  lcd_gotoxy(x,y) Set write position on LCD (upper left is 1,1)     ////
////                                                                    ////
////                                                                    ////
////////////////////////////////////////////////////////////////////////////
//// ARS KA7EXM July 2004.                                              ////
//// Based upon code from Custom Computer Services, used by permission. ////
////////////////////////////////////////////////////////////////////////////

// As defined in the following structure the pin connection is as follows:
//     D0  enable
//     D1  rs
//     D2  rw

//     D4  D4
//     D5  D5
//     D6  D6
//     D7  D7
//
//   LCD pins D0-D3 are not used and PIC D3 is not used.

void lcd_send_nibble( byte n ) {
  lcd.data = n;
  delay_ms(2); // was 10, 5 OK, 2 seems OK but be careful. (1 does not work)
  lcd.enable = 1;
  delay_us(2);
  lcd.enable = 0;
}


void lcd_send_byte( byte address, byte n ) {
  lcd.rs = 0;
  lcd.rs = address;
  lcd.enable = 0;
  lcd_send_nibble(n >> 4);
  lcd_send_nibble(n & 0xf);
}


void lcd_init() {
    byte i;
    set_tris_lcd(LCD_WRITE);
    lcd.rs = 0;
    lcd.enable = 0;
    delay_ms(15);
    for(i=1; i<=3; ++i) {
       lcd_send_nibble(3);
       delay_ms(5);
    }
    lcd_send_nibble(2);
    delay_ms(5);
    for(i=0;i<=4;++i) {
      lcd_send_byte(0,LCD_INIT_STRING[i]);
    }
}


void lcd_gotoxy( byte x, byte y) {
   byte address;

   if(y!=1)
     address=lcd_line_two;
   else
     address=0;
   address+=x-1;
   lcd_send_byte(0,0x80|address);
}


void lcd_putc( char c) {
   switch (c) {
     case '\f'   : lcd_send_byte(0,1);
                   delay_ms(2);
                                           break;
     case '\n'   : lcd_gotoxy(1,2);        break;
     case '\b'   : lcd_send_byte(0,0x10);  break;
     default     : lcd_send_byte(1,c);     break;
   }
}




void lcd_put_long_hex(long value) {

  int i;
  char output[8];

  sprintf(output, "%4LX", value);
  for(i = 0; i < 4; i++) {
    lcd_putc(output[i]);
  }
}


void lcd_put_long(long value) {
  char output[8];
  sprintf(output, "%ld", value);
  lcd_put_string(output);
}


void lcd_put_string(char *in) {
  int i = 0;
  while(i < 16 && in[i] != '\0') {
    lcd_putc(in[i++]);
  }
}

