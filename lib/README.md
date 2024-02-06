I want startup to be
- critical init order (sequential from 10)
- unimportant init    (50)
- unimportant shell   (80)


Timeline
- init  10
- shell 80

Log
- init  11
- shell 80

Uart
- init  12
- shell 80
- thread UART0Output   prio 7
- thread UART1Input    prio 7
- thread UART1Output   prio 7
- thread UARTUSBOutput prio 7
- thread UARTUSBInput  prio 7

PAL
- init  13
- shell 80
- json 80

Watchdog
- init 14
- shell 80

ADCInternal
- shell 80

FlashStore
- init  50
- shell 80

Evm
- init  50
- shell 80

JSONMsgRouter
- init 50
- shell 80
- json 80

PowerManager
- init 50
- shell 80

RP2040
- init 50
- shell 80

Shell
- init 50
- json 80

Format
- shell 80

I2C
- shell 80

Pin
- shell 80

Utl
- shell 80

Work
- shell 80
- thread Work prio 7
