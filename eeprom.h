void twiinit(void);
int ee24xx_read_bytes(uint8_t eeid, uint16_t eeaddr, int len, uint8_t *buf);
int ee24xx_write_bytes(uint8_t eeid, uint16_t eeaddr, int len, uint8_t *buf);
