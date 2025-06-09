let sensor = new SI7021()

Log(`sensor.GetTemperatureCelsius()    = ${sensor.GetTemperatureCelsius()}`)
Log(`sensor.GetTemperatureFahrenheit() = ${sensor.GetTemperatureFahrenheit()}`)
Log(`sensor.GetHumidityPct()           = ${sensor.GetHumidityPct()}`)