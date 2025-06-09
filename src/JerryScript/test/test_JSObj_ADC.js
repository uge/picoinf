let adc26 = new ADC(26)
Log(`adc26.ReadVolts() = ${adc26.ReadVolts()}`)
Log(`adc26.ReadRaw()   = ${adc26.ReadRaw()}`)

let adc27 = new ADC(27)
Log(`adc27.ReadVolts() = ${adc27.ReadVolts()}`)
Log(`adc27.ReadRaw()   = ${adc27.ReadRaw()}`)

function SpeedTest(pin, count) {
  let adc = new ADC(pin)

  let timeStart = Date.now()

  for (let i = 0; i < count; ++i) {
    adc.ReadVolts()
  }

  let timeDiff = Date.now() - timeStart;

  Log(`${count} reads took: ${timeDiff} ms`)
}

SpeedTest(26, 50)