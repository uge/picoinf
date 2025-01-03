function StressI2C(addr) {
  let addrHex = "0x" + addr.toString(16)
  
  let i2c = new I2C(addr)
  
  if (i2c.IsAlive()) {
    Log(`${addrHex} is Alive`)
    
    for (let i = 0; i < 3000; ++i) {
      i2c.ReadReg8(0x00)
    }
  } else {
    Log(`${addrHex} is NOT Alive`)
  }
}

StressI2C(0x40)
StressI2C(0x48)
StressI2C(0x60)
StressI2C(0x61)  // Not expected alive
