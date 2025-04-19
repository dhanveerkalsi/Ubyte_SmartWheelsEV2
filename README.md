**SmartWheels EV2 Initialization Code**  

**Peripherals covered:**  
   1. I2C  
      Scans available devices from 0x0 to 0xff
   2. CAN2.0 Standard - 250kbps  
      Transmits frames with ID 0x0 to 0xff and data from 0xff to 0x0
   3. Digital Input - 2 User buttons
   4. Digital Output - RGB LED
   5. Analog Input - Potentiometer
   6. UART - Sending Data  
      Prints ADC, Button Input, and I2C scan Data